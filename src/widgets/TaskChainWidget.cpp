#include "TaskChainWidget.h"
#include "core/Toast.h"
#include "core/AppConfig.h"
#include "core/ApiService.h"
#include "core/MotionConfigManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QMenu>
#include <QAction>
#include <QListWidgetItem>
#include <QAbstractItemModel>
#include <QDrag>
#include <QMimeData>
#include <QMouseEvent>
#include <QStyle>
#include <QEventLoop>
#include <QTimer>

// ── 音频名称下拉支持 ────────────────────────────────────────────────────────

QString TaskChainWidget::audioDisplayName(const QString &fullName)
{
    // 去掉扩展名后按最后一个 '-' 截断:
    // sample-3s-20260921164924149.wav -> sample-3s
    const QString base = QFileInfo(fullName).completeBaseName();
    const int idx = base.lastIndexOf('-');
    return idx > 0 ? base.left(idx) : base;
}

QStringList TaskChainWidget::fetchAudioFiles(int timeoutMs)
{
    QStringList files;
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    ApiService::instance().listAudio([&](int code, const QJsonObject &resp) {
        if (code == 200) {
            const QJsonArray rows = resp.value("data").toArray();
            for (const QJsonValue &v : rows) {
                const QString name = QFileInfo(v.toString().trimmed()).fileName();
                if (!name.isEmpty()) files << name;
            }
        }
        loop.quit();
    });

    timer.start(timeoutMs);
    loop.exec();
    return files;
}

// 参考 RoboTaskChain 插件 — 四块面板布局
// 面板1(左): 任务列表 — 新建/加载/保存
// 面板2: 任务模块 — 新建任务/删除任务/全选/取消选中 + 任务列表
// 面板3: 组模块 — 添加组/插入组/删除组/全选组/循环/发送勾选/发送全部 + 组列表
// 面板4(右): 可拖拽标签 — 动作标签列表, 拖拽到组列表中添加动作

static const char *S_PANEL_TITLE =
    "color:#b0b8c0; font-size:13px; font-weight:bold; padding:4px 6px; "
    "background:#222831; border-radius:4px;";

static const char *S_LIST_STYLE =
    "QListWidget { background:#181d23; border:1px solid #2a3038; border-radius:4px; }"
    "QListWidget::item { color:#c0c8d0; padding:6px 10px; border-bottom:1px solid #22272d; }"
    "QListWidget::item:selected { background:#2a5d8f; color:#fff; }"
    "QListWidget::item:hover { background:rgba(42,93,143,0.3); }";

static const char *S_BTN_STYLE =
    "QPushButton { color:#c0c8d0; background:#2a313a; border:1px solid #3a4048; "
    "padding:5px 12px; border-radius:3px; font-size:12px; }"
    "QPushButton:hover { background:#323a44; color:#fff; }"
    "QPushButton:pressed { background:#222831; }"
    "QPushButton:disabled { color:#555; background:#222831; border-color:#2a3038; }";

static QPushButton *makeBtn(const QString &text) {
    auto *btn = new QPushButton(text);
    btn->setStyleSheet(S_BTN_STYLE);
    return btn;
}

static QString actionTypeFromName(const QString &name) {
    if (name == "路径导航") return "0";
    if (name == "平动") return "1";
    if (name == "转动") return "2";
    if (name == "播放音频") return "3";
    if (name == "顶升") return "4";
    if (name == "停止播放音频") return "5";
    return "0";
}

static QString actionDescFromType(const QString &type) {
    if (type == "0") return "路径导航";
    if (type == "1") return "平动";
    if (type == "2") return "转动";
    if (type == "3") return "播放音频";
    if (type == "4") return "顶升";
    if (type == "5") return "停止播放音频";
    return "路径导航";
}

// 可拖拽的列表项 — 支持拖出动作标签
class DraggableListWidget : public QListWidget
{
public:
    explicit DraggableListWidget(QWidget *parent = nullptr) : QListWidget(parent)
    {
        setDragEnabled(true);
        setDragDropMode(QAbstractItemView::DragOnly);
        setSelectionMode(QAbstractItemView::SingleSelection);
    }

protected:
    void startDrag(Qt::DropActions /*supportedActions*/) override
    {
        QListWidgetItem *item = currentItem();
        if (!item) return;
        QDrag *drag = new QDrag(this);
        QMimeData *mime = new QMimeData;
        mime->setText(QStringLiteral("action:%1").arg(item->text()));
        mime->setData("application/x-taskchain-action", item->text().toUtf8());
        drag->setMimeData(mime);
        drag->exec(Qt::CopyAction);
    }
};

// 可接收拖拽的组列表
class DropTargetListWidget : public QListWidget
{
public:
    explicit DropTargetListWidget(QWidget *parent = nullptr) : QListWidget(parent)
    {
        setAcceptDrops(true);
        setDragDropMode(QAbstractItemView::DropOnly);
        setDefaultDropAction(Qt::CopyAction);
    }

    std::function<void(const QString &actionName, int row)> onActionDropped;

protected:
    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (event->mimeData()->hasFormat("application/x-taskchain-action"))
            event->acceptProposedAction();
        else
            event->ignore();
    }

    void dragMoveEvent(QDragMoveEvent *event) override
    {
        if (event->mimeData()->hasFormat("application/x-taskchain-action"))
            event->acceptProposedAction();
        else
            event->ignore();
    }

    void dropEvent(QDropEvent *event) override
    {
        if (event->mimeData()->hasFormat("application/x-taskchain-action")) {
            QString actionName = QString::fromUtf8(event->mimeData()->data("application/x-taskchain-action"));
            // 根据鼠标位置确定拖到哪一行
            auto *dropItem = itemAt(event->position().toPoint());
            int dropRow = dropItem ? row(dropItem) : -1;
            if (onActionDropped) onActionDropped(actionName, dropRow);
            event->acceptProposedAction();
        } else {
            event->ignore();
        }
    }
};

TaskChainWidget::TaskChainWidget(QWidget *parent)
    : QWidget(parent)
{
    initActionDefs();
    setupUI();
}

void TaskChainWidget::setupUI()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // 四面板分割器
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    root->addWidget(splitter, 1);

    // ── 面板1: 任务链列表 ──
    auto *panel1 = new QWidget;
    auto *p1Layout = new QVBoxLayout(panel1);
    p1Layout->setContentsMargins(6, 6, 6, 6);
    p1Layout->setSpacing(6);
    setupPanel1Chain(p1Layout);
    splitter->addWidget(panel1);

    // ── 面板2: 任务模块 (缩窄50%) ──
    auto *panel2 = new QWidget;
    auto *p2Layout = new QVBoxLayout(panel2);
    p2Layout->setContentsMargins(6, 6, 6, 6);
    p2Layout->setSpacing(6);
    setupPanel2Task(p2Layout);
    splitter->addWidget(panel2);

    // ── 面板3: 组模块 ──
    auto *panel3 = new QWidget;
    auto *p3Layout = new QVBoxLayout(panel3);
    p3Layout->setContentsMargins(6, 6, 6, 6);
    p3Layout->setSpacing(6);
    setupPanel3Group(p3Layout);
    splitter->addWidget(panel3);

    // ── 面板4: 可拖拽标签 ──
    auto *panel4 = new QWidget;
    auto *p4Layout = new QVBoxLayout(panel4);
    p4Layout->setContentsMargins(6, 6, 6, 6);
    p4Layout->setSpacing(6);
    setupPanel4DragDrop(p4Layout);
    splitter->addWidget(panel4);

    // 任务面板缩窄50%, 空间均分给组和标签
    // 之前: {220, 320, 320, 200}  total=1060  task=320
    // 现在: task=160, 释放160均分给group和tag  => {220, 160, 400, 280}
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);   // 缩窄
    splitter->setStretchFactor(2, 3);   // 加宽
    splitter->setStretchFactor(3, 2);   // 加宽
    splitter->setSizes({200, 160, 400, 280});

    // 初始化可拖拽动作标签
    refreshActionTags();

    // 底部公共状态栏
    setupBottomBar(root);

    updateBottomBar();

    if (m_chains.isEmpty()) {
        onChainNew();
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// 面板1: 任务链列表
// ═══════════════════════════════════════════════════════════════════════════════

void TaskChainWidget::setupPanel1Chain(QVBoxLayout *layout)
{
    auto *title = new QLabel(QStringLiteral("  任务链列表"));
    title->setStyleSheet(S_PANEL_TITLE);
    layout->addWidget(title);

    // 按钮行在列表上方
    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(6);
    m_chainNewBtn  = makeBtn(QStringLiteral("新建"));
    m_chainLoadBtn = makeBtn(QStringLiteral("加载"));
    m_chainSaveBtn = makeBtn(QStringLiteral("保存"));
    btnRow->addWidget(m_chainNewBtn);
    btnRow->addWidget(m_chainLoadBtn);
    btnRow->addWidget(m_chainSaveBtn);
    btnRow->addStretch();
    layout->addLayout(btnRow);

    m_chainList = new QListWidget;
    m_chainList->setStyleSheet(S_LIST_STYLE);
    layout->addWidget(m_chainList, 1);

    m_chainStatus = new QLabel;
    m_chainStatus->setStyleSheet("color:#666; font-size:10px; padding-top:4px;");
    layout->addWidget(m_chainStatus);

    // 连接
    connect(m_chainNewBtn,  &QPushButton::clicked, this, &TaskChainWidget::onChainNew);
    connect(m_chainLoadBtn, &QPushButton::clicked, this, &TaskChainWidget::onChainLoad);
    connect(m_chainSaveBtn, &QPushButton::clicked, this, &TaskChainWidget::onChainSave);
    connect(m_chainList, &QListWidget::currentItemChanged,
            this, &TaskChainWidget::onChainItemChanged);
}

// ══════════════════════════════════════════════════════════════════════════════
// 面板2: 任务模块
// ═══════════════════════════════════════════════════════════════════════════════

void TaskChainWidget::setupPanel2Task(QVBoxLayout *layout)
{
    auto *title = new QLabel(QStringLiteral("  任务"));
    title->setStyleSheet(S_PANEL_TITLE);
    layout->addWidget(title);

    // 按钮行
    auto *btnRow1 = new QHBoxLayout;
    btnRow1->setSpacing(6);
    m_taskNewBtn      = makeBtn(QStringLiteral("新建任务"));
    m_taskDeleteBtn    = makeBtn(QStringLiteral("删除任务"));
    btnRow1->addWidget(m_taskNewBtn);
    btnRow1->addWidget(m_taskDeleteBtn);
    btnRow1->addStretch();
    layout->addLayout(btnRow1);

    auto *btnRow2 = new QHBoxLayout;
    btnRow2->setSpacing(6);
    btnRow2->addStretch();
    layout->addLayout(btnRow2);

    // 任务列表 (带勾选 + 拖拽排序)
    m_taskList = new QListWidget;
    m_taskList->setStyleSheet(S_LIST_STYLE);
    m_taskList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_taskList->setDragDropMode(QAbstractItemView::InternalMove);
    m_taskList->setDefaultDropAction(Qt::MoveAction);
    layout->addWidget(m_taskList, 1);

    m_taskCountLabel = new QLabel(QStringLiteral("共 [0] 项"));
    m_taskCountLabel->setStyleSheet("color:#777; font-size:11px; padding:2px;");
    layout->addWidget(m_taskCountLabel);

    // 全选任务按钮在面板下方
    m_taskSelectAllBtn = makeBtn(QStringLiteral("全选任务"));
    layout->addWidget(m_taskSelectAllBtn);

    // 连接
    connect(m_taskNewBtn,       &QPushButton::clicked, this, &TaskChainWidget::onTaskNew);
    connect(m_taskDeleteBtn,     &QPushButton::clicked, this, &TaskChainWidget::onTaskDelete);
   connect(m_taskSelectAllBtn, &QPushButton::clicked, this, &TaskChainWidget::onTaskSelectAll);
    // 拖拽排序后重新编号
    connect(m_taskList->model(), &QAbstractItemModel::rowsMoved,
            this, [this]() {
        int idx = currentChainIndex();
        if (idx < 0) return;
        QJsonArray reordered;
        for (int i = 0; i < m_taskList->count(); ++i) {
            int origIdx = m_taskList->item(i)->data(Qt::UserRole).toInt();
            if (origIdx >= 0 && origIdx < m_chains[idx].tasks.size())
                reordered.append(m_chains[idx].tasks[origIdx]);
        }
        m_chains[idx].tasks = reordered;
        refreshTaskList();
    });
    // 选择变化时更新底部状态栏 + 刷新组列表
    connect(m_taskList, &QListWidget::currentItemChanged,
            this, [this](QListWidgetItem *current, QListWidgetItem*) {
        if (current) {
            m_currentTaskIndex = current->data(Qt::UserRole).toInt();
        } else {
            m_currentTaskIndex = -1;
        }
        refreshGroupList();
        updateBottomBar();
        updateTaskSelectBtn();  // 更新全选/取消按钮文字
    });
}

// ═══════════════════════════════════════════════════════════════════════════════
// 面板3: 组模块
// ═══════════════════════════════════════════════════════════════════════════════

void TaskChainWidget::setupPanel3Group(QVBoxLayout *layout)
{
    auto *title = new QLabel(QStringLiteral("  组"));
    title->setStyleSheet(S_PANEL_TITLE);
    layout->addWidget(title);

    // 按钮行
    auto *btnRow1 = new QHBoxLayout;
    btnRow1->setSpacing(6);
    m_groupAddBtn      = makeBtn(QStringLiteral("添加组"));
    m_groupInsertBtn   = makeBtn(QStringLiteral("插入组"));
    m_groupDeleteBtn    = makeBtn(QStringLiteral("删除选中"));
    m_groupSelectAllBtn = makeBtn(QStringLiteral("全选组"));
    btnRow1->addWidget(m_groupAddBtn);
    btnRow1->addWidget(m_groupInsertBtn);
    btnRow1->addWidget(m_groupDeleteBtn);
    btnRow1->addWidget(m_groupSelectAllBtn);
    btnRow1->addStretch();
    layout->addLayout(btnRow1);

    // 组列表 (可接收拖拽)
    auto *dropList = new DropTargetListWidget;
    dropList->setStyleSheet(S_LIST_STYLE);
    m_groupList = dropList;
    dropList->onActionDropped = [this](const QString &actionName, int groupIdx) {
        if (m_currentTaskIndex < 0) {
            QMessageBox::warning(this, QStringLiteral("提示"),
                QStringLiteral("请先选择一个任务"));
            return;
        }

        QJsonArray groups = currentTaskGroups();

        // 如果没有组, 自动创建一个
        if (groups.isEmpty()) {
            QJsonObject newGroup;
            newGroup["name"] = QStringLiteral("group1");
            newGroup["key"] = "1";
            groups.append(newGroup);
            setCurrentTaskGroups(groups);
        }

        // 如果拖拽位置无效, 放到最后一个组
        if (groupIdx < 0 || groupIdx >= groups.size())
            groupIdx = groups.size() - 1;

        QJsonObject group = groups[groupIdx].toObject();
        QString groupActionType = group.value("actionType").toString();

        // 如果组还没有标签类型, 设置为当前拖入的标签类型
        if (groupActionType.isEmpty()) {
            group["actionType"] = actionName;
        } else if (groupActionType != actionName) {
            // 每个组只能添加一种标签
            QMessageBox::warning(this, QStringLiteral("提示"),
                QStringLiteral("每个组只能添加一种标签，当前组已设置为 [%1]").arg(groupActionType));
            return;
        }

        // 添加动作到组 (初始化规范参数)
        QJsonArray actions = group.value("actions").toArray();
        QJsonObject action;
        action["name"]        = actionName;
        action["action_desc"] = actionName;
        action["action_type"] = actionTypeFromName(actionName);

        QJsonObject params;
        if (actionName == "路径导航") {
            QString stName = !m_stations.isEmpty() ? m_stations[0].name : "LM3";
            action["target_station"] = stName;
            params["target_station"] = stName;
            params["target"]         = stName;
        } else if (actionName == "平动") {
            action["direction"] = "前进";
            action["distance"]  = 1.0;
            action["speed"]     = 0.2;
            params["direction"] = "前进";
            params["distance"]  = 1.0;
            params["speed"]     = 0.2;
        } else if (actionName == "转动") {
            action["direction"] = "顺时针(右转)";
            action["angle"]     = 90.0;
            action["speed"]     = 0.5;
            params["direction"] = "顺时针(右转)";
            params["angle"]     = 90.0;
            params["speed"]     = 0.5;
        }
        action["params"] = params;
        actions.append(action);
        group["actions"] = actions;
        groups[groupIdx] = group;
        setCurrentTaskGroups(groups);

        refreshGroupList();
        updateStatus(QStringLiteral("已添加动作: %1").arg(actionName));
    };
    layout->addWidget(m_groupList, 1);

    // 循环 + 发送按钮
    auto *btnRow2 = new QHBoxLayout;
    btnRow2->setSpacing(6);
    m_groupLoopCheck = new QCheckBox(QStringLiteral("循环"));
    m_groupLoopCheck->setStyleSheet("color:#c0c8d0; font-size:12px;");
    btnRow2->addWidget(m_groupLoopCheck);
    btnRow2->addStretch();
    m_groupSendSelBtn = makeBtn(QStringLiteral("发送勾选的任务"));
    m_groupSendAllBtn = makeBtn(QStringLiteral("发送全部任务"));
    btnRow2->addWidget(m_groupSendSelBtn);
    btnRow2->addWidget(m_groupSendAllBtn);
    layout->addLayout(btnRow2);

    m_groupHintLabel = new QLabel(QStringLiteral("拖拽标签到组中, 每组只能添加一种标签"));
    m_groupHintLabel->setStyleSheet("color:#666; font-size:11px; padding:2px;");
    layout->addWidget(m_groupHintLabel);

    // 连接
    connect(m_groupAddBtn,       &QPushButton::clicked, this, &TaskChainWidget::onGroupAdd);
    connect(m_groupInsertBtn,    &QPushButton::clicked, this, &TaskChainWidget::onGroupInsert);
    connect(m_groupDeleteBtn,     &QPushButton::clicked, this, &TaskChainWidget::onGroupDelete);
    connect(m_groupSelectAllBtn, &QPushButton::clicked, this, &TaskChainWidget::onGroupSelectAll);
    connect(m_groupLoopCheck,  &QCheckBox::toggled, this, &TaskChainWidget::onGroupLoopToggled);
    connect(m_groupSendSelBtn, &QPushButton::clicked, this, &TaskChainWidget::onGroupSendSelected);
    connect(m_groupSendAllBtn,  &QPushButton::clicked, this, &TaskChainWidget::onGroupSendAll);
    // 选择变化时更新底部状态栏
    connect(m_groupList, &QListWidget::currentItemChanged,
            this, [this](QListWidgetItem*, QListWidgetItem*) { updateBottomBar(); });
}

// ═══════════════════════════════════════════════════════════════════════════════
// 面板4: 拖拽标签
// ═══════════════════════════════════════════════════════════════════════════════

void TaskChainWidget::setupPanel4DragDrop(QVBoxLayout *layout)
{
    auto *title = new QLabel(QStringLiteral("  可拖拽动作标签"));
    title->setStyleSheet(S_PANEL_TITLE);
    layout->addWidget(title);

    m_dragHintLabel = new QLabel(QStringLiteral("拖拽任务标签到组列表完成添加"));
    m_dragHintLabel->setStyleSheet("color:#777; font-size:11px; padding:4px 6px;");
    m_dragHintLabel->setWordWrap(true);
    layout->addWidget(m_dragHintLabel);

    m_actionTagList = new DraggableListWidget;
    m_actionTagList->setStyleSheet(S_LIST_STYLE);
    layout->addWidget(m_actionTagList, 1);
}

// ═══════════════════════════════════════════════════════════════════════════════
// 数据操作
// ══════════════════════════════════════════════════════════════════════════════

void TaskChainWidget::addChainItem(const QString &name)
{
    ChainData data;
    data.name = name;
    m_chains.append(data);

    int chainIndex = m_chains.size() - 1;
    auto *item = new QListWidgetItem(m_chainList);
    item->setData(Qt::UserRole, chainIndex);
    createChainRowWidget(item, chainIndex);
    m_chainList->setCurrentItem(item);
}

void TaskChainWidget::refreshChainList()
{
    m_chainList->blockSignals(true);
    m_chainList->clear();
    for (int i = 0; i < m_chains.size(); ++i) {
        auto *item = new QListWidgetItem(m_chainList);
        item->setData(Qt::UserRole, i);
        createChainRowWidget(item, i);
    }
    m_chainList->blockSignals(false);
}

void TaskChainWidget::createChainRowWidget(QListWidgetItem *item, int chainIndex)
{
    if (chainIndex < 0 || chainIndex >= m_chains.size()) return;
    const QString &name = m_chains[chainIndex].name;

    auto *row = new QWidget;
    auto *hbox = new QHBoxLayout(row);
    hbox->setContentsMargins(4, 2, 4, 2);
    hbox->setSpacing(4);

    auto *nameLabel = new QLabel(name);
    nameLabel->setStyleSheet("color:#c0c8d0; font-size:12px; background:transparent;");
    nameLabel->setToolTip(name);
    hbox->addWidget(nameLabel, 1);

    // 导出图标按钮
    auto *exportBtn = new QToolButton;
    exportBtn->setText("📥");
    exportBtn->setToolTip("导出");
    exportBtn->setAutoRaise(true);
    exportBtn->setFixedSize(22, 22);
    exportBtn->setStyleSheet("QToolButton{background:transparent;border:none;color:#8899aa;font-size:14px;}QToolButton:hover{color:#00bcd4;}");
    hbox->addWidget(exportBtn);

    // 修改名称图标按钮
    auto *renameBtn = new QToolButton;
    renameBtn->setText("✏");
    renameBtn->setToolTip("重命名");
    renameBtn->setAutoRaise(true);
    renameBtn->setFixedSize(22, 22);
    renameBtn->setStyleSheet("QToolButton{background:transparent;border:none;color:#8899aa;font-size:14px;}QToolButton:hover{color:#f0a060;}");
    hbox->addWidget(renameBtn);

    // 除图标按钮
    auto *deleteBtn = new QToolButton;
    deleteBtn->setText("🗑");
    deleteBtn->setToolTip("删除");
    deleteBtn->setAutoRaise(true);
    deleteBtn->setFixedSize(22, 22);
    deleteBtn->setStyleSheet("QToolButton{background:transparent;border:none;color:#8899aa;font-size:14px;}QToolButton:hover{color:#e05555;}");
    hbox->addWidget(deleteBtn);

    // 信号
    connect(exportBtn, &QToolButton::clicked, this, [this, chainIndex]() {
        if (chainIndex < 0 || chainIndex >= m_chains.size()) return;
        selectChain(chainIndex);
        onChainSave();
    });

    connect(renameBtn, &QToolButton::clicked, this, [this, chainIndex, nameLabel]() {
        if (chainIndex < 0 || chainIndex >= m_chains.size()) return;
        bool ok = false;
        QString newName = QInputDialog::getText(this, QStringLiteral("重命名"),
            QStringLiteral("输入名称:"), QLineEdit::Normal, m_chains[chainIndex].name, &ok);
        if (ok && !newName.trimmed().isEmpty()) {
            m_chains[chainIndex].name = newName.trimmed();
            nameLabel->setText(newName.trimmed());
            nameLabel->setToolTip(newName.trimmed());
        }
    });

    connect(deleteBtn, &QToolButton::clicked, this, [this, chainIndex, item]() {
        if (chainIndex < 0 || chainIndex >= m_chains.size()) return;
        if (QMessageBox::question(this, QStringLiteral("确认"),
                QStringLiteral("确定删除 [%1] 任务链吗？").arg(m_chains[chainIndex].name)) !=
                QMessageBox::Yes)
            return;

        QString chainName = m_chains[chainIndex].name;
        m_chains.removeAt(chainIndex);
        // 重建列表 (index 需要重新排列)
        refreshChainList();
        refreshTaskList();
        refreshGroupList();

        // 同步删除服务端任务链文件 (DELETE /task/chain/{fileName})
        QString fileName = chainName;
        if (!fileName.endsWith(".json", Qt::CaseInsensitive)) fileName += ".json";
        ApiService::instance().deleteTaskChain(fileName, [this, fileName](int code, const QJsonObject &) {
            if (code == 200) {
                updateStatus(QStringLiteral("已删除服务端任务链文件: %1").arg(fileName));
            }
        });
    });

    item->setSizeHint(QSize(0, 36));
    m_chainList->setItemWidget(item, row);
}

void TaskChainWidget::selectChain(int index)
{
    if (index < 0 || index >= m_chains.size()) return;
    m_currentChainIndex = index;
    m_currentTaskIndex = -1;   // 重置任务选择
    refreshTaskList();
    refreshGroupList();
    updateBottomBar();
}

int TaskChainWidget::currentChainIndex() const
{
    auto *item = m_chainList->currentItem();
    if (!item) return -1;
    return item->data(Qt::UserRole).toInt();
}

QJsonArray TaskChainWidget::currentTaskGroups() const
{
    int chainIdx = currentChainIndex();
    if (chainIdx < 0 || chainIdx >= m_chains.size()) return QJsonArray();
    if (m_currentTaskIndex < 0 || m_currentTaskIndex >= m_chains[chainIdx].tasks.size())
        return QJsonArray();
    QJsonObject task = m_chains[chainIdx].tasks[m_currentTaskIndex].toObject();
    return task.value("groups").toArray();
}

void TaskChainWidget::setCurrentTaskGroups(const QJsonArray &groups)
{
    int chainIdx = currentChainIndex();
    if (chainIdx < 0 || chainIdx >= m_chains.size()) return;
    if (m_currentTaskIndex < 0 || m_currentTaskIndex >= m_chains[chainIdx].tasks.size()) return;
    QJsonObject task = m_chains[chainIdx].tasks[m_currentTaskIndex].toObject();
    task["groups"] = groups;
    m_chains[chainIdx].tasks[m_currentTaskIndex] = task;
}

void TaskChainWidget::refreshTaskList()
{
    m_taskList->blockSignals(true);
    m_taskList->clear();

    int idx = currentChainIndex();
    if (idx >= 0 && idx < m_chains.size()) {
        const ChainData &chain = m_chains[idx];
        for (int i = 0; i < chain.tasks.size(); ++i) {
            QJsonObject task = chain.tasks[i].toObject();
            QString subTaskName = task.value("sub_task_name").toString();
            if (subTaskName.isEmpty()) subTaskName = task.value("name").toString();
            if (subTaskName.isEmpty()) subTaskName = QString("sub_task_%1").arg(i + 1);

            QString note = task.value("note").toString();
            bool checked = task.contains("enable") ? task.value("enable").toBool() : task.value("checked").toBool();

            auto *item = new QListWidgetItem(m_taskList);
            item->setData(Qt::UserRole, i);

            auto *row = new QWidget;
            auto *vbox = new QVBoxLayout(row);
            vbox->setContentsMargins(4, 4, 4, 4);
            vbox->setSpacing(2);

            auto *topRow = new QHBoxLayout;
            topRow->setSpacing(6);
            auto *checkBox = new QCheckBox;
            checkBox->setChecked(checked);
            checkBox->setStyleSheet("QCheckBox{spacing:4px;} QCheckBox::indicator{width:16px;height:16px;}");
            topRow->addWidget(checkBox);

            auto *nameLabel = new QLabel(subTaskName);
            nameLabel->setStyleSheet("color:#00bcd4; font-size:13px; font-weight:bold; background:transparent;");
            topRow->addWidget(nameLabel);
            topRow->addStretch();
            vbox->addLayout(topRow);

            auto *noteEdit = new QTextEdit(note);
            noteEdit->setPlaceholderText(QStringLiteral("备注..."));
            noteEdit->setMinimumHeight(50);
            noteEdit->setStyleSheet(
                "QTextEdit{background:#22272d; border:1px solid #2a3038; "
                "border-radius:3px; padding:3px 6px; color:#c0c8d0; font-size:11px;}"
                "QTextEdit:focus{border-color:#00bcd4;}");

            // 勾选框变化时同步 enable 与 checked 状态
            connect(checkBox, &QCheckBox::toggled, this,
                [this, idx, i](bool state) {
                    if (idx < 0 || idx >= m_chains.size()) return;
                    if (i < 0 || i >= m_chains[idx].tasks.size()) return;
                    QJsonObject task = m_chains[idx].tasks[i].toObject();
                    task["enable"]  = state;
                    task["checked"] = state;
                    m_chains[idx].tasks[i] = task;
                    updateTaskSelectBtn();
                });

            // 备注修改时保存到数据 + 动态调整高度
            connect(noteEdit, &QTextEdit::textChanged, this,
                [this, idx, i, noteEdit, item, nameLabel]() {
                    if (idx < 0 || idx >= m_chains.size()) return;
                    if (i < 0 || i >= m_chains[idx].tasks.size()) return;
                    QJsonObject task = m_chains[idx].tasks[i].toObject();
                    task["note"] = noteEdit->toPlainText();
                    m_chains[idx].tasks[i] = task;

                    // 动态调整高度: 基于文档行数
                    int lineCount = noteEdit->document()->size().height() / noteEdit->fontMetrics().lineSpacing();
                    int h = qMax(60, qMin(200, lineCount * noteEdit->fontMetrics().lineSpacing() + 14));
                    if (noteEdit->height() != h) {
                        noteEdit->setFixedHeight(h);
                        item->setSizeHint(QSize(0, nameLabel->height() + h + 32));
                    }
                });
            vbox->addWidget(noteEdit);

            int initH = qMax(60, noteEdit->document()->size().height() > 0 ?
                (int)(noteEdit->document()->size().height() + 14) : 60);
            item->setSizeHint(QSize(0, nameLabel->sizeHint().height() + initH + 32));
            m_taskList->setItemWidget(item, row);
        }
    }
    m_taskCountLabel->setText(QStringLiteral("共 [%1] 项").arg(m_taskList->count()));
    m_taskList->blockSignals(false);
    updateTaskSelectBtn();
}

void TaskChainWidget::refreshGroupList()
{
    m_groupList->blockSignals(true);
    m_groupList->clear();

    QJsonArray groups = currentTaskGroups();
    for (int i = 0; i < groups.size(); ++i) {
        QJsonObject groupObj = groups[i].toObject();
        bool checked = groupObj.value("checked").toBool();

        auto *item = new QListWidgetItem(m_groupList);
        item->setData(Qt::UserRole, i);
        // 不使用 ItemIsUserCheckable, 用自定义 QCheckBox 代替
        createGroupRowWidget(item, i, checked);
    }
    m_groupList->blockSignals(false);
    updateGroupSelectBtn();
    updateBottomBar();
}

void TaskChainWidget::createGroupRowWidget(QListWidgetItem *item, int groupIndex, bool checked)
{
    QJsonArray groups = currentTaskGroups();
    if (groupIndex < 0 || groupIndex >= groups.size()) return;

    QJsonObject group = groups[groupIndex].toObject();
    QString groupName = group.value("group_name").toString();
    if (groupName.isEmpty()) groupName = group.value("name").toString();
    if (groupName.isEmpty()) groupName = QString("group_%1").arg(groupIndex + 1);

    QString actionType = group.value("actionType").toString();
    QJsonArray actions = group.value("actions").toArray();

    auto *container = new QWidget;
    container->setAcceptDrops(true);
    auto *vbox = new QVBoxLayout(container);
    vbox->setContentsMargins(4, 4, 4, 4);
    vbox->setSpacing(3);

    auto *headerRow = new QHBoxLayout;
    headerRow->setSpacing(6);

    auto *checkBox = new QCheckBox;
    checkBox->setChecked(checked);
    checkBox->setStyleSheet("QCheckBox{spacing:4px;} QCheckBox::indicator{width:16px;height:16px;}");
    headerRow->addWidget(checkBox);

    auto *nameLbl = new QLabel(groupName);
    nameLbl->setStyleSheet("color:#00bcd4; font-size:12px; font-weight:bold; background:transparent;");
    headerRow->addWidget(nameLbl);

    if (!actionType.isEmpty()) {
        auto *typeLbl = new QLabel(QStringLiteral("  [%1]").arg(actionType));
        typeLbl->setStyleSheet("color:#888; font-size:11px; background:transparent;");
        headerRow->addWidget(typeLbl);
    }
    headerRow->addStretch();

    auto *delBtn = new QToolButton;
    delBtn->setText("🗑");
    delBtn->setToolTip(QStringLiteral("删除组"));
    delBtn->setAutoRaise(true);
    delBtn->setFixedSize(20, 20);
    delBtn->setStyleSheet("QToolButton{background:transparent;border:none;color:#8899aa;font-size:13px;}QToolButton:hover{color:#e05555;}");
    headerRow->addWidget(delBtn);
    vbox->addLayout(headerRow);

    // 勾选框变化时同步 enable 与 checked 状态
    connect(checkBox, &QCheckBox::toggled, this,
        [this, groupIndex](bool state) {
            QJsonArray groups = currentTaskGroups();
            if (groupIndex < 0 || groupIndex >= groups.size()) return;
            QJsonObject group = groups[groupIndex].toObject();
            group["enable"]  = state;
            group["checked"] = state;
            groups[groupIndex] = group;
            setCurrentTaskGroups(groups);
            updateGroupSelectBtn();
        });

    // 如果没有标签, 显示占位提示
    if (actionType.isEmpty()) {
        auto *hint = new QLabel(QStringLiteral("  拖拽标签到此组..."));
        hint->setStyleSheet("color:#555; font-size:11px; background:transparent; padding:8px 4px;");
        hint->setMinimumHeight(60);
        vbox->addWidget(hint);
    } else {
        // 显示每个动作标签 + 删除按钮 + 属性按钮 + 参数区域
        for (int a = 0; a < actions.size(); ++a) {
            QJsonObject action = actions[a].toObject();
            QString actionName = action.value("name").toString();
            if (actionName.isEmpty()) actionName = action.value("action_desc").toString();
            QJsonObject params = action.value("params").toObject();

            // 为“路径导航”解析站点显示名称
            QString displayTarget = "未知";
            QString targetId = params.value("target_station").toString();
            if (targetId.isEmpty()) targetId = params.value("target").toString();
            if (targetId.isEmpty()) targetId = action.value("target_station").toString();

            if (!targetId.isEmpty()) {
                displayTarget = targetId;
                for (const auto &st : m_stations) {
                    if (st.id == targetId || st.name == targetId) {
                        displayTarget = st.name;
                        break;
                    }
                }
            }

            QString displayName = actionName;
            if (actionName == "路径导航" || action.value("action_desc").toString() == "路径导航") {
                displayName = QStringLiteral("路径导航 -> %1").arg(displayTarget);
            }

            // 动作行: 序号 + 名称 + 属性按钮 + 删除标签按钮
            auto *actRow = new QHBoxLayout;
            actRow->setSpacing(4);

            auto *actLabel = new QLabel(QStringLiteral("%1. %2").arg(a + 1).arg(displayName));
            actLabel->setStyleSheet("color:#c0c8d0; font-size:11px; background:transparent; padding:2px 8px;");
            actLabel->setMinimumHeight(24);
            actRow->addWidget(actLabel, 1);

            // 属性按钮
            auto *attrBtn = new QToolButton;
            attrBtn->setText("⚙");
            attrBtn->setToolTip(QStringLiteral("属性"));
            attrBtn->setAutoRaise(true);
            attrBtn->setFixedSize(22, 22);
            attrBtn->setStyleSheet("QToolButton{background:transparent;border:none;color:#8899aa;font-size:14px;}QToolButton:hover{color:#f0a060;}");
            actRow->addWidget(attrBtn);

            // 删除标签按钮
            auto *delTagBtn = new QToolButton;
            delTagBtn->setText("✕");
            delTagBtn->setToolTip(QStringLiteral("删除标签"));
            delTagBtn->setAutoRaise(true);
            delTagBtn->setFixedSize(22, 22);
            delTagBtn->setStyleSheet("QToolButton{background:transparent;border:none;color:#8899aa;font-size:12px;}QToolButton:hover{color:#e05555;}");
            actRow->addWidget(delTagBtn);

            vbox->addLayout(actRow);

            // 参数摘要区域 (击弹出定义对话框)
            QString paramSummary;
            if (params.isEmpty()) {
                paramSummary = QStringLiteral("未设置 — 点击编辑");
            } else {
                QStringList parts;
                for (auto it = params.begin(); it != params.end(); ++it)
                    parts << QStringLiteral("%1: %2").arg(it.key(), it.value().toVariant().toString());
                paramSummary = parts.join(", ");
            }

            auto *paramBtn = new QPushButton(paramSummary);
            paramBtn->setStyleSheet(
                "QPushButton{text-align:left; background:#22272d; border:1px solid #2a3038; "
                "border-radius:3px; padding:6px 10px; color:#999; font-size:11px; margin-left:8px;}"
                "QPushButton:hover{border-color:#00bcd4; color:#c0c8d0;}");
            paramBtn->setToolTip(QStringLiteral("点击编辑参数"));
            paramBtn->setMinimumHeight(32);

            // 属性按钮 → 打开 ActionAttributesForm 对话框 (占位)
            connect(attrBtn, &QToolButton::clicked, this, [this, actionName]() {
                updateStatus(QStringLiteral("属性设置: %1 (占位)").arg(actionName));
            });

            // 参数按钮 → 打开参数定义对话框
            connect(paramBtn, &QPushButton::clicked, this,
                [this, groupIndex, a, actionName]() {
                    ActionDef *def = findActionDef(actionName);
                    if (!def) return;

                    QJsonArray groups = currentTaskGroups();
                    if (groupIndex < 0 || groupIndex >= groups.size()) return;
                    QJsonObject group = groups[groupIndex].toObject();
                    QJsonArray acts = group.value("actions").toArray();
                    if (a < 0 || a >= acts.size()) return;
                    QJsonObject action = acts[a].toObject();
                    QJsonObject existingParams = action.value("params").toObject();

                    QString result = openActionDefDialog(*def, existingParams);
                    if (result.isEmpty()) return;

                    QJsonDocument doc = QJsonDocument::fromJson(result.toUtf8());
                    if (doc.isObject()) {
                        action["params"] = doc.object();
                        acts[a] = action;
                        group["actions"] = acts;
                        groups[groupIndex] = group;
                        setCurrentTaskGroups(groups);
                        refreshGroupList();
                    }
                });

            vbox->addWidget(paramBtn);

            // 删除标签按钮 → 从 actions 数组中删除
            connect(delTagBtn, &QToolButton::clicked, this,
                [this, groupIndex, a]() {
                    QJsonArray groups = currentTaskGroups();
                    if (groupIndex < 0 || groupIndex >= groups.size()) return;
                    QJsonObject group = groups[groupIndex].toObject();
                    QJsonArray acts = group.value("actions").toArray();
                    if (a < 0 || a >= acts.size()) return;
                    acts.removeAt(a);
                    // 如果删完了, 清空 actionType
                    if (acts.isEmpty())
                        group.remove("actionType");
                    group["actions"] = acts;
                    groups[groupIndex] = group;
                    setCurrentTaskGroups(groups);
                    refreshGroupList();
                });
        }
    }

    // 计算合适高度
    int contentH = 0;
    if (actionType.isEmpty()) {
        contentH = 60;
    } else {
        contentH = actions.size() * (24 + 32 + 8);
    }
    int totalH = 28 + contentH + 16;
    item->setSizeHint(QSize(0, qMax(80, totalH)));
    m_groupList->setItemWidget(item, container);

    // 删除组按钮
    connect(delBtn, &QToolButton::clicked, this, [this, groupIndex]() {
        QJsonArray groups = currentTaskGroups();
        if (groupIndex < 0 || groupIndex >= groups.size()) return;
        if (QMessageBox::question(this, QStringLiteral("确认"),
                QStringLiteral("确认删除组 [%1] 吗？").arg(
                    groups[groupIndex].toObject().value("name").toString())) ==
                QMessageBox::Yes) {
            groups.removeAt(groupIndex);
            setCurrentTaskGroups(groups);
            refreshGroupList();
        }
    });
}

void TaskChainWidget::refreshActionTags()
{
    m_actionTagList->clear();
    for (const auto &def : m_actionDefs) {
        auto *item = new QListWidgetItem(def.name, m_actionTagList);
        item->setToolTip(def.category);
    }
}

void TaskChainWidget::initActionDefs()
{
    m_actionDefs.clear();

    ActionDef def;

    // 1. 路径导航 (仅包含目标站点设置，选中后 target_station 赋值为点位 id)
    def = {"路径导航", "nav"};
    def.params = {
        {"target", "目标站点:", "station_combo", {}, "未选择时显示“未知”", 0},
    };
    m_actionDefs.append(def);

    // 2. 平动
    def = {"平动", "motion"};
    def.params = {
        {"direction", "平动方向:", "combo", {"前进", "后退", "左移", "右移"}, "", 0},
        {"distance", "移动距离(m):", "double", {}, "m", 1.0},
        {"speed", "平动速度(m/s):", "double", {}, "m/s", 0.2},
    };
    m_actionDefs.append(def);

    // 3. 转动
    def = {"转动", "motion"};
    def.params = {
        {"direction", "旋转方向:", "combo", {"顺时针(右转)", "逆时针(左转)"}, "", 0},
        {"angle", "旋转角度(°):", "double", {}, "°", 90.0},
        {"speed", "角速度(rad/s):", "double", {}, "rad/s", 0.5},
    };
    m_actionDefs.append(def);

    // 4. 播放音频
    def = {"播放音频", "audio"};
    def.params = {
        {"audioName", "音频名称:", "audio_combo", {}, "选择音频文件", 0},
        {"loop", "循环播放:", "combo", {"否", "是"}, "", 0},
    };
    m_actionDefs.append(def);

    // 5. 顶升
    def = {"顶升", "jack"};
    def.params = {
        {"operation", "动作:", "combo", {"顶起", "放下"}, "", 0},
        {"height", "高度(m):", "double", {}, "m", 0.05},
    };
    m_actionDefs.append(def);

    // 6. 停止播放音频
    def = {"停止播放音频", "audio"};
    def.params = {
        {"audioName", "音频名称:", "audio_combo", {}, "选择音频文件", 0},
    };
    m_actionDefs.append(def);
}

ActionDef* TaskChainWidget::findActionDef(const QString &name)
{
    for (auto &def : m_actionDefs) {
        if (def.name == name) return &def;
    }
    return nullptr;
}

QString TaskChainWidget::openActionDefDialog(const ActionDef &def, const QJsonObject &existingParams)
{
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("设置: %1").arg(def.name));
    dlg.setMinimumWidth(360);
    auto *vbox = new QVBoxLayout(&dlg);
    vbox->setContentsMargins(16, 16, 16, 16);
    vbox->setSpacing(10);

    // 为每个参数创建对应控件
    QMap<QString, QWidget*> inputWidgets;

    for (const auto &param : def.params) {
        auto *label = new QLabel(param.label);
        label->setStyleSheet("color:#c0c8d0; font-size:12px;");
        vbox->addWidget(label);

        QWidget *input = nullptr;

        // 获取当前值
        QJsonValue existingVal = existingParams.value(param.key);

        if (param.type == "text") {
            auto *edit = new QLineEdit;
            edit->setText(existingVal.toString());
            edit->setStyleSheet("QLineEdit{background:#22272d;border:1px solid #2a3038;border-radius:3px;padding:5px;color:#c0c8d0;font-size:12px;}");
            if (!param.unit.isEmpty()) edit->setPlaceholderText(param.unit);
            input = edit;
        } else if (param.type == "double") {
            auto *spin = new QDoubleSpinBox;
            const auto &mc = MotionConfigManager::instance();
            if (param.key == "speed" || param.unit == "m/s") {
                spin->setRange(0.01, mc.maxSpeed());
                spin->setSingleStep(0.05);
                spin->setValue(mc.clampLinearSpeed(existingVal.toDouble(param.defaultValue)));
                spin->setToolTip(QStringLiteral("受全局最大速度限制 (上限: %1 m/s)").arg(mc.maxSpeed()));
            } else if (param.unit == "rad/s" || (param.key == "speed" && def.name.contains("转"))) {
                spin->setRange(0.01, mc.maxAngularSpeedRad());
                spin->setSingleStep(0.05);
                spin->setValue(mc.clampAngularSpeedRad(existingVal.toDouble(param.defaultValue)));
                spin->setToolTip(QStringLiteral("受全局最大角速度限制 (上限: %1 rad/s)").arg(mc.maxAngularSpeedRad(), 0, 'f', 3));
            } else {
                spin->setRange(-99999, 99999);
                spin->setValue(existingVal.toDouble(param.defaultValue));
            }
            spin->setDecimals(3);
            if (!param.unit.isEmpty()) spin->setSuffix(" " + param.unit);
            spin->setStyleSheet("QDoubleSpinBox{background:#22272d;border:1px solid #2a3038;border-radius:3px;padding:5px;color:#c0c8d0;font-size:12px;}");
            input = spin;
        } else if (param.type == "int") {
            auto *spin = new QSpinBox;
            spin->setRange(-99999, 99999);
            spin->setValue(existingVal.toInt(param.defaultValue));
            if (!param.unit.isEmpty()) spin->setSuffix(" " + param.unit);
            spin->setStyleSheet("QSpinBox{background:#22272d;border:1px solid #2a3038;border-radius:3px;padding:5px;color:#c0c8d0;font-size:12px;}");
            input = spin;
        } else if (param.type == "combo") {
            auto *combo = new QComboBox;
            combo->addItems(param.options);
            int idx = combo->findText(existingVal.toString());
            if (idx >= 0) combo->setCurrentIndex(idx);
            combo->setStyleSheet("QComboBox{background:#22272d;border:1px solid #2a3038;border-radius:3px;padding:5px;color:#c0c8d0;font-size:12px;}");
            input = combo;
        } else if (param.type == "audio_combo") {
            // 音频名称: 数据来自服务端音频列表, 显示截断名, 实际保存完整文件名
            auto *combo = new QComboBox;
            const QStringList files = fetchAudioFiles();
            if (!files.isEmpty()) m_audioFiles = files;

            for (const QString &full : m_audioFiles) {
                combo->addItem(audioDisplayName(full), full);
                combo->setItemData(combo->count() - 1, full, Qt::ToolTipRole);
            }

            const QString cur = existingVal.toString();
            int idx = -1;
            for (int i = 0; i < combo->count(); ++i) {
                if (combo->itemData(i).toString() == cur) { idx = i; break; }
            }
            if (idx < 0 && !cur.isEmpty()) {
                // 列表中找不到 (如服务端已删除) 时保留原完整文件名
                combo->addItem(audioDisplayName(cur), cur);
                combo->setItemData(combo->count() - 1, cur, Qt::ToolTipRole);
                idx = combo->count() - 1;
            }
            if (idx < 0 && combo->count() > 0) idx = 0;
            if (idx >= 0) combo->setCurrentIndex(idx);
            combo->setEditable(combo->count() == 0);   // 列表为空时允许手动输入
            combo->setStyleSheet("QComboBox{background:#22272d;border:1px solid #2a3038;border-radius:3px;padding:5px;color:#c0c8d0;font-size:12px;}");
            input = combo;
        } else if (param.type == "station_combo") {
            auto *combo = new QComboBox;
            combo->addItem(QStringLiteral("未知"), "");
            QString currentTarget = existingParams.value("target_station").toString();
            if (currentTarget.isEmpty()) currentTarget = existingParams.value("target").toString();

            int selectedIdx = 0;
            for (int s = 0; s < m_stations.size(); ++s) {
                const auto &st = m_stations[s];
                combo->addItem(st.name, st.id);
                if (!currentTarget.isEmpty() && (currentTarget == st.id || currentTarget == st.name)) {
                    selectedIdx = combo->count() - 1;
                }
            }
            combo->setCurrentIndex(selectedIdx);
            combo->setStyleSheet("QComboBox{background:#22272d;border:1px solid #2a3038;border-radius:3px;padding:5px;color:#c0c8d0;font-size:12px;}");
            input = combo;
        }

        if (input) {
            vbox->addWidget(input);
            inputWidgets[param.key] = input;
        }
    }

    if (def.params.isEmpty()) {
        auto *hint = new QLabel(QStringLiteral("此动作无参数"));
        hint->setStyleSheet("color:#777; font-size:12px;");
        hint->setAlignment(Qt::AlignCenter);
        vbox->addWidget(hint);
    }

    // 确定/取消按钮
    auto *btnRow = new QHBoxLayout;
    btnRow->addStretch();
    auto *okBtn = new QPushButton(QStringLiteral("确定"));
    auto *cancelBtn = new QPushButton(QStringLiteral("取消"));
    okBtn->setStyleSheet(S_BTN_STYLE);
    cancelBtn->setStyleSheet(S_BTN_STYLE);
    btnRow->addWidget(okBtn);
    btnRow->addWidget(cancelBtn);
    vbox->addLayout(btnRow);

    connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return QString();

    // 收集参数
    QJsonObject resultParams;
    for (const auto &param : def.params) {
        QWidget *w = inputWidgets.value(param.key);
        if (!w) continue;

        if (param.type == "text") {
            auto *edit = qobject_cast<QLineEdit*>(w);
            resultParams[param.key] = edit->text();
        } else if (param.type == "double") {
            auto *spin = qobject_cast<QDoubleSpinBox*>(w);
            double val = spin->value();
            const auto &mc = MotionConfigManager::instance();
            if (param.key == "speed" || param.unit == "m/s") {
                val = mc.clampLinearSpeed(val);
            } else if (param.unit == "rad/s" || (param.key == "speed" && def.name.contains("转"))) {
                val = mc.clampAngularSpeedRad(val);
            }
            resultParams[param.key] = val;
        } else if (param.type == "int") {
            auto *spin = qobject_cast<QSpinBox*>(w);
            resultParams[param.key] = spin->value();
        } else if (param.type == "combo") {
            auto *combo = qobject_cast<QComboBox*>(w);
            resultParams[param.key] = combo->currentText();
        } else if (param.type == "audio_combo") {
            auto *combo = qobject_cast<QComboBox*>(w);
            // 保存的是完整文件名 (itemData), 列表为空手输时退化为输入内容
            QString full = combo->currentData().toString();
            if (full.isEmpty()) full = combo->currentText().trimmed();
            resultParams[param.key] = full;
        } else if (param.type == "station_combo") {
            auto *combo = qobject_cast<QComboBox*>(w);
            QString stationName = combo->currentText();
            if (stationName.isEmpty() || stationName == QStringLiteral("未知")) {
                stationName = combo->currentData().toString();
            }
            if (stationName.isEmpty() || stationName == QStringLiteral("未知")) {
                stationName = "LM3";
            }
            resultParams["target_station"] = stationName;
        }
    }

    QJsonDocument doc(resultParams);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
}

void TaskChainWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    loadMapStations();
}

void TaskChainWidget::loadMapStations()
{
    QString carId = AppConfig::instance().agvId();
    if (carId.isEmpty()) carId = AppConfig::instance().currentDevice().carId;

    auto doFetchRoute = [this](const QString &/*mapId*/) {
        ApiService::instance().loadEditedMap([this](int code, const QJsonObject &data) {
            if (code != 200) return;
            m_stations.clear();

            QString jsonStr = data.value("json").toString();
            if (!jsonStr.isEmpty()) {
                QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
                if (doc.isObject()) {
                    QJsonArray features = doc.object().value("features").toArray();
                    for (const auto &fv : features) {
                        QJsonObject f = fv.toObject();
                        QJsonObject props = f.value("properties").toObject();
                        QJsonObject geom  = f.value("geometry").toObject();
                        if (geom.value("type").toString() == "Point") {
                            StationItem item;
                            item.id   = props.value("id").toString();
                            item.name = props.value("name").toString();
                            if (item.name.isEmpty()) item.name = item.id;
                            if (!item.id.isEmpty()) {
                                m_stations.append(item);
                            }
                        }
                    }
                }
            }
            refreshGroupList();
        });
    };

    if (!carId.isEmpty()) {
        ApiService::instance().getCar(carId, [this, doFetchRoute](int code, const QJsonObject &data) {
            QString mapId;
            if (code == 200) {
                mapId = data.value("mapId").toString();
            }
            doFetchRoute(mapId);
        });
    } else {
        doFetchRoute("");
    }
}

void TaskChainWidget::updateStatus(const QString &text)
{
    m_chainStatus->setText(text);
}

// ═══════════════════════════════════════════════════════════════════════════════
// 槽函数实现
// ═══════════════════════════════════════════════════════════════════════════════

// ── 面板1: 任链列表 ──

void TaskChainWidget::onChainNew()
{
    // 任务链名称即服务端文件名, 必须全局唯一: task_<时间戳>_<序号>
    const QString base = QString("task_%1").arg(QDateTime::currentMSecsSinceEpoch());
    int seq = 1;
    for (const auto &c : m_chains) {
        if (c.name.startsWith(base + "_")) {
            bool ok = false;
            int n = c.name.mid(base.length() + 1).toInt(&ok);
            if (ok && n >= seq) seq = n + 1;
        }
    }
    QString name = QString("%1_%2").arg(base).arg(seq);

    ChainData data;
    data.name = name;

    // 创建默认的标准子任务与组结构
    QJsonObject defaultTask;
    defaultTask["sub_task_name"] = "sub_task_1";
    defaultTask["name"]          = "sub_task_1";
    defaultTask["enable"]        = true;
    defaultTask["checked"]       = true;
    defaultTask["note"]          = "";

    QJsonObject defaultGroup;
    defaultGroup["group_name"] = "group_1";
    defaultGroup["name"]       = "group_1";
    defaultGroup["enable"]     = true;
    defaultGroup["checked"]    = true;
    defaultGroup["actionType"] = "路径导航";

    QJsonObject defaultAction;
    defaultAction["name"]           = "路径导航";
    defaultAction["action_desc"]    = "路径导航";
    defaultAction["action_type"]    = "0";
    QString stName = !m_stations.isEmpty() ? m_stations[0].name : "LM3";
    defaultAction["target_station"] = stName;

    QJsonObject params;
    params["target_station"] = stName;
    params["target"]         = stName;
    defaultAction["params"]  = params;

    QJsonArray actions;
    actions.append(defaultAction);
    defaultGroup["actions"] = actions;

    QJsonArray groups;
    groups.append(defaultGroup);
    defaultTask["group_info"] = groups;
    defaultTask["groups"]     = groups;

    data.tasks.append(defaultTask);

    m_chains.append(data);
    refreshChainList();
    selectChain(m_chains.size() - 1);
    updateStatus(QStringLiteral("已新建标准任务链: %1").arg(name));
}

void TaskChainWidget::onChainLoad()
{
    QString filePath = QFileDialog::getOpenFileName(this, QStringLiteral("加载任务链 JSON"), "", "JSON Files (*.json *.task)");
    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("无法打开所选 JSON 文件"));
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("JSON 格式错误: %1").arg(err.errorString()));
        return;
    }

    ChainData loadedChain = importChainFromJson(doc.object());
    if (loadedChain.name.isEmpty()) {
        loadedChain.name = QFileInfo(filePath).baseName();
    }

    m_chains.append(loadedChain);
    refreshChainList();
    selectChain(m_chains.size() - 1);
    updateStatus(QStringLiteral("已成功加载任务链: %1").arg(loadedChain.name));

    // 同步上传至服务端任务链目录 (POST /task/chain/save, multipart 字段 taskFile)
    ApiService::instance().saveTaskChain(filePath, [this](int code, const QJsonObject &resp) {
        if (code == 200) {
            updateStatus(QStringLiteral("任务链已加载并同步上传至服务器"));
            Toast::success(this, QStringLiteral("任务链已载入并上传至服务器"));
        } else {
            Toast::error(this, QStringLiteral("任务链上传失败(%1): %2")
                .arg(code).arg(resp.value("msg").toString(QStringLiteral("请求失败"))));
        }
    });
}

void TaskChainWidget::onChainSave()
{
    if (m_chains.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
            QStringLiteral("任务链列表为空, 无可保存数据"));
        return;
    }

    // 一次性保存列表中所有任务链: POST /task/chain/batchSave
    // Body: [{"fileName": 任务链名称, "fileContent": 该任务链全部数据}]
    QJsonArray chains;
    for (const auto &chain : m_chains) {
        QJsonObject item;
        item["fileName"]    = chain.name;
        item["fileContent"] = exportChainToJson(chain);
        chains.append(item);
    }

    updateStatus(QStringLiteral("正在保存任务链列表 (%1 条)...").arg(chains.size()));
    ApiService::instance().batchSaveTaskChain(chains,
        [this, count = chains.size()](int code, const QJsonObject &resp) {
            if (code == 200) {
                updateStatus(QStringLiteral("已保存任务链列表: %1 条").arg(count));
                Toast::success(this, QStringLiteral("已保存 %1 条任务链到服务器").arg(count));
            } else {
                Toast::error(this, QStringLiteral("任务链保存失败(%1): %2")
                    .arg(code).arg(resp.value("msg").toString(QStringLiteral("请求失败"))));
            }
        });
}

void TaskChainWidget::onChainItemChanged(QListWidgetItem *current)
{
    if (!current) return;
    int idx = current->data(Qt::UserRole).toInt();
    selectChain(idx);
    updateBottomBar();
}

// ── 面板2: 任务模块 ──

void TaskChainWidget::onTaskNew()
{
    int idx = currentChainIndex();
    if (idx < 0) {
        QMessageBox::warning(this, QStringLiteral("提示"),
            QStringLiteral("请先选择或新建任务链"));
        return;
    }

    QJsonObject task;
    QString subName = QString("sub_task_%1").arg(m_chains[idx].tasks.size() + 1);
    task["sub_task_name"] = subName;
    task["name"]          = subName;
    task["note"]          = "";
    task["enable"]        = true;
    task["checked"]       = true;
    task["group_info"]    = QJsonArray();
    task["groups"]        = QJsonArray();
    m_chains[idx].tasks.append(task);
    refreshTaskList();
}

void TaskChainWidget::onTaskDelete()
{
    int idx = currentChainIndex();
    if (idx < 0) return;

    auto selected = m_taskList->selectedItems();
    if (selected.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
            QStringLiteral("请勾选需要删除的项"));
        return;
    }

    if (QMessageBox::question(this, QStringLiteral("确认"),
            QStringLiteral("确定要删除勾选的 【%1】 项吗?").arg(selected.size())) !=
            QMessageBox::Yes)
        return;

    // 从后往前删除
    QList<int> rows;
    for (auto *item : selected)
        rows << m_taskList->row(item);
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (int row : rows) {
        if (row >= 0 && row < m_chains[idx].tasks.size())
            m_chains[idx].tasks.removeAt(row);
    }
    refreshTaskList();
}

void TaskChainWidget::onTaskSelectAll()
{
    // 互斥: 全部已选中 → 取消选中; 否则 → 全选
    // 通过遍历 itemWidget 找到 QCheckBox
    bool allChecked = true;
    QList<QCheckBox*> checkBoxes;
    for (int i = 0; i < m_taskList->count(); ++i) {
        auto *item = m_taskList->item(i);
        auto *widget = m_taskList->itemWidget(item);
        if (widget) {
            auto *cb = widget->findChild<QCheckBox*>();
            if (cb) {
                checkBoxes << cb;
                if (!cb->isChecked()) allChecked = false;
            }
        }
    }

    bool targetState = !allChecked;
    for (auto *cb : checkBoxes)
        cb->setChecked(targetState);
    // 勾选框 toggled 信号会自动同步到数据
    updateTaskSelectBtn();
}

void TaskChainWidget::onTaskItemChanged(QListWidgetItem *item)
{
    int idx = currentChainIndex();
    if (idx < 0 || !item) return;
    int row = m_taskList->row(item);
    if (row >= 0 && row < m_chains[idx].tasks.size()) {
        QJsonObject task = m_chains[idx].tasks[row].toObject();
        task["checked"] = (item->checkState() == Qt::Checked);
        m_chains[idx].tasks[row] = task;
    }
    updateTaskSelectBtn();
}

// ── 面板3: 组模块 ──

void TaskChainWidget::onGroupAdd()
{
    int idx = currentChainIndex();
    if (idx < 0) {
        QMessageBox::warning(this, QStringLiteral("提示"),
            QStringLiteral("请先选择或新建任务链"));
        return;
    }
    if (m_currentTaskIndex < 0) {
        QMessageBox::warning(this, QStringLiteral("提示"),
            QStringLiteral("请先选择一个任务"));
        return;
    }

    QJsonArray groups = currentTaskGroups();
    QJsonObject group;
    QString gName = QString("group_%1").arg(groups.size() + 1);
    group["group_name"] = gName;
    group["name"]       = gName;
    group["enable"]     = true;
    group["checked"]    = true;
    group["actions"]    = QJsonArray();
    groups.append(group);
    setCurrentTaskGroups(groups);

    refreshGroupList();
}

void TaskChainWidget::onGroupInsert()
{
    int idx = currentChainIndex();
    if (idx < 0) return;
    if (m_currentTaskIndex < 0) {
        QMessageBox::warning(this, QStringLiteral("提示"),
            QStringLiteral("请先选择一个任务"));
        return;
    }

    int row = m_groupList->currentRow();
    QJsonArray groups = currentTaskGroups();
    QJsonObject group;
    QString gName = QString("group_%1").arg(groups.size() + 1);
    group["group_name"] = gName;
    group["name"]       = gName;
    group["enable"]     = true;
    group["checked"]    = true;
    group["actions"]    = QJsonArray();

    if (row >= 0) {
        groups.insert(row, group);
    } else {
        groups.append(group);
    }
    setCurrentTaskGroups(groups);
    refreshGroupList();
}

void TaskChainWidget::onGroupDelete()
{
    int idx = currentChainIndex();
    if (idx < 0) return;
    if (m_currentTaskIndex < 0) return;

    auto selected = m_groupList->selectedItems();
    if (selected.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
            QStringLiteral("请勾选需要删除的项"));
        return;
    }

    QList<int> rows;
    for (auto *item : selected)
        rows << m_groupList->row(item);
    std::sort(rows.begin(), rows.end(), std::greater<int>());

    QJsonArray groups = currentTaskGroups();
    for (int row : rows) {
        if (row >= 0 && row < groups.size())
            groups.removeAt(row);
    }
    setCurrentTaskGroups(groups);
    refreshGroupList();
}

void TaskChainWidget::onGroupSelectAll()
{
    // 互斥: 通过遍历 itemWidget 找到 QCheckBox
    bool allChecked = true;
    QList<QCheckBox*> checkBoxes;
    for (int i = 0; i < m_groupList->count(); ++i) {
        auto *widget = m_groupList->itemWidget(m_groupList->item(i));
        if (widget) {
            auto *cb = widget->findChild<QCheckBox*>();
            if (cb) {
                checkBoxes << cb;
                if (!cb->isChecked()) allChecked = false;
            }
        }
    }

    bool targetState = !allChecked;
    for (auto *cb : checkBoxes)
        cb->setChecked(targetState);
    // 勾选框 toggled 信号会自动同步到数据
    updateGroupSelectBtn();
}

void TaskChainWidget::onGroupLoopToggled(bool checked)
{
    Q_UNUSED(checked);
    // 占位: 设置循环执行模式
}

void TaskChainWidget::onGroupSendSelected()
{
    int idx = currentChainIndex();
    if (idx < 0) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先选择任务链"));
        return;
    }

    QJsonObject chainJson = exportChainToJson(m_chains[idx], true);
    QJsonArray subTasks = chainJson.value("sub_task_info").toArray();
    if (subTasks.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
            QStringLiteral("未勾选有效任务或任务链数据为空"));
        return;
    }

    // 序列化为标准任务链字符串并下发 (POST /task/chain/sendTask)
    QString chainString = QString::fromUtf8(QJsonDocument(chainJson).toJson(QJsonDocument::Compact));

    m_execStatus = Running;
    updateBottomBar();
    updateStatus(QStringLiteral("正在下发任务链 (勾选子任务): %1").arg(m_chains[idx].name));

    ApiService::instance().sendTaskChain(chainString, true, [this](int code, const QJsonObject &resp) {
        if (code == 200) {
            Toast::success(this, QStringLiteral("任务链数据已成功下发至设备"));
        } else {
            Toast::error(this, QStringLiteral("任务链下发失败(%1): %2")
                .arg(code).arg(resp.value("msg").toString(QStringLiteral("请求失败"))));
        }
    });
}

void TaskChainWidget::onGroupSendAll()
{
    int idx = currentChainIndex();
    if (idx < 0) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先选择任务链"));
        return;
    }

    QJsonObject chainJson = exportChainToJson(m_chains[idx], false);
    QJsonArray subTasks = chainJson.value("sub_task_info").toArray();
    if (subTasks.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
            QStringLiteral("任务链数据为空"));
        return;
    }

    // 序列化为标准任务链字符串并下发全部 (POST /task/chain/sendTask)
    QString chainString = QString::fromUtf8(QJsonDocument(chainJson).toJson(QJsonDocument::Compact));

    m_execStatus = Running;
    updateBottomBar();
    updateStatus(QStringLiteral("正在下发任务链 (全部子任务): %1").arg(m_chains[idx].name));

    ApiService::instance().sendTaskChain(chainString, true, [this](int code, const QJsonObject &resp) {
        if (code == 200) {
            Toast::success(this, QStringLiteral("全部任务链数据已成功下发至设备"));
        } else {
            Toast::error(this, QStringLiteral("任务链下发失败(%1): %2")
                .arg(code).arg(resp.value("msg").toString(QStringLiteral("请求失败"))));
        }
    });
}

// ── 面板4: 可拖拽标签 ──

void TaskChainWidget::onActionTriggered(const QString &actionName)
{
    if (m_currentTaskIndex < 0) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先选择一个任务"));
        return;
    }

    QJsonArray groups = currentTaskGroups();
    if (groups.isEmpty()) {
        QJsonObject newGroup;
        newGroup["group_name"] = "group_1";
        newGroup["name"]       = "group_1";
        newGroup["enable"]     = true;
        newGroup["checked"]    = true;
        groups.append(newGroup);
    }

    int groupIdx = m_groupList->currentRow();
    if (groupIdx < 0 || groupIdx >= groups.size()) groupIdx = groups.size() - 1;

    QJsonObject group = groups[groupIdx].toObject();
    group["actionType"] = actionName;

    QJsonArray actions = group.value("actions").toArray();
    QJsonObject action;
    action["name"]        = actionName;
    action["action_desc"] = actionName;
    action["action_type"] = actionTypeFromName(actionName);

    QJsonObject params;
    if (actionName == "路径导航") {
        QString stName = !m_stations.isEmpty() ? m_stations[0].name : "LM3";
        action["target_station"] = stName;
        params["target_station"] = stName;
        params["target"]         = stName;
    } else if (actionName == "平动") {
        action["direction"] = "前进";
        action["distance"]  = 1.0;
        action["speed"]     = 0.2;
        params["direction"] = "前进";
        params["distance"]  = 1.0;
        params["speed"]     = 0.2;
    } else if (actionName == "转动") {
        action["direction"] = "顺时针(右转)";
        action["angle"]     = 90.0;
        action["speed"]     = 0.5;
        params["direction"] = "顺时针(右转)";
        params["angle"]     = 90.0;
        params["speed"]     = 0.5;
    }
    action["params"] = params;
    actions.append(action);
    group["actions"] = actions;
    groups[groupIdx] = group;
    setCurrentTaskGroups(groups);

    refreshGroupList();
    updateStatus(QStringLiteral("已添加动作: %1").arg(actionName));
}

// ═══════════════════════════════════════════════════════════════════════════════
// 底部公共状态栏
// ═══════════════════════════════════════════════════════════════════════════════

void TaskChainWidget::setupBottomBar(QVBoxLayout *layout)
{
    auto *bar = new QWidget;
    bar->setFixedHeight(52);
    bar->setStyleSheet(QStringLiteral(
        "QWidget { background:#1a1f26; border-top:1px solid #2a3038; }"
        "QLabel { background:transparent; color:#8899aa; font-size:12px; }"
        "QPushButton { color:#c0c8d0; background:#2a313a; border:1px solid #3a4048; "
        "padding:5px 16px; border-radius:3px; font-size:12px; }"
        "QPushButton:hover { background:#323a44; color:#fff; }"
        "QPushButton:pressed { background:#222831; }"
        "QPushButton:disabled { color:#555; background:#222831; border-color:#2a3038; }"));

    auto *hbox = new QHBoxLayout(bar);
    hbox->setContentsMargins(12, 4, 12, 4);
    hbox->setSpacing(10);

    // 左侧: 状态信息
    m_bottomInfoLabel = new QLabel(QStringLiteral("未选择任务链"));
    m_bottomInfoLabel->setStyleSheet("font-size:12px;");
    hbox->addWidget(m_bottomInfoLabel, 1);

    // 右侧: 控制按钮
    m_suspendResumeBtn = new QPushButton(QStringLiteral("暂停任务链"));
    m_cancelBtn         = new QPushButton(QStringLiteral("取消"));
    m_executeNextBtn   = new QPushButton(QStringLiteral("执行 Next 任务"));
    m_suspendResumeBtn->setEnabled(false);
    m_cancelBtn->setEnabled(false);
    m_executeNextBtn->setEnabled(false);

    hbox->addWidget(m_suspendResumeBtn);
    hbox->addWidget(m_cancelBtn);
    hbox->addWidget(m_executeNextBtn);

    layout->addWidget(bar);

    // 连接
    connect(m_suspendResumeBtn, &QPushButton::clicked, this, &TaskChainWidget::onSuspendResume);
    connect(m_cancelBtn,        &QPushButton::clicked, this, &TaskChainWidget::onCancelChain);
    connect(m_executeNextBtn,   &QPushButton::clicked, this, &TaskChainWidget::onExecuteNext);
}

void TaskChainWidget::updateBottomBar()
{
    int idx = currentChainIndex();
    QString chainName = (idx >= 0 && idx < m_chains.size()) ? m_chains[idx].name : QString();
    QString taskName;
    QString groupName;

    // 获取当前选中的任务名
    auto *taskItem = m_taskList->currentItem();
    if (taskItem) {
        int taskIdx = taskItem->data(Qt::UserRole).toInt();
        if (idx >= 0 && idx < m_chains.size() && taskIdx >= 0 && taskIdx < m_chains[idx].tasks.size()) {
            QJsonObject task = m_chains[idx].tasks[taskIdx].toObject();
            taskName = task.value("name").toString();
        }
    }

    // 获取当前选中的组名
    auto *groupItem = m_groupList->currentItem();
    if (groupItem) {
        int groupIdx = groupItem->data(Qt::UserRole).toInt();
        QJsonArray groups = currentTaskGroups();
        if (groupIdx >= 0 && groupIdx < groups.size()) {
            QJsonObject group = groups[groupIdx].toObject();
            groupName = group.value("name").toString();
        }
    }

    // 状态文字
    QString statusText;
    switch (m_execStatus) {
    case NotStarted:  statusText = QStringLiteral("未开始"); break;
    case Running:     statusText = QStringLiteral("执行中"); break;
    case Suspended:   statusText = QStringLiteral("暂停中"); break;
    case Completed:   statusText = QStringLiteral("已完成"); break;
    }

    // 组装信息
    QStringList parts;
    if (!chainName.isEmpty()) parts << QStringLiteral("任务链: %1 [%2]").arg(chainName, statusText);
    if (!taskName.isEmpty())   parts << QStringLiteral("任务: %1").arg(taskName);
    if (!groupName.isEmpty()) parts << QStringLiteral("组: %1").arg(groupName);
    if (parts.isEmpty()) parts << QStringLiteral("未选择任务链");

    m_bottomInfoLabel->setText(parts.join("  |  "));

    // 按钮互斥逻辑
    if (chainName.isEmpty()) {
        // 无任务链
        m_suspendResumeBtn->setText(QStringLiteral("暂停任务链"));
        m_suspendResumeBtn->setEnabled(false);
        m_cancelBtn->setEnabled(false);
        m_executeNextBtn->setEnabled(false);
    } else {
        m_executeNextBtn->setEnabled(true);

        switch (m_execStatus) {
        case NotStarted:
            m_suspendResumeBtn->setText(QStringLiteral("暂停任务链"));
            m_suspendResumeBtn->setEnabled(false);
            m_cancelBtn->setEnabled(false);
            break;
        case Running:
            m_suspendResumeBtn->setText(QStringLiteral("继续任务链"));
            m_suspendResumeBtn->setEnabled(true);
            m_cancelBtn->setEnabled(true);
            break;
        case Suspended:
            m_suspendResumeBtn->setText(QStringLiteral("继续任务链"));
            m_suspendResumeBtn->setEnabled(true);
            m_cancelBtn->setEnabled(true);
            break;
        case Completed:
            m_suspendResumeBtn->setText(QStringLiteral("暂停任务链"));
            m_suspendResumeBtn->setEnabled(false);
            m_cancelBtn->setEnabled(false);
            break;
        }
    }
}

void TaskChainWidget::updateTaskSelectBtn()
{
    if (!m_taskSelectAllBtn) return;
    bool allChecked = true;
    for (int i = 0; i < m_taskList->count(); ++i) {
        auto *widget = m_taskList->itemWidget(m_taskList->item(i));
        if (widget) {
            auto *cb = widget->findChild<QCheckBox*>();
            if (cb && cb->isChecked()) continue;
        }
        allChecked = false;
        break;
    }
    if (m_taskList->count() == 0 || !allChecked) {
        m_taskSelectAllBtn->setText(QStringLiteral("全选任务"));
    } else {
        m_taskSelectAllBtn->setText(QStringLiteral("取消选中"));
    }
}

void TaskChainWidget::updateGroupSelectBtn()
{
    if (!m_groupSelectAllBtn) return;
    bool allChecked = true;
    for (int i = 0; i < m_groupList->count(); ++i) {
        auto *widget = m_groupList->itemWidget(m_groupList->item(i));
        if (widget) {
            auto *cb = widget->findChild<QCheckBox*>();
            if (cb && cb->isChecked()) continue;
        }
        allChecked = false;
        break;
    }
    if (m_groupList->count() == 0 || !allChecked) {
        m_groupSelectAllBtn->setText(QStringLiteral("全选组"));
    } else {
        m_groupSelectAllBtn->setText(QStringLiteral("取消选中"));
    }
}

QJsonObject TaskChainWidget::exportChainToJson(const ChainData &chain, bool onlyChecked) const
{
    QJsonObject root;
    root["task_name"] = chain.name;

    QJsonArray subTaskInfo;
    for (int i = 0; i < chain.tasks.size(); ++i) {
        QJsonObject taskObj = chain.tasks[i].toObject();
        bool enable = taskObj.contains("enable") ? taskObj.value("enable").toBool() : taskObj.value("checked").toBool();

        if (onlyChecked && !enable) continue;

        QJsonObject subTask;

        QString subTaskName = taskObj.value("sub_task_name").toString();
        if (subTaskName.isEmpty()) subTaskName = taskObj.value("name").toString();
        if (subTaskName.isEmpty()) subTaskName = QString("sub_task_%1").arg(i + 1);

        subTask["sub_task_name"] = subTaskName;
        subTask["enable"]        = enable;

        QJsonArray groupInfo;
        QJsonArray rawGroups = taskObj.value("group_info").isArray()
            ? taskObj.value("group_info").toArray()
            : taskObj.value("groups").toArray();

        for (int g = 0; g < rawGroups.size(); ++g) {
            QJsonObject rawGroup = rawGroups[g].toObject();
            bool groupEnable = rawGroup.contains("enable") ? rawGroup.value("enable").toBool() : rawGroup.value("checked").toBool();

            if (onlyChecked && !groupEnable) continue;

            QJsonObject group;

            QString groupName = rawGroup.value("group_name").toString();
            if (groupName.isEmpty()) groupName = rawGroup.value("name").toString();
            if (groupName.isEmpty()) groupName = QString("group_%1").arg(g + 1);

            group["group_name"] = groupName;
            group["enable"]     = groupEnable;

            QJsonArray actionsOut;
            QJsonArray rawActions = rawGroup.value("actions").toArray();
            for (int a = 0; a < rawActions.size(); ++a) {
                QJsonObject rawAction = rawActions[a].toObject();
                QJsonObject action;

                QString name = rawAction.value("name").toString();
                if (name.isEmpty()) name = rawAction.value("action_desc").toString();

                QString actionType = rawAction.value("action_type").toString();
                if (actionType.isEmpty()) actionType = actionTypeFromName(name);

                action["action_type"] = actionType;
                action["action_desc"] = name.isEmpty() ? actionDescFromType(actionType) : name;

                QJsonObject params = rawAction.value("params").toObject();

                if (actionType == "0") {
                    // 路径导航: 纯净字段 target_station (如 "LM3")
                    QString station = params.value("target_station").toString();
                    if (station.isEmpty()) station = rawAction.value("target_station").toString();
                    if (station.isEmpty()) station = params.value("target").toString();
                    if (station.isEmpty()) station = rawAction.value("target").toString();
                    if (station.isEmpty() || station == QStringLiteral("未知")) {
                        station = !m_stations.isEmpty() ? m_stations[0].name : "LM3";
                    }
                    action["target_station"] = station;
                } else if (actionType == "1") {
                    // 平动
                    action["direction"] = params.contains("direction") ? params.value("direction").toString() : rawAction.value("direction").toString("前进");
                    action["distance"]  = params.contains("distance") ? params.value("distance").toDouble() : rawAction.value("distance").toDouble(1.0);
                    action["speed"]     = params.contains("speed") ? params.value("speed").toDouble() : rawAction.value("speed").toDouble(0.2);
                } else if (actionType == "2") {
                    // 转动
                    action["direction"] = params.contains("direction") ? params.value("direction").toString() : rawAction.value("direction").toString("顺时针(右转)");
                    action["angle"]     = params.contains("angle") ? params.value("angle").toDouble() : rawAction.value("angle").toDouble(90.0);
                    action["speed"]     = params.contains("speed") ? params.value("speed").toDouble() : rawAction.value("speed").toDouble(0.5);
                } else if (actionType == "3") {
                    // 播放音频
                    QString audioName = params.value("audio_name").toString();
                    if (audioName.isEmpty()) audioName = params.value("audioName").toString();
                    if (audioName.isEmpty()) audioName = rawAction.value("audio_name").toString();
                    if (audioName.isEmpty()) audioName = rawAction.value("audioName").toString();
                    action["audio_name"] = audioName;
                    action["loop"] = params.contains("loop") ? params.value("loop").toString() : rawAction.value("loop").toString("否");
                } else if (actionType == "4") {
                    // 顶升
                    action["operation"] = params.contains("operation") ? params.value("operation").toString() : rawAction.value("operation").toString("顶起");
                    action["height"]    = params.contains("height") ? params.value("height").toDouble() : rawAction.value("height").toDouble(0.05);
                } else if (actionType == "5") {
                    // 停止播放音频
                    QString audioName = params.value("audioName").toString();
                    if (audioName.isEmpty()) audioName = params.value("audio_name").toString();
                    if (audioName.isEmpty()) audioName = rawAction.value("audio_name").toString();
                    if (audioName.isEmpty()) audioName = rawAction.value("audioName").toString();
                    action["audio_name"] = audioName;
                } else {
                    for (auto it = params.begin(); it != params.end(); ++it) {
                        if (it.key() != "checked" && it.key() != "target" && it.key() != "id") {
                            action[it.key()] = it.value();
                        }
                    }
                }

                actionsOut.append(action);
            }

            group["actions"] = actionsOut;
            groupInfo.append(group);
        }

        subTask["group_info"] = groupInfo;
        subTaskInfo.append(subTask);
    }

    root["sub_task_info"] = subTaskInfo;
    return root;
}

TaskChainWidget::ChainData TaskChainWidget::importChainFromJson(const QJsonObject &json) const
{
    ChainData chain;
    chain.name = json.value("task_name").toString();
    if (chain.name.isEmpty()) chain.name = json.value("name").toString();

    QJsonArray subTaskInfo = json.value("sub_task_info").isArray()
        ? json.value("sub_task_info").toArray()
        : json.value("tasks").toArray();

    for (int i = 0; i < subTaskInfo.size(); ++i) {
        QJsonObject rawSub = subTaskInfo[i].toObject();
        QJsonObject task;

        QString subTaskName = rawSub.value("sub_task_name").toString();
        if (subTaskName.isEmpty()) subTaskName = rawSub.value("name").toString();
        if (subTaskName.isEmpty()) subTaskName = QString("sub_task_%1").arg(i + 1);

        bool enable = rawSub.contains("enable") ? rawSub.value("enable").toBool() : rawSub.value("checked").toBool();

        task["sub_task_name"] = subTaskName;
        task["name"]          = subTaskName;
        task["enable"]        = enable;
        task["checked"]       = enable;
        task["note"]          = rawSub.value("note").toString();

        QJsonArray groupInfo = rawSub.value("group_info").isArray()
            ? rawSub.value("group_info").toArray()
            : rawSub.value("groups").toArray();

        QJsonArray outGroups;
        for (int g = 0; g < groupInfo.size(); ++g) {
            QJsonObject rawGroup = groupInfo[g].toObject();
            QJsonObject group;

            QString groupName = rawGroup.value("group_name").toString();
            if (groupName.isEmpty()) groupName = rawGroup.value("name").toString();
            if (groupName.isEmpty()) groupName = QString("group_%1").arg(g + 1);

            bool groupEnable = rawGroup.contains("enable") ? rawGroup.value("enable").toBool() : rawGroup.value("checked").toBool();

            group["group_name"] = groupName;
            group["name"]       = groupName;
            group["enable"]     = groupEnable;
            group["checked"]    = groupEnable;

            QJsonArray actionsIn = rawGroup.value("actions").toArray();
            QJsonArray outActions;
            for (int a = 0; a < actionsIn.size(); ++a) {
                QJsonObject rawAction = actionsIn[a].toObject();
                QJsonObject action;

                QString actionDesc = rawAction.value("action_desc").toString();
                if (actionDesc.isEmpty()) actionDesc = rawAction.value("name").toString();

                QString actionType = rawAction.value("action_type").toString();
                if (actionType.isEmpty()) actionType = actionTypeFromName(actionDesc);
                if (actionDesc.isEmpty()) actionDesc = actionDescFromType(actionType);

                action["name"]        = actionDesc;
                action["action_desc"] = actionDesc;
                action["action_type"] = actionType;

                QJsonObject params;
                for (auto it = rawAction.begin(); it != rawAction.end(); ++it) {
                    if (it.key() != "name" && it.key() != "action_desc" && it.key() != "action_type") {
                        params[it.key()] = it.value();
                        if (it.key() == "target_station") params["target"] = it.value();
                        if (it.key() == "audio_name") params["audioName"] = it.value();
                    }
                }
                action["params"] = params;
                outActions.append(action);
            }
            group["actions"] = outActions;
            if (!outActions.isEmpty()) {
                group["actionType"] = outActions[0].toObject().value("name").toString();
            }
            outGroups.append(group);
        }

        task["group_info"] = outGroups;
        task["groups"]     = outGroups;
        chain.tasks.append(task);
    }

    return chain;
}

void TaskChainWidget::executeSingleAction(const QJsonObject &action)
{
    QString type = action.value("action_type").toString();
    int actionId = static_cast<int>(QDateTime::currentMSecsSinceEpoch() % 1000000 + 1);

    if (type == "0") {
        // 路径导航: set_robot_fix_path / set_third_fix_path
        QString targetStation = action.value("target_station").toString();
        if (targetStation.isEmpty()) targetStation = "LM3";

        QJsonObject fixPath;
        QJsonObject d;
        d["action_id"] = actionId;
        d["mapcode"]   = AppConfig::instance().currentDevice().mapName;

        QJsonArray paths;
        QJsonObject p;
        p["start_node"] = 1;
        bool ok = false;
        int goalNodeInt = targetStation.toInt(&ok);
        if (ok) {
            p["goal_node"] = goalNodeInt;
        } else {
            p["goal_node"] = targetStation;
        }
        p["index"]      = 1;
        double navSpd   = action.value("speed").toDouble(0.5);
        p["path_speed"] = MotionConfigManager::instance().clampLinearSpeed(navSpd <= 0.01 ? MotionConfigManager::instance().maxSpeed() : navSpd);
        paths.append(p);
        d["robot_paths"] = paths;
        fixPath["data"] = d;

        ApiService::instance().setRobotFixPath(fixPath, [this, targetStation](int code, const QJsonObject &resp) {
            qDebug() << "[TaskChain] setRobotFixPath to" << targetStation << "code:" << code << resp;
        });
        updateStatus(QStringLiteral("正在向控制器下发路径导航: 目标站点 %1").arg(targetStation));
    } else if (type == "1" || type == "6") {
        // 平动: 按照文档规范调用控制器 set_action_control (action_type: 6)
        QString dir = action.value("direction").toString("前进");
        double dist = action.value("distance").toDouble(1.0);
        double spd  = MotionConfigManager::instance().clampLinearSpeed(action.value("speed").toDouble(0.2));

        double vx = 0.0, vy = 0.0, sendDist = dist;
        if (dir == "前进") {
            vx = spd; vy = 0.0; sendDist = std::abs(dist);
        } else if (dir == "后退") {
            vx = spd; vy = 0.0; sendDist = -std::abs(dist);
        } else if (dir == "左移") {
            vx = 0.0; vy = spd; sendDist = std::abs(dist);
        } else if (dir == "右移") {
            vx = 0.0; vy = spd; sendDist = -std::abs(dist);
        }

        ApiService::instance().setLinearMotion(vx, vy, sendDist, actionId, [this, dir, dist](int code, const QJsonObject &resp) {
            qDebug() << "[TaskChain] setLinearMotion (" << dir << dist << "m) code:" << code << resp;
        });
        updateStatus(QStringLiteral("正在向控制器下发平动动作: %1 %2m (速度 %3m/s)").arg(dir).arg(dist).arg(spd));
    } else if (type == "2" || type == "5") {
        // 转动: 按照文档规范调用控制器 set_action_control (action_type: 5)
        QString dir = action.value("direction").toString("顺时针(右转)");
        double deg  = action.value("angle").toDouble(90.0);
        double spd  = MotionConfigManager::instance().clampAngularSpeedRad(action.value("speed").toDouble(0.5));

        double angleRad = deg * M_PI / 180.0;
        if (dir.contains("顺时针") || dir.contains("右转")) {
            angleRad = -std::abs(angleRad);
        } else {
            angleRad = std::abs(angleRad);
        }

        ApiService::instance().setRotateMotion(angleRad, spd, actionId, [this, dir, deg](int code, const QJsonObject &resp) {
            qDebug() << "[TaskChain] setRotateMotion (" << dir << deg << "°) code:" << code << resp;
        });
        updateStatus(QStringLiteral("正在向控制器下发转动动作: %1 %2° (角速度 %3rad/s)").arg(dir).arg(deg).arg(spd));
    } else if (type == "4") {
        // 顶升: 按照文档规范调用控制器 set_action_control (action_type: 3)
        QString op = action.value("operation").toString("顶起");
        double h   = action.value("height").toDouble(0.05);
        int actVal = (op == "顶起") ? 1 : 0;

        ApiService::instance().setJackControl(actVal, h, actionId, [this, op](int code, const QJsonObject &resp) {
            qDebug() << "[TaskChain] setJackControl (" << op << ") code:" << code << resp;
        });
        updateStatus(QStringLiteral("正在向控制器下发顶升动作: %1 (高度 %2m)").arg(op).arg(h));
    }
}

void TaskChainWidget::onSuspendResume()
{
    if (m_execStatus == Running) {
        m_execStatus = Suspended;
        // PUT /task/chain/control?command=pause navigation
        ApiService::instance().controlTaskChain(QStringLiteral("pause navigation"), [this](int code, const QJsonObject &resp) {
            if (code != 200) {
                Toast::error(this, QStringLiteral("暂停失败(%1): %2")
                    .arg(code).arg(resp.value("msg").toString(QStringLiteral("请求失败"))));
            }
        });
        updateStatus(QStringLiteral("任务链已向服务端发送暂停指令"));
        Toast::info(this, QStringLiteral("任务链已暂停"));
    } else if (m_execStatus == Suspended) {
        m_execStatus = Running;
        // PUT /task/chain/control?command=continue navigation
        ApiService::instance().controlTaskChain(QStringLiteral("continue navigation"), [this](int code, const QJsonObject &resp) {
            if (code != 200) {
                Toast::error(this, QStringLiteral("继续失败(%1): %2")
                    .arg(code).arg(resp.value("msg").toString(QStringLiteral("请求失败"))));
            }
        });
        updateStatus(QStringLiteral("任务链已向服务端发送继续指令"));
        Toast::success(this, QStringLiteral("任务链已继续"));
    }
    updateBottomBar();
}

void TaskChainWidget::onCancelChain()
{
    m_execStatus = NotStarted;
    // PUT /task/chain/control?command=stop navigation
    ApiService::instance().controlTaskChain(QStringLiteral("stop navigation"), [this](int code, const QJsonObject &resp) {
        if (code != 200) {
            Toast::error(this, QStringLiteral("结束失败(%1): %2")
                .arg(code).arg(resp.value("msg").toString(QStringLiteral("请求失败"))));
        }
    });
    updateStatus(QStringLiteral("任务链已向服务端发送结束指令"));
    Toast::info(this, QStringLiteral("任务链已取消"));
    updateBottomBar();
}

void TaskChainWidget::onExecuteNext()
{
    int idx = currentChainIndex();
    if (idx < 0) return;

    QJsonObject chainJson = exportChainToJson(m_chains[idx], false);
    QJsonArray subTasks = chainJson.value("sub_task_info").toArray();

    // 寻找可执行动作并调用控制器接口下发
    bool executed = false;
    for (const auto &subVal : subTasks) {
        QJsonObject subObj = subVal.toObject();
        QJsonArray groups = subObj.value("group_info").toArray();
        for (const auto &gVal : groups) {
            QJsonObject gObj = gVal.toObject();
            QJsonArray acts = gObj.value("actions").toArray();
            if (!acts.isEmpty()) {
                executeSingleAction(acts[0].toObject());
                executed = true;
                break;
            }
        }
        if (executed) break;
    }

    m_execStatus = Running;
    updateStatus(QStringLiteral("已向控制器触发 Next 任务动作"));
    updateBottomBar();
}