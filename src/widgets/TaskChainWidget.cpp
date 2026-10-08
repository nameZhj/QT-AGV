#include "TaskChainWidget.h"
#include "core/Toast.h"
#include "core/AppConfig.h"
#include "core/ApiService.h"
#include "core/MotionConfigManager.h"
#include "core/MqttService.h"
#include "core/TaskChainManager.h"
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
#include <QPainter>
#include <QDateTime>
#include <QCoreApplication>

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

// 顶升线速度上限 (m/s)
static constexpr double kMaxLiftLinearSpeed = 0.1;
// 顶升高度上限 (m)
static constexpr double kMaxLiftHeight = 0.15;

static const char *S_LIST_STYLE =
    "QListWidget { background:#181d23; border:1px solid #2a3038; border-radius:4px; outline:none; }"
    "QListWidget::item { color:#c0c8d0; padding:6px 10px; border-bottom:1px solid #22272d; }"
    "QListWidget::item:selected, QListWidget::item:selected:!active { background:#2a5d8f; color:#fff; }"
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
    if (name == "路径导航") return "99";
    if (name == "顶升") return "3";          // 顶升上升或下降
    if (name == "转动") return "5";
    if (name == "平动") return "6";
    if (name == "播放音频") return "10";
    if (name == "停止播放音频") return "11";
    return "-1";
}

static QString actionDescFromType(const QString &type) {
    if (type == "99") return "路径导航";
    if (type == "3" ) return "顶升";     
    if (type == "5" || type == "12") return "转动";     
    if (type == "6" ) return "平动";  
    if (type == "10") return "播放音频";
    if (type == "11") return "停止播放音频";
    return "路径导航";
}

// 旧版本动作编号归一化为当前编号, 导出与导入共用
// 当前: 99 路径导航 / 3 顶升 / 5(兼容12) 转动 / 6 平动 / 10 播放音频 / 11 停止播放音频
// 旧数据: 0 路径导航 / 1 平动 / 2 转动 / 4 顶升; 旧 3/5 为音频动作, 需按动作名称区分
static QString normalizeActionType(const QString &type, const QString &name) {
    if (type == "0") return "99";
    if (type == "1") return "6";
    if (type == "2" || type == "12") return "5";
    if (type == "4") return "3";
    if (type == "3" && name != "顶升") return "10";
    if (type == "5" && name == "停止播放音频") return "11";
    return type;
}

// 列表内所有项的自定义勾选框是否都被勾选 (空列表返回 false)
static bool allCheckBoxesChecked(QListWidget *list) {
    if (!list || list->count() == 0) return false;
    for (int i = 0; i < list->count(); ++i) {
        auto *widget = list->itemWidget(list->item(i));
        auto *cb = widget ? widget->findChild<QCheckBox*>() : nullptr;
        if (!cb || !cb->isChecked()) return false;
    }
    return true;
}

// 统一勾选/取消列表内所有项的自定义勾选框
static void setAllCheckBoxesChecked(QListWidget *list, bool checked) {
    if (!list) return;
    for (int i = 0; i < list->count(); ++i) {
        auto *widget = list->itemWidget(list->item(i));
        auto *cb = widget ? widget->findChild<QCheckBox*>() : nullptr;
        if (cb) cb->setChecked(checked);
    }
}

// 本地缓存文件: 与 exe 同级的 taskChainCache.json
static QString cacheFilePath() {
    return QCoreApplication::applicationDirPath() + "/taskChainCache.json";
}

// 本地任务链保存目录: 与 exe 同级的 task_chains 文件夹
static QString taskChainLocalDir() {
    QString dir = QCoreApplication::applicationDirPath() + "/task_chains";
    QDir().mkpath(dir);
    return dir;
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

// 可接收拖拽动作标签以及任务/组重新排序的通用列表控件
class DropTargetListWidget : public QListWidget
{
public:
    explicit DropTargetListWidget(QWidget *parent = nullptr) : QListWidget(parent)
    {
        setAcceptDrops(true);
        setDragDropMode(QAbstractItemView::DropOnly);
        setDefaultDropAction(Qt::MoveAction);
    }

    std::function<void(const QString &actionName, int row)> onActionDropped;
    std::function<void(int fromRow, int toRow)> onGroupReordered;
    std::function<void(int fromRow, int toRow)> onTaskReordered;

protected:
    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (event->mimeData()->hasFormat("application/x-taskchain-action") ||
            event->mimeData()->hasFormat("application/x-taskchain-group-reorder") ||
            event->mimeData()->hasFormat("application/x-taskchain-task-reorder"))
            event->acceptProposedAction();
        else
            event->ignore();
    }

    void dragMoveEvent(QDragMoveEvent *event) override
    {
        if (event->mimeData()->hasFormat("application/x-taskchain-action") ||
            event->mimeData()->hasFormat("application/x-taskchain-group-reorder") ||
            event->mimeData()->hasFormat("application/x-taskchain-task-reorder"))
            event->acceptProposedAction();
        else
            event->ignore();
    }

    void dropEvent(QDropEvent *event) override
    {
        if (event->mimeData()->hasFormat("application/x-taskchain-group-reorder")) {
            int fromRow = QString::fromUtf8(event->mimeData()->data("application/x-taskchain-group-reorder")).toInt();
            auto *dropItem = itemAt(event->position().toPoint());
            int toRow = dropItem ? row(dropItem) : count() - 1;
            if (onGroupReordered) onGroupReordered(fromRow, toRow);
            event->acceptProposedAction();
        } else if (event->mimeData()->hasFormat("application/x-taskchain-task-reorder")) {
            int fromRow = QString::fromUtf8(event->mimeData()->data("application/x-taskchain-task-reorder")).toInt();
            auto *dropItem = itemAt(event->position().toPoint());
            int toRow = dropItem ? row(dropItem) : count() - 1;
            if (onTaskReordered) onTaskReordered(fromRow, toRow);
            event->acceptProposedAction();
        } else if (event->mimeData()->hasFormat("application/x-taskchain-action")) {
            QString actionName = QString::fromUtf8(event->mimeData()->data("application/x-taskchain-action"));
            auto *dropItem = itemAt(event->position().toPoint());
            int dropRow = dropItem ? row(dropItem) : -1;
            if (onActionDropped) onActionDropped(actionName, dropRow);
            event->acceptProposedAction();
        } else {
            event->ignore();
        }
    }
};

// 通用卡片长按与拖拽重排序事件过滤器 (适用于任务卡片和组卡片)
class CardLongPressDragFilter : public QObject
{
public:
    CardLongPressDragFilter(const QString &mimeType, int index, QWidget *container)
        : QObject(container), m_mimeType(mimeType), m_index(index), m_container(container)
    {
        m_timer = new QTimer(this);
        m_timer->setSingleShot(true);
        connect(m_timer, &QTimer::timeout, this, &CardLongPressDragFilter::onLongPressTimeout);
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                // 点击在可交互控件上时，正常响应交互，不触发长按拖拽
                QWidget *child = m_container->childAt(me->pos());
                if (child && (qobject_cast<QAbstractButton*>(child) || qobject_cast<QTextEdit*>(child))) {
                    return QObject::eventFilter(watched, event);
                }
                m_pressPos = me->pos();
                m_longPressed = false;
                m_dragStarted = false;
                m_timer->start(300); // 300ms 判定为长按
            }
        } else if (event->type() == QEvent::MouseMove) {
            auto *me = static_cast<QMouseEvent*>(event);
            if (me->buttons() & Qt::LeftButton) {
                int dist = (me->pos() - m_pressPos).manhattanLength();
                if (dist > 8 && m_timer->isActive()) {
                    m_timer->stop();
                }
                if (m_longPressed && !m_dragStarted && dist > 3) {
                    m_dragStarted = true;
                    startDrag();
                    return true;
                }
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            m_timer->stop();
            if (m_longPressed && m_container) {
                m_container->setStyleSheet(m_origStyle);
                m_container->unsetCursor();
            }
            m_longPressed = false;
        }
        return QObject::eventFilter(watched, event);
    }

private:
    void onLongPressTimeout()
    {
        m_longPressed = true;
        if (m_container) {
            m_origStyle = m_container->styleSheet();
            m_container->setStyleSheet(
                "background: rgba(0, 188, 212, 0.16); border: 2px dashed #00bcd4; border-radius: 4px;");
            m_container->setCursor(Qt::ClosedHandCursor);
        }
    }

    void startDrag()
    {
        if (!m_container) return;
        auto *drag = new QDrag(m_container);
        auto *mime = new QMimeData;
        mime->setData(m_mimeType, QByteArray::number(m_index));
        drag->setMimeData(mime);

        QPixmap pix = m_container->grab();
        QPixmap transparentPix(pix.size());
        transparentPix.fill(Qt::transparent);
        QPainter p(&transparentPix);
        p.setOpacity(0.75);
        p.drawPixmap(0, 0, pix);
        p.end();

        drag->setPixmap(transparentPix);
        drag->setHotSpot(m_pressPos);
        drag->exec(Qt::MoveAction);

        if (m_container) {
            m_container->setStyleSheet(m_origStyle);
            m_container->unsetCursor();
        }
        m_longPressed = false;
        m_dragStarted = false;
    }

    QString m_mimeType;
    int m_index = 0;
    QWidget *m_container = nullptr;
    QTimer *m_timer = nullptr;
    QPoint m_pressPos;
    bool m_dragStarted = false;
    bool m_longPressed = false;
    QString m_origStyle;
};

// 覆盖在四面板编辑区上的半透明遮罩 (执行中锁定，拦截一切点击与拖拽操作)
class TaskChainLockOverlay : public QWidget
{
public:
    explicit TaskChainLockOverlay(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_StyledBackground, true);
        setAttribute(Qt::WA_Hover, true);
        setFocusPolicy(Qt::StrongFocus);
        setCursor(Qt::ForbiddenCursor);
    }

protected:
    void paintEvent(QPaintEvent *e) override
    {
        Q_UNUSED(e);
        QPainter p(this);
        // 轻柔通透的半透明锁定层，确保底层所有任务、组和参数清晰可辨
        p.fillRect(rect(), QColor(10, 14, 20, 45));
    }
    void mousePressEvent(QMouseEvent *e) override { e->accept(); }
    void mouseReleaseEvent(QMouseEvent *e) override { e->accept(); }
    void mouseDoubleClickEvent(QMouseEvent *e) override { e->accept(); }
    void mouseMoveEvent(QMouseEvent *e) override { e->accept(); }
    void wheelEvent(QWheelEvent *e) override { e->accept(); }
    void contextMenuEvent(QContextMenuEvent *e) override { e->accept(); }
    void dragEnterEvent(QDragEnterEvent *e) override { e->acceptProposedAction(); }
    void dragMoveEvent(QDragMoveEvent *e) override { e->acceptProposedAction(); }
    void dropEvent(QDropEvent *e) override { e->acceptProposedAction(); }
    void keyPressEvent(QKeyEvent *e) override { e->accept(); }
    void keyReleaseEvent(QKeyEvent *e) override { e->accept(); }
};

TaskChainWidget::TaskChainWidget(QWidget *parent)
    : QWidget(parent)
{
    initActionDefs();
    setupUI();

    // 数据变更后延迟写本地缓存, 避免连续编辑时频繁落盘
    m_cacheTimer = new QTimer(this);
    m_cacheTimer->setSingleShot(true);
    m_cacheTimer->setInterval(800);
    connect(m_cacheTimer, &QTimer::timeout, this, &TaskChainWidget::writeCache);
}

TaskChainWidget::~TaskChainWidget()
{
    // 释放 MQTT 订阅, 避免回调持有已销毁对象
    if (m_statusSubscribed) {
        MqttService::instance().unsubscribeTopic("/taskchain/status");
        m_statusSubscribed = false;
    }
    if (m_currentSubscribed) {
        MqttService::instance().unsubscribeTopic("/taskchain/currentTask");
        m_currentSubscribed = false;
    }

    // 本次运行点击过“保存”则保留缓存, 否则随程序退出清理
    if (m_didSave) {
        if (m_cacheTimer && m_cacheTimer->isActive()) {
            m_cacheTimer->stop();
            writeCache();
        }
    } else {
        QFile::remove(cacheFilePath());
    }
}

void TaskChainWidget::setupUI()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── 编辑区容器 (包含四面板分割器及半透明锁定遮罩) ──
    m_panelsContainer = new QWidget(this);
    m_panelsContainer->installEventFilter(this);
    auto *containerLayout = new QVBoxLayout(m_panelsContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);

    // 四面板分割器
    auto *splitter = new QSplitter(Qt::Horizontal, m_panelsContainer);
    containerLayout->addWidget(splitter, 1);

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

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);   // 缩窄
    splitter->setStretchFactor(2, 3);   // 加宽
    splitter->setStretchFactor(3, 2);   // 加宽
    splitter->setSizes({200, 160, 400, 280});

    // ── 创建覆盖在整个四面板编辑区之上的半透明锁定遮罩 ──
    auto *overlay = new TaskChainLockOverlay(m_panelsContainer);
    overlay->setObjectName("taskChainLockOverlay");
    overlay->setStyleSheet("background: transparent;");

    auto *overlayLayout = new QVBoxLayout(overlay);
    overlayLayout->setAlignment(Qt::AlignCenter);
    overlayLayout->setContentsMargins(0, 0, 0, 0);

    m_overlayTextLabel = new QLabel(QStringLiteral("任务执行中"), overlay);
    m_overlayTextLabel->setStyleSheet(
        "color: #38bdf8; "
        "font-size: 15px; "
        "font-weight: bold; "
        "letter-spacing: 1px; "
        "background: rgba(15, 23, 42, 0.85); "
        "border: 1px solid rgba(56, 189, 248, 0.45); "
        "border-radius: 18px; "
        "padding: 6px 22px;"
    );
    m_overlayTextLabel->setAlignment(Qt::AlignCenter);
    m_overlayTextLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    overlayLayout->addWidget(m_overlayTextLabel);

    overlay->setVisible(false);
    m_lockOverlay = overlay;

    root->addWidget(m_panelsContainer, 1);

    // 初始化可拖拽动作标签
    refreshActionTags();

    // 底部公共状态栏 (独立于编辑区容器之外，完全不被遮罩覆盖)
    setupBottomBar(root);

    // 监听全局任务链状态管理器信号 (解决页面切换导致的状态断档与状态错误)
    connect(&TaskChainManager::instance(), &TaskChainManager::chainStatusChanged, this, [this](bool working, bool paused) {
        m_chainWorking = working;
        m_chainPaused  = paused;
        if (!working) {
            m_execStatus = NotStarted;
            setListsLocked(false);
            updateStatus(QStringLiteral("任务链已结束, 编辑区域已解锁"));
        } else if (paused) {
            m_execStatus = Suspended;
            setListsLocked(true);
        } else {
            m_execStatus = Running;
            setListsLocked(true);
        }

        if (m_overlayTextLabel) {
            m_overlayTextLabel->setText(m_chainPaused ? QStringLiteral("任务链已暂停") : QStringLiteral("任务执行中"));
        }
        updateBottomBar();
    });

    connect(&TaskChainManager::instance(), &TaskChainManager::currentTaskChanged, this, [this](const QString &task, const QString &group) {
        m_currentTaskName  = task;
        m_currentGroupName = group;
        applyRuntimeSelection();
        updateBottomBar();
    });

    updateBottomBar();
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

    // 任务列表 (可长按卡片直接拖动排序)
    auto *taskDropList = new DropTargetListWidget;
    taskDropList->setStyleSheet(S_LIST_STYLE);
    m_taskList = taskDropList;
    taskDropList->onTaskReordered = [this](int fromRow, int toRow) {
        reorderTask(fromRow, toRow);
    };
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
            // 目标站点默认取路网第一个点; 路网未加载时留空, 由用户在参数中指定
            QString stId;
            QString stName;
            if (!m_stations.isEmpty()) {
                stId   = m_stations[0].id;
                stName = m_stations[0].name;
            }
            action["target_station"] = stId;
            params["target_station"] = stId;
            params["target"]         = stName;
        } else if (actionName == "平动") {
            action["distance"]  = 1.0;
            action["vx"]        = 0.2;
            action["vy"]        = 0.0;
            params["distance"]  = 1.0;
            params["vx"]        = 0.2;
            params["vy"]        = 0.0;
        } else if (actionName == "转动") {
            // 旋转方向由旋转角度正负决定
            action["rotate_angle"] = -90.0;
            action["rotate_speed"] = 30.0;
            params["angle"]        = -90.0;
            params["speed"]        = 30.0;
        }
        action["params"] = params;
        actions.append(action);
        group["actions"] = actions;
        groups[groupIdx] = group;
        setCurrentTaskGroups(groups);

        refreshGroupList();
        updateStatus(QStringLiteral("已添加动作: %1").arg(actionName));
    };
    dropList->onGroupReordered = [this](int fromRow, int toRow) {
        reorderGroup(fromRow, toRow);
    };
    layout->addWidget(m_groupList, 1);

    // 发送按钮
    auto *btnRow2 = new QHBoxLayout;
    btnRow2->setSpacing(6);
    btnRow2->addStretch();
    m_loopCheck = new QCheckBox(QStringLiteral("循环"));
    m_loopCheck->setToolTip(QStringLiteral("勾选后将以循环模式执行下发任务"));
    m_groupSendSelBtn = makeBtn(QStringLiteral("发送勾选的任务"));
    m_groupSendAllBtn = makeBtn(QStringLiteral("发送全部任务"));
    btnRow2->addWidget(m_loopCheck);
    btnRow2->addWidget(m_groupSendSelBtn);
    btnRow2->addWidget(m_groupSendAllBtn);
    layout->addLayout(btnRow2);

    m_groupHintLabel = new QLabel(QStringLiteral("拖拽标签到组中添加动作；长按组卡片可直接拖动重新排序"));
    m_groupHintLabel->setStyleSheet("color:#666; font-size:11px; padding:2px;");
    layout->addWidget(m_groupHintLabel);

    // 连接
    connect(m_groupAddBtn,       &QPushButton::clicked, this, &TaskChainWidget::onGroupAdd);
    connect(m_groupInsertBtn,    &QPushButton::clicked, this, &TaskChainWidget::onGroupInsert);
    connect(m_groupDeleteBtn,     &QPushButton::clicked, this, &TaskChainWidget::onGroupDelete);
    connect(m_groupSelectAllBtn, &QPushButton::clicked, this, &TaskChainWidget::onGroupSelectAll);
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
    scheduleCacheWrite();
    if (m_listLocked) {
        installLockFilter(m_chainList, true);
    }
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

    // 同步列表当前项: 任务/组面板通过 currentChainIndex() 读取选中项
    if (m_chainList->currentRow() != index) {
        m_chainList->blockSignals(true);
        m_chainList->setCurrentRow(index);
        m_chainList->blockSignals(false);
    }

    refreshTaskList();
    refreshGroupList();
    updateBottomBar();
}

int TaskChainWidget::currentChainIndex() const
{
    if (m_chainList) {
        auto *item = m_chainList->currentItem();
        if (item) return item->data(Qt::UserRole).toInt();
        if (m_currentChainIndex >= 0 && m_currentChainIndex < m_chains.size()) {
            return m_currentChainIndex;
        }
    }
    return -1;
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
    // 两个键保持同步, 避免下发时读到创建时的旧快照
    task["group_info"] = groups;
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
            row->setObjectName("taskCardRoot");
            row->setStyleSheet("#taskCardRoot { background: transparent; border: 1px solid transparent; border-radius: 4px; }");
            row->setAcceptDrops(true);
            row->installEventFilter(new CardLongPressDragFilter("application/x-taskchain-task-reorder", i, row));
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
            nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
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
                    scheduleCacheWrite();
                });

            // 备注修改时保存到数据 + 动态调整高度
            connect(noteEdit, &QTextEdit::textChanged, this,
                [this, idx, i, noteEdit, item, nameLabel]() {
                    if (idx < 0 || idx >= m_chains.size()) return;
                    if (i < 0 || i >= m_chains[idx].tasks.size()) return;
                    QJsonObject task = m_chains[idx].tasks[i].toObject();
                    task["note"] = noteEdit->toPlainText();
                    m_chains[idx].tasks[i] = task;
                    scheduleCacheWrite();

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
    scheduleCacheWrite();
    if (m_listLocked) {
        installLockFilter(m_taskList, true);
    }
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
    scheduleCacheWrite();
    if (m_listLocked) {
        installLockFilter(m_groupList, true);
    }
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
    container->setObjectName("groupCardRoot");
    container->setStyleSheet("#groupCardRoot { background: transparent; border: 1px solid transparent; border-radius: 4px; }");
    container->setAcceptDrops(true);
    container->installEventFilter(new CardLongPressDragFilter("application/x-taskchain-group-reorder", groupIndex, container));
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
    nameLbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    nameLbl->setStyleSheet("color:#00bcd4; font-size:12px; font-weight:bold; background:transparent;");
    headerRow->addWidget(nameLbl);

    if (!actionType.isEmpty()) {
        auto *typeLbl = new QLabel(QStringLiteral("  [%1]").arg(actionType));
        typeLbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);
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
            scheduleCacheWrite();
        });

    // 如果没有标签, 显示占位提示
    if (actionType.isEmpty()) {
        auto *hint = new QLabel(QStringLiteral("  拖拽标签到此组..."));
        hint->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        hint->setStyleSheet("color:#555; font-size:11px; background:transparent; padding:8px 4px;");
        hint->setMinimumHeight(60);
        vbox->addWidget(hint);
    } else {
        // 显示每个动作标签 + 参数区域 + 删除按钮
        for (int a = 0; a < actions.size(); ++a) {
            QJsonObject action = actions[a].toObject();
            QString actionName = action.value("name").toString();
            if (actionName.isEmpty()) actionName = action.value("action_desc").toString();
            QJsonObject params = action.value("params").toObject();

            // 为“路径导航”解析站点显示名称 (target_station 存的是路网点 id)
            QString targetId = params.value("target_station").toString();
            if (targetId.isEmpty()) targetId = params.value("target").toString();
            if (targetId.isEmpty()) targetId = action.value("target_station").toString();
            const QString displayTarget = targetId.isEmpty() ? QStringLiteral("未知") : stationName(targetId);

            QString displayName = actionName;
            if (actionName == "路径导航" || action.value("action_desc").toString() == "路径导航") {
                displayName = QStringLiteral("路径导航 -> %1").arg(displayTarget);
            }

            // 动作行: 序号 + 名称 + 删除标签按钮
            auto *actRow = new QHBoxLayout;
            actRow->setSpacing(4);

            auto *actLabel = new QLabel(QStringLiteral("%1. %2").arg(a + 1).arg(displayName));
            actLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            actLabel->setStyleSheet("color:#c0c8d0; font-size:11px; background:transparent; padding:2px 8px;");
            actLabel->setMinimumHeight(24);
            actRow->addWidget(actLabel, 1);

            // 删除标签按钮
            auto *delTagBtn = new QToolButton;
            delTagBtn->setText("✕");
            delTagBtn->setToolTip(QStringLiteral("删除标签"));
            delTagBtn->setAutoRaise(true);
            delTagBtn->setFixedSize(22, 22);
            delTagBtn->setStyleSheet("QToolButton{background:transparent;border:none;color:#8899aa;font-size:12px;}QToolButton:hover{color:#e05555;}");
            actRow->addWidget(delTagBtn);

            vbox->addLayout(actRow);

            // 参数摘要区域 (点击弹出定义对话框)
            // 站点字段显示路网点名称; 内部/重复字段不展示
            QString paramSummary;
            {
                QStringList parts;
                bool audioShown = false;
                if (actionName == QStringLiteral("停止播放音频") || (findActionDef(actionName) && findActionDef(actionName)->params.isEmpty())) {
                    paramSummary = QStringLiteral("无需设置参数");
                } else {
                    for (auto it = params.begin(); it != params.end(); ++it) {
                        const QString key = it.key();
                        if (key == "target" || key == "checked" || key == "id")
                            continue;
                        if (key == "audioName" || key == "audio_name") {
                            // 音频名称: 显示已选音频 (两种字段名只显示一次)
                            if (audioShown) continue;
                            const QString audio = it.value().toString();
                            if (audio.isEmpty()) continue;
                            audioShown = true;
                            parts << QStringLiteral("音频名称: %1").arg(audioDisplayName(audio));
                            continue;
                        }
                        QString value = it.value().toVariant().toString();
                        if (key == "loop") {
                            // 循环播放: 数据为 yes / no, 界面显示 是 / 否
                            parts << QStringLiteral("循环播放: %1")
                                         .arg(value.compare("yes", Qt::CaseInsensitive) == 0 ? "是" : "否");
                            continue;
                        }
                        if (key == "target_station") {
                            parts << QStringLiteral("目标站点: %1").arg(stationName(value));
                            continue;
                        }
                        parts << QStringLiteral("%1: %2").arg(key, value);
                    }
                    paramSummary = parts.isEmpty() ? QStringLiteral("未设置 — 点击编辑") : parts.join(", ");
                }
            }

            auto *paramBtn = new QPushButton(paramSummary);
            bool isNoParam = (actionName == QStringLiteral("停止播放音频") || (findActionDef(actionName) && findActionDef(actionName)->params.isEmpty()));
            if (isNoParam) {
                paramBtn->setStyleSheet(
                    "QPushButton{text-align:left; background:#1e2329; border:1px solid #252b33; "
                    "border-radius:3px; padding:6px 10px; color:#66707a; font-size:11px; margin-left:8px;}");
                paramBtn->setToolTip(QStringLiteral("此动作直接下发 action_type，无需额外设置参数"));
                paramBtn->setEnabled(false);
            } else {
                paramBtn->setStyleSheet(
                    "QPushButton{text-align:left; background:#22272d; border:1px solid #2a3038; "
                    "border-radius:3px; padding:6px 10px; color:#999; font-size:11px; margin-left:8px;}"
                    "QPushButton:hover{border-color:#00bcd4; color:#c0c8d0;}");
                paramBtn->setToolTip(QStringLiteral("点击编辑参数"));
            }
            paramBtn->setMinimumHeight(32);

            // 参数按钮 → 打开参数定义对话框
            if (!isNoParam) {
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
            }

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

    // 2. 平动 (参数与“地图与控制-移动-平动”一致: 距离 / vx / vy)
    def = {"平动", "motion"};
    def.params = {
        {"distance", "设置距离：", "double", {}, "m", 1.0},
        {"vx", "设置 vx 速度：", "double", {}, "m/s", 0.2},
        {"vy", "设置 vy 速度：", "double", {}, "m/s", 0.0},
    };
    m_actionDefs.append(def);

    // 3. 转动 (旋转方向由旋转角度的正负决定, 不单独设置方向)
    def = {"转动", "motion"};
    def.params = {
        {"angle", "设置旋转角度：", "double", {}, "°", -90.0},
        {"speed", "设置角速度：", "double", {}, "°/s", 30.0},
    };
    m_actionDefs.append(def);

    // 4. 播放音频
    def = {"播放音频", "audio"};
    def.params = {
        {"audioName", "音频名称:", "audio_combo", {}, "选择音频文件", 0},
        {"loop", "循环播放:", "combo", {"否", "是"}, "", 0},
    };
    m_actionDefs.append(def);

    // 5. 顶升 (传目标高度 lift_height 与升降线速度 lift_linear, 高度 0-0.15 m, 速度上限 0.1 m/s)
    def = {"顶升", "jack"};
    def.params = {
        {"lift_height", "目标顶升高度:", "double", {}, "m", 0.05},
        {"lift_linear", "顶升线速度:", "double", {}, "m/s", 0.05},
    };
    m_actionDefs.append(def);

    // 6. 停止播放音频 (无需选择音频，直接传 action_type)
    def = {"停止播放音频", "audio"};
    def.params = {};
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
            if (param.key == "lift_height") {
                // 顶升高度范围 0 - 0.15 m
                spin->setRange(0.0, kMaxLiftHeight);
                spin->setSingleStep(0.01);
                spin->setValue(qBound(0.0, existingVal.toDouble(param.defaultValue), kMaxLiftHeight));
                spin->setToolTip(QStringLiteral("顶升高度上限: %1 m").arg(kMaxLiftHeight));
            } else if (param.key == "lift_linear") {
                // 顶升线速度非负, 上限 0.1 m/s
                spin->setRange(0.0, kMaxLiftLinearSpeed);
                spin->setSingleStep(0.05);
                spin->setValue(qBound(0.0, existingVal.toDouble(param.defaultValue), kMaxLiftLinearSpeed));
                spin->setToolTip(QStringLiteral("顶升线速度上限: %1 m/s").arg(kMaxLiftLinearSpeed));
            } else if (param.unit == "°/s") {
                spin->setRange(-mc.maxAngularSpeedDeg(), mc.maxAngularSpeedDeg());
                spin->setSingleStep(2.0);
                spin->setValue(mc.clampSignedAngularSpeedDeg(existingVal.toDouble(param.defaultValue)));
                spin->setToolTip(QStringLiteral("受全局最大角速度限制 (上限: %1 °/s)").arg(mc.maxAngularSpeedDeg()));
            } else if (param.unit == "m/s") {
                spin->setRange(-mc.maxSpeed(), mc.maxSpeed());
                spin->setSingleStep(0.05);
                spin->setValue(mc.clampSignedLinearSpeed(existingVal.toDouble(param.defaultValue)));
                spin->setToolTip(QStringLiteral("受全局最大速度限制 (上限: %1 m/s)").arg(mc.maxSpeed()));
            } else if (param.key == "distance") {
                // 距离只能为非负值, 方向由 vx / vy 的正负决定
                spin->setRange(0.0, 100.0);
                spin->setSingleStep(0.1);
                spin->setValue(std::max(0.0, existingVal.toDouble(param.defaultValue)));
            } else {
                spin->setRange(-99999, 99999);
                spin->setValue(existingVal.toDouble(param.defaultValue));
            }
            spin->setDecimals(3);
            spin->setStyleSheet("QDoubleSpinBox{background:#22272d;border:1px solid #2a3038;border-radius:3px;padding:5px;color:#c0c8d0;font-size:12px;}");
            input = spin;
        } else if (param.type == "int") {
            auto *spin = new QSpinBox;
            spin->setRange(-99999, 99999);
            spin->setValue(existingVal.toInt(param.defaultValue));
            spin->setStyleSheet("QSpinBox{background:#22272d;border:1px solid #2a3038;border-radius:3px;padding:5px;color:#c0c8d0;font-size:12px;}");
            input = spin;
        } else if (param.type == "combo") {
            auto *combo = new QComboBox;
            if (param.key == "loop") {
                // 循环播放: 界面显示"否/是", 实际存 "no"/"yes"
                combo->addItem("否", "no");
                combo->addItem("是", "yes");
                QString cur = existingVal.toString();
                if (cur == "是") cur = "yes";
                else if (cur == "否") cur = "no";
                int idx = combo->findData(cur);
                combo->setCurrentIndex(idx >= 0 ? idx : 0);
            } else {
                combo->addItems(param.options);
                int idx = combo->findText(existingVal.toString());
                if (idx >= 0) combo->setCurrentIndex(idx);
            }
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
            if (!param.unit.isEmpty() && (param.type == "double" || param.type == "int")) {
                auto *rowWidget = new QWidget(&dlg);
                auto *rowLayout = new QHBoxLayout(rowWidget);
                rowLayout->setContentsMargins(0, 0, 0, 0);
                rowLayout->setSpacing(8);
                rowLayout->addWidget(input, 1);
                auto *unitLbl = new QLabel(param.unit, rowWidget);
                unitLbl->setStyleSheet("color: #a6b0be; font-size: 12px;");
                rowLayout->addWidget(unitLbl);
                vbox->addWidget(rowWidget);
            } else {
                vbox->addWidget(input);
            }
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
            if (param.key == "lift_height") {
                val = qBound(0.0, val, kMaxLiftHeight);
            } else if (param.key == "lift_linear") {
                val = qBound(0.0, val, kMaxLiftLinearSpeed);
            } else if (param.unit == "m/s") {
                val = mc.clampSignedLinearSpeed(val);
            } else if (param.unit == "°/s") {
                val = mc.clampSignedAngularSpeedDeg(val);
            } else if (param.key == "distance") {
                val = std::max(0.0, val);
            }
            resultParams[param.key] = val;
        } else if (param.type == "int") {
            auto *spin = qobject_cast<QSpinBox*>(w);
            resultParams[param.key] = spin->value();
        } else if (param.type == "combo") {
            auto *combo = qobject_cast<QComboBox*>(w);
            if (param.key == "loop") {
                // 传参使用 yes / no
                QString v = combo->currentData().toString();
                resultParams[param.key] = v.isEmpty() ? combo->currentText() : v;
            } else {
                resultParams[param.key] = combo->currentText();
            }
        } else if (param.type == "audio_combo") {
            auto *combo = qobject_cast<QComboBox*>(w);
            // 保存的是完整文件名 (itemData), 列表为空手输时退化为输入内容
            QString full = combo->currentData().toString();
            if (full.isEmpty()) full = combo->currentText().trimmed();
            resultParams[param.key] = full;
        } else if (param.type == "station_combo") {
            auto *combo = qobject_cast<QComboBox*>(w);
            // 下拉框显示名称, itemData 存路网点 id, 保存 id; 未选择时为空
            resultParams["target_station"] = combo->currentData().toString();
        }
    }

    QJsonDocument doc(resultParams);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
}

void TaskChainWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    // 仅在非运行状态下重新拉取地图站点并刷新组，避免打乱正在执行的组列表
    if (!m_listLocked && m_execStatus != Running && !m_chainWorking) {
        loadMapStations();
    }
    loadChainsFromCacheAndServer();
    subscribeChainStatus();
    subscribeChainCurrentTask();

    // 从全局单例 TaskChainManager 获取最新缓存状态（彻底杜绝菜单切换导致状态错误与断档）
    auto &mgr = TaskChainManager::instance();
    m_chainWorking     = mgr.isWorking();
    m_chainPaused      = mgr.isPaused();
    m_currentTaskName  = mgr.currentTask();
    m_currentGroupName = mgr.currentGroup();

    if (m_chainWorking || mgr.isLocalExecutionActive()) {
        m_execStatus = m_chainPaused ? Suspended : Running;
        setListsLocked(true);
    } else if (m_execStatus != Running && m_execStatus != Suspended) {
        m_execStatus = NotStarted;
        setListsLocked(false);
    }

    if (m_overlayTextLabel) {
        m_overlayTextLabel->setText(m_chainPaused ? QStringLiteral("任务链已暂停") : QStringLiteral("任务执行中"));
    }

    applyRuntimeSelection();
    updateBottomBar();

    // 如果处于锁定或运行中，切回时必须强制恢复遮罩与控件锁定
    if (m_listLocked || m_execStatus == Running || m_chainWorking) {
        setListsLocked(true);
        updateLockOverlayGeometry();
        // 在事件循环下一轮再次校验并提升置顶，确保 QStackedWidget 切换后的最终布局严格生效
        QTimer::singleShot(0, this, [this]() {
            if (m_listLocked || m_execStatus == Running || m_chainWorking) {
                updateLockOverlayGeometry();
            }
            applyRuntimeSelection();
        });
    }
}

// 订阅任务链运行状态: {"robotWorking":true,"robotPause":false}
void TaskChainWidget::subscribeChainStatus()
{
    if (m_statusSubscribed) return;
    m_statusSubscribed = true;
    MqttService::instance().subscribeTopic("/taskchain/status", 0,
        [this](const QString &, const QByteArray &payload) { onChainStatusMessage(payload); });
}

void TaskChainWidget::onChainStatusMessage(const QByteArray &payload)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return;

    const QJsonObject data = doc.object();
    m_chainWorking = data.value("robotWorking").toBool(false);
    m_chainPaused  = data.value("robotPause").toBool(false);

    // 本地执行状态跟随车辆实际状态
    if (!m_chainWorking)     m_execStatus = NotStarted;
    else if (m_chainPaused)  m_execStatus = Suspended;
    else                     m_execStatus = Running;

    // 任务链执行状态与锁定遮罩联动
    if (m_chainWorking && !m_listLocked) {
        setListsLocked(true);
    } else if (!m_chainWorking && m_listLocked) {
        setListsLocked(false);
        updateStatus(QStringLiteral("任务链已结束, 编辑区域已解锁"));
    }

    // 动态更新半透明遮罩上的提示文字
    if (m_overlayTextLabel) {
        m_overlayTextLabel->setText(m_chainPaused ? QStringLiteral("任务链已暂停")
                                                  : QStringLiteral("任务执行中"));
    }

    updateBottomBar();
}

// 订阅任务链执行进度: {"currentTask":"sub_task_1","currentGroup":"group_2"}
void TaskChainWidget::subscribeChainCurrentTask()
{
    if (m_currentSubscribed) return;
    m_currentSubscribed = true;
    MqttService::instance().subscribeTopic("/taskchain/currentTask", 0,
        [this](const QString &, const QByteArray &payload) { onChainCurrentTaskMessage(payload); });
}

void TaskChainWidget::onChainCurrentTaskMessage(const QByteArray &payload)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return;

    const QJsonObject data = doc.object();
    m_currentTaskName  = data.value("currentTask").toString();
    m_currentGroupName = data.value("currentGroup").toString();

    // 锁定期间按上报的任务/组显示选中状态
    applyRuntimeSelection();
    updateBottomBar();
}

void TaskChainWidget::updateLockOverlayGeometry()
{
    if (m_lockOverlay && m_panelsContainer) {
        m_lockOverlay->setGeometry(m_panelsContainer->rect());
        if (m_listLocked || m_execStatus == Running || m_chainWorking) {
            m_lockOverlay->show();
            m_lockOverlay->raise();
        }
    }
}

void TaskChainWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateLockOverlayGeometry();
}

// 锁定/解锁 编辑区域 (四面板列表与全部操作按钮):
// 执行期间弹出半透明遮罩覆盖四面板，彻底阻止一切交互，同时保留底部控制栏可用
void TaskChainWidget::setListsLocked(bool locked)
{
    m_listLocked = locked;

    if (m_lockOverlay && m_panelsContainer) {
        if (locked) {
            if (m_overlayTextLabel) {
                m_overlayTextLabel->setText(m_chainPaused ? QStringLiteral("任务链已暂停")
                                                          : QStringLiteral("任务执行中"));
            }
            m_lockOverlay->setGeometry(m_panelsContainer->rect());
            m_lockOverlay->show();
            m_lockOverlay->raise();
        } else {
            m_lockOverlay->hide();
        }
    }

    // 锁定/解锁 m_panelsContainer 下所有子控件
    installLockFilter(m_panelsContainer, locked);
    if (m_lockOverlay) {
        m_lockOverlay->removeEventFilter(this);
        if (m_overlayTextLabel) {
            m_overlayTextLabel->removeEventFilter(this);
        }
    }

    // 锁定期间禁止键盘焦点切换, 避免方向键改变选中项
    if (m_chainList) m_chainList->setFocusPolicy(locked ? Qt::NoFocus : Qt::StrongFocus);
    if (m_taskList)  m_taskList->setFocusPolicy(locked ? Qt::NoFocus : Qt::StrongFocus);
    if (m_groupList) m_groupList->setFocusPolicy(locked ? Qt::NoFocus : Qt::StrongFocus);

    if (locked) {
        if (m_groupHintLabel)
            m_groupHintLabel->setText(QStringLiteral("任务链执行中, 编辑区域已锁定遮罩 (任务链结束后自动解锁)"));
    } else {
        m_currentTaskName.clear();
        m_currentGroupName.clear();
        updateRuntimeCardHighlights(-1, -1);
        if (m_groupHintLabel)
            m_groupHintLabel->setText(QStringLiteral("拖拽标签到组中, 每组只能添加一种标签"));
    }
}

// 递归安装/移除锁定事件过滤器 (含列表项内部的复选框/编辑框等子控件)
void TaskChainWidget::installLockFilter(QWidget *w, bool install)
{
    if (!w) return;
    if (install) w->installEventFilter(this);
    else         w->removeEventFilter(this);

    const QList<QWidget*> kids = w->findChildren<QWidget*>();
    for (QWidget *c : kids) {
        if (install) c->installEventFilter(this);
        else         c->removeEventFilter(this);
    }
}

bool TaskChainWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_panelsContainer && event->type() == QEvent::Resize) {
        updateLockOverlayGeometry();
    }

    if (m_listLocked) {
        // 遮罩自身不拦截，交由遮罩自己的事件处理函数
        if (obj != m_lockOverlay && (!m_lockOverlay || !m_lockOverlay->isAncestorOf(qobject_cast<const QWidget*>(obj)))) {
            switch (event->type()) {
            case QEvent::MouseButtonPress:
            case QEvent::MouseButtonRelease:
            case QEvent::MouseButtonDblClick:
            case QEvent::MouseMove:
            case QEvent::Wheel:
            case QEvent::KeyPress:
            case QEvent::KeyRelease:
            case QEvent::DragEnter:
            case QEvent::DragMove:
            case QEvent::DragLeave:
            case QEvent::Drop:
            case QEvent::ContextMenu:
                return true;   // 锁定期间吞掉用户输入, 不可操作
            default:
                break;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

// 按 MQTT 上报的当前任务/组, 在列表中显示对应的选中状态并高亮边框
void TaskChainWidget::applyRuntimeSelection()
{
    int targetTaskIdx = -1;
    if (!m_currentTaskName.isEmpty() && m_taskList) {
        for (int i = 0; i < m_taskList->count(); ++i) {
            QListWidgetItem *item = m_taskList->item(i);
            if (!item) continue;
            int taskIdx = item->data(Qt::UserRole).toInt();
            if (matchSubTaskName(taskIdx, m_currentTaskName)) {
                targetTaskIdx = taskIdx;
                m_taskList->setCurrentItem(item);
                m_taskList->scrollToItem(item);
                break;
            }
        }
    }

    // 确保当前任务索引与组列表同步
    if (targetTaskIdx >= 0 && targetTaskIdx != m_currentTaskIndex) {
        m_currentTaskIndex = targetTaskIdx;
        refreshGroupList();
    } else if (m_groupList && m_groupList->count() == 0 && targetTaskIdx >= 0) {
        m_currentTaskIndex = targetTaskIdx;
        refreshGroupList();
    }

    int targetGroupIdx = -1;
    if (!m_currentGroupName.isEmpty() && m_groupList) {
        for (int i = 0; i < m_groupList->count(); ++i) {
            QListWidgetItem *item = m_groupList->item(i);
            if (!item) continue;
            int groupIdx = item->data(Qt::UserRole).toInt();
            if (matchGroupName(groupIdx, m_currentGroupName)) {
                targetGroupIdx = groupIdx;
                m_groupList->setCurrentItem(item);
                m_groupList->scrollToItem(item);
                break;
            }
        }
    }

    // 无论是否失焦，给当前正在执行的任务卡片和组卡片应用鲜明的高亮发光选中边框
    updateRuntimeCardHighlights(targetTaskIdx, targetGroupIdx);
}

void TaskChainWidget::updateRuntimeCardHighlights(int currentTaskIdx, int currentGroupIdx)
{
    if (m_taskList) {
        for (int i = 0; i < m_taskList->count(); ++i) {
            QListWidgetItem *item = m_taskList->item(i);
            if (!item) continue;
            QWidget *w = m_taskList->itemWidget(item);
            if (!w) continue;
            int idx = item->data(Qt::UserRole).toInt();
            if (idx == currentTaskIdx && currentTaskIdx >= 0) {
                w->setStyleSheet("#taskCardRoot { background: rgba(0, 188, 212, 0.20); border: 2px solid #00bcd4; border-radius: 4px; }");
            } else {
                w->setStyleSheet("#taskCardRoot { background: transparent; border: 1px solid transparent; border-radius: 4px; }");
            }
        }
    }

    if (m_groupList) {
        for (int i = 0; i < m_groupList->count(); ++i) {
            QListWidgetItem *item = m_groupList->item(i);
            if (!item) continue;
            QWidget *w = m_groupList->itemWidget(item);
            if (!w) continue;
            int idx = item->data(Qt::UserRole).toInt();
            if (idx == currentGroupIdx && currentGroupIdx >= 0) {
                w->setStyleSheet("#groupCardRoot { background: rgba(56, 189, 248, 0.20); border: 2px solid #38bdf8; border-radius: 4px; }");
            } else {
                w->setStyleSheet("#groupCardRoot { background: transparent; border: 1px solid transparent; border-radius: 4px; }");
            }
        }
    }
}

bool TaskChainWidget::matchSubTaskName(int taskIdx, const QString &targetName) const
{
    if (targetName.isEmpty()) return false;
    const int idx = currentChainIndex();
    if (idx < 0 || idx >= m_chains.size()) return false;
    if (taskIdx < 0 || taskIdx >= m_chains[idx].tasks.size()) return false;

    const QJsonObject task = m_chains[idx].tasks[taskIdx].toObject();
    QString subTaskName = task.value("sub_task_name").toString();
    QString name = task.value("name").toString();
    QString defaultName = QString("sub_task_%1").arg(taskIdx + 1);

    return (subTaskName == targetName || name == targetName || defaultName == targetName);
}

bool TaskChainWidget::matchGroupName(int groupIdx, const QString &targetName) const
{
    if (targetName.isEmpty()) return false;
    const QJsonArray groups = currentTaskGroups();
    if (groupIdx < 0 || groupIdx >= groups.size()) return false;

    const QJsonObject group = groups[groupIdx].toObject();
    QString groupName = group.value("group_name").toString();
    QString name = group.value("name").toString();
    QString defaultName = QString("group_%1").arg(groupIdx + 1);

    return (groupName == targetName || name == targetName || defaultName == targetName);
}

QString TaskChainWidget::subTaskNameAt(int taskIdx) const
{
    const int idx = currentChainIndex();
    if (idx < 0 || idx >= m_chains.size()) return QString();
    if (taskIdx < 0 || taskIdx >= m_chains[idx].tasks.size()) return QString();

    const QJsonObject task = m_chains[idx].tasks[taskIdx].toObject();
    QString name = task.value("sub_task_name").toString();
    if (name.isEmpty()) name = task.value("name").toString();
    if (name.isEmpty()) name = QString("sub_task_%1").arg(taskIdx + 1);
    return name;
}

QString TaskChainWidget::groupNameAt(int groupIdx) const
{
    const QJsonArray groups = currentTaskGroups();
    if (groupIdx < 0 || groupIdx >= groups.size()) return QString();

    const QJsonObject group = groups[groupIdx].toObject();
    QString name = group.value("group_name").toString();
    if (name.isEmpty()) name = group.value("name").toString();
    if (name.isEmpty()) name = QString("group_%1").arg(groupIdx + 1);
    return name;
}

QString TaskChainWidget::stationName(const QString &idOrName) const
{
    if (idOrName.isEmpty()) return QString();
    for (const auto &st : m_stations) {
        if (st.id == idOrName || st.name == idOrName) return st.name;
    }
    return idOrName;
}

// 载入本地缓存 + 服务器任务链列表; 同名时以本地缓存为准
void TaskChainWidget::loadChainsFromCacheAndServer()
{
    if (m_cacheLoaded) return;
    m_cacheLoaded = true;

    m_chains.clear();

    // 1) 本地缓存: 上次运行未落盘/正在编辑的数据
    QFile cache(cacheFilePath());
    if (cache.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(cache.readAll());
        cache.close();
        if (doc.isArray()) {
            for (const QJsonValue &v : doc.array()) {
                if (!v.isObject()) continue;
                ChainData chain = importChainFromJson(v.toObject());
                if (!chain.name.isEmpty()) m_chains.append(chain);
            }
        }
    }

    const int localCount = m_chains.size();
    refreshChainList();
    if (!m_chains.isEmpty()) selectChain(0);

    // 2) 服务器列表: 解析并合并服务器任务链 (后台返回字段支持 content 与 fileContent)
    ApiService::instance().listTaskChain([this, localCount](int code, const QJsonObject &resp) {
        if (code != 200) return;

        int added = 0;
        int updated = 0;
        for (const QJsonValue &row : resp.value("data").toArray()) {
            const QJsonObject obj = row.toObject();
            QString fileName = obj.value("fileName").toString();
            if (fileName.isEmpty()) continue;
            if (fileName.endsWith(".json", Qt::CaseInsensitive)) fileName.chop(5);

            // 解析 content 或 fileContent (支持 JSON 字符串及直接对象)
            QJsonObject chainJsonObj;
            if (obj.value("content").isObject()) {
                chainJsonObj = obj.value("content").toObject();
            } else if (obj.value("fileContent").isObject()) {
                chainJsonObj = obj.value("fileContent").toObject();
            } else {
                QString rawStr = obj.value("content").toString();
                if (rawStr.isEmpty()) rawStr = obj.value("fileContent").toString();
                if (!rawStr.isEmpty()) {
                    QJsonDocument doc = QJsonDocument::fromJson(rawStr.toUtf8());
                    if (doc.isObject()) chainJsonObj = doc.object();
                }
            }
            if (chainJsonObj.isEmpty()) continue;

            ChainData chain = importChainFromJson(chainJsonObj);
            if (chain.name.isEmpty()) chain.name = fileName;

            int existIdx = -1;
            for (int i = 0; i < m_chains.size(); ++i) {
                if (m_chains[i].name == chain.name || m_chains[i].name == fileName) {
                    existIdx = i;
                    break;
                }
            }

            if (existIdx >= 0) {
                // 如果本地已有同名任务链，用服务器获取到的最新数据更新
                m_chains[existIdx] = chain;
                ++updated;
            } else {
                m_chains.append(chain);
                ++added;
            }
        }

        if (added > 0 || updated > 0) {
            refreshChainList();
            if (m_currentChainIndex < 0 && !m_chains.isEmpty()) selectChain(0);
            else if (m_currentChainIndex >= 0 && m_currentChainIndex < m_chains.size()) selectChain(m_currentChainIndex);
        }
        m_cacheLoaded = true;
        updateStatus(QStringLiteral("已载入任务链: 本地 %1 条, 服务器获取 %2 条 (新增 %3, 同步 %4)")
            .arg(localCount).arg(added + updated).arg(added).arg(updated));
    });
}

void TaskChainWidget::scheduleCacheWrite()
{
    if (m_cacheTimer) m_cacheTimer->start();
}

void TaskChainWidget::writeCache()
{
    QJsonArray arr;
    for (const auto &chain : m_chains) arr.append(exportChainToJson(chain));

    QFile file(cacheFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    file.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    file.close();
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
            if (!m_listLocked && m_execStatus != Running && !m_chainWorking) {
                refreshGroupList();
            }
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

    m_chains.append(data);
    refreshChainList();
    selectChain(m_chains.size() - 1);
    updateStatus(QStringLiteral("已新建空任务链: %1, 请继续新建任务").arg(name));
}

void TaskChainWidget::onChainLoad()
{
    QString localDir = taskChainLocalDir();
    QString filePath = QFileDialog::getOpenFileName(this, QStringLiteral("加载任务链 JSON"), localDir, "JSON Files (*.json *.task)");
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

    // 1. 同步保存到本地 task_chains 文件夹
    QString localDir = taskChainLocalDir();
    int localSavedCount = 0;
    for (const auto &chain : m_chains) {
        QString fileName = chain.name.trimmed();
        if (fileName.isEmpty()) continue;
        if (!fileName.endsWith(".json", Qt::CaseInsensitive)) {
            fileName += ".json";
        }
        QFile localFile(localDir + "/" + fileName);
        if (localFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QJsonDocument doc(exportChainToJson(chain));
            localFile.write(doc.toJson(QJsonDocument::Indented));
            localFile.close();
            localSavedCount++;
        }
    }

    // 2. 一次性保存列表中所有任务链至服务器端: POST /task/chain/batchSave
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
        [this, count = chains.size(), localSavedCount](int code, const QJsonObject &resp) {
            if (code == 200) {
                // 已落盘到服务器, 保留本地缓存供下次启动合并
                m_didSave = true;
                writeCache();
                updateStatus(QStringLiteral("已保存任务链: 本地 %1 条, 服务器 %2 条").arg(localSavedCount).arg(count));
                Toast::success(this, QStringLiteral("已成功保存 %1 条任务链到本地与服务器").arg(count));
            } else {
                Toast::error(this, QStringLiteral("本地已保存(%1条), 但服务器保存失败(%2): %3")
                    .arg(localSavedCount).arg(code).arg(resp.value("msg").toString(QStringLiteral("请求失败"))));
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
    task["enable"]        = false; // 默认不勾选
    task["checked"]       = false; // 默认不勾选
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
    setAllCheckBoxesChecked(m_taskList, !allCheckBoxesChecked(m_taskList));
    // 勾选框 toggled 信号会自动同步到数据
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
    group["enable"]     = false; // 默认不勾选
    group["checked"]    = false; // 默认不勾选
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
    group["enable"]     = false; // 默认不勾选
    group["checked"]    = false; // 默认不勾选
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
    // 互斥: 全部已选中 → 取消选中; 否则 → 全选
    setAllCheckBoxesChecked(m_groupList, !allCheckBoxesChecked(m_groupList));
    // 勾选框 toggled 信号会自动同步到数据
    updateGroupSelectBtn();
}

void TaskChainWidget::onGroupSendSelected()
{
    int idx = currentChainIndex();
    if (idx < 0) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先选择任务链"));
        return;
    }

    QJsonObject chainJson = exportChainToJson(m_chains[idx], true, true);
    QJsonArray subTasks = chainJson.value("sub_task_info").toArray();
    if (subTasks.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
            QStringLiteral("未勾选有效任务或任务链数据为空"));
        return;
    }

    // 序列化为标准任务链字符串并下发 (POST /task/chain/sendTask)
    QString chainString = QString::fromUtf8(QJsonDocument(chainJson).toJson(QJsonDocument::Compact));
    bool looped = (m_loopCheck && m_loopCheck->isChecked());

    m_execStatus = Running;
    setListsLocked(true);   // 下发后锁定列表, 直至任务链结束
    TaskChainManager::instance().setLocalExecutionActive(true);
    updateBottomBar();
    updateStatus(QStringLiteral("正在下发任务链 (勾选子任务, %1): %2")
        .arg(looped ? QStringLiteral("循环模式") : QStringLiteral("单次模式"))
        .arg(m_chains[idx].name));

    ApiService::instance().sendTaskChain(chainString, true, looped, [this](int code, const QJsonObject &resp) {
        if (code == 200) {
            Toast::success(this, QStringLiteral("任务链数据已成功下发至设备"));
        } else {
            m_execStatus = NotStarted;
            setListsLocked(false);
            TaskChainManager::instance().resetExecutionState();
            updateBottomBar();
            QString err = resp.value("msg").toString(QStringLiteral("请求失败"));
            updateStatus(QStringLiteral("任务链下发失败: %1").arg(err));
            Toast::error(this, QStringLiteral("任务链下发失败(%1): %2").arg(code).arg(err));
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

    QJsonObject chainJson = exportChainToJson(m_chains[idx], false, true);
    QJsonArray subTasks = chainJson.value("sub_task_info").toArray();
    if (subTasks.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
            QStringLiteral("任务链数据为空"));
        return;
    }

    // 序列化为标准任务链字符串并下发全部 (POST /task/chain/sendTask)
    QString chainString = QString::fromUtf8(QJsonDocument(chainJson).toJson(QJsonDocument::Compact));
    bool looped = (m_loopCheck && m_loopCheck->isChecked());

    m_execStatus = Running;
    setListsLocked(true);   // 下发后锁定列表, 直至任务链结束
    TaskChainManager::instance().setLocalExecutionActive(true);
    updateBottomBar();
    updateStatus(QStringLiteral("正在下发任务链 (全部子任务, %1): %2")
        .arg(looped ? QStringLiteral("循环模式") : QStringLiteral("单次模式"))
        .arg(m_chains[idx].name));

    ApiService::instance().sendTaskChain(chainString, true, looped, [this](int code, const QJsonObject &resp) {
        if (code == 200) {
            Toast::success(this, QStringLiteral("全部任务链数据已成功下发至设备"));
        } else {
            m_execStatus = NotStarted;
            setListsLocked(false);
            TaskChainManager::instance().resetExecutionState();
            updateBottomBar();
            QString err = resp.value("msg").toString(QStringLiteral("请求失败"));
            updateStatus(QStringLiteral("任务链下发失败: %1").arg(err));
            Toast::error(this, QStringLiteral("任务链下发失败(%1): %2").arg(code).arg(err));
        }
    });
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

    // 任务链执行中: 以 MQTT /taskchain/currentTask 上报的当前任务/组为准
    if (!m_currentTaskName.isEmpty())  taskName  = m_currentTaskName;
    if (!m_currentGroupName.isEmpty()) groupName = m_currentGroupName;

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

    // 暂停/继续任务链 & 取消任务链
    // 无论从哪个页面切回或处于本地/网络执行阶段，只要任务处于运行、暂停或锁定状态，取消按钮和暂停按钮都必须绝对可用
    bool isWorking = (m_chainWorking || m_execStatus == Running || m_execStatus == Suspended || m_listLocked ||
                      TaskChainManager::instance().isWorking() || TaskChainManager::instance().isLocalExecutionActive());
    m_suspendResumeBtn->setVisible(isWorking);
    m_suspendResumeBtn->setEnabled(isWorking);
    m_suspendResumeBtn->setText((m_chainPaused || m_execStatus == Suspended) ? QStringLiteral("继续任务链")
                                                                            : QStringLiteral("暂停任务链"));

    m_cancelBtn->setEnabled(isWorking);
    m_executeNextBtn->setEnabled(isWorking && !chainName.isEmpty());
}

void TaskChainWidget::updateTaskSelectBtn()
{
    if (!m_taskSelectAllBtn) return;
    m_taskSelectAllBtn->setText(allCheckBoxesChecked(m_taskList)
        ? QStringLiteral("取消选中") : QStringLiteral("全选任务"));
}

void TaskChainWidget::updateGroupSelectBtn()
{
    if (!m_groupSelectAllBtn) return;
    m_groupSelectAllBtn->setText(allCheckBoxesChecked(m_groupList)
        ? QStringLiteral("取消选中") : QStringLiteral("全选组"));
}

QJsonObject TaskChainWidget::exportChainToJson(const ChainData &chain, bool onlyChecked, bool forExecution) const
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
        // 下发执行时强制为 true，确保后台控制器无条件执行；本地保存/写缓存时保留界面真实勾选状态
        subTask["enable"]        = forExecution ? true : enable;

        QJsonArray groupInfo;
        // 以界面正在编辑的 groups 为准, group_info 仅作为兼容旧数据的兜底
        QJsonArray rawGroups = taskObj.value("groups").isArray()
            ? taskObj.value("groups").toArray()
            : taskObj.value("group_info").toArray();

        for (int g = 0; g < rawGroups.size(); ++g) {
            QJsonObject rawGroup = rawGroups[g].toObject();
            bool groupEnable = rawGroup.contains("enable") ? rawGroup.value("enable").toBool() : rawGroup.value("checked").toBool();

            if (onlyChecked && !groupEnable) continue;

            QJsonObject group;

            QString groupName = rawGroup.value("group_name").toString();
            if (groupName.isEmpty()) groupName = rawGroup.value("name").toString();
            if (groupName.isEmpty()) groupName = QString("group_%1").arg(g + 1);

            group["group_name"] = groupName;
            // 下发执行时强制为 true，确保后台控制器无条件执行；本地保存/写缓存时保留界面真实勾选状态
            group["enable"]     = forExecution ? true : groupEnable;

            QJsonArray actionsOut;
            QJsonArray rawActions = rawGroup.value("actions").toArray();
            for (int a = 0; a < rawActions.size(); ++a) {
                QJsonObject rawAction = rawActions[a].toObject();
                QJsonObject action;

                QString name = rawAction.value("name").toString();
                if (name.isEmpty()) name = rawAction.value("action_desc").toString();

                QString actionType = rawAction.value("action_type").toString();
                if (actionType.isEmpty()) actionType = actionTypeFromName(name);
                actionType = normalizeActionType(actionType, name);

                action["action_type"] = actionType;
                action["action_desc"] = name.isEmpty() ? actionDescFromType(actionType) : name;

                QJsonObject params = rawAction.value("params").toObject();

                if (actionType == "99") {
                    // 路径导航: target_station 存路网点 id
                    QString station = params.value("target_station").toString();
                    if (station.isEmpty()) station = rawAction.value("target_station").toString();
                    if (station.isEmpty()) station = params.value("target").toString();
                    if (station.isEmpty()) station = rawAction.value("target").toString();
                    // 兼容历史数据: 存的是名称时统一转换成 id
                    for (const auto &st : m_stations) {
                        if (st.name == station) { station = st.id; break; }
                    }
                    // 用户未选择站点时保持为空, 不填充任何默认站点
                    if (station == QStringLiteral("未知")) station.clear();
                    action["target_station"] = station;
                } else if (actionType == "6") {
                    // 平动: 与“地图与控制-移动-平动”一致, 直接传 distance / vx / vy
                    action["distance"] = params.contains("distance") ? params.value("distance").toDouble() : rawAction.value("distance").toDouble(1.0);
                    action["vx"]       = params.contains("vx") ? params.value("vx").toDouble() : rawAction.value("vx").toDouble(0.2);
                    action["vy"]       = params.contains("vy") ? params.value("vy").toDouble() : rawAction.value("vy").toDouble(0.0);
                } else if (actionType == "5") {
                    // 转动: 旋转方向由旋转角度的正负决定, 不传方向字段
                    action["rotate_angle"] = params.contains("angle") ? params.value("angle").toDouble() : rawAction.value("angle").toDouble(-90.0);
                    action["rotate_speed"] = params.contains("speed") ? params.value("speed").toDouble() : rawAction.value("speed").toDouble(30.0);
                } else if (actionType == "10") {
                    // 播放音频
                    QString audioName = params.value("audio_name").toString();
                    if (audioName.isEmpty()) audioName = params.value("audioName").toString();
                    if (audioName.isEmpty()) audioName = rawAction.value("audio_name").toString();
                    if (audioName.isEmpty()) audioName = rawAction.value("audioName").toString();
                    action["audio_name"] = audioName;
                    // 循环播放: 传参统一为 yes / no (兼容历史数据中的 是 / 否 及布尔值)
                    QString loopVal = params.contains("loop") ? params.value("loop").toVariant().toString()
                                                              : rawAction.value("loop").toVariant().toString();
                    if (loopVal.isEmpty()) loopVal = "no";
                    if (loopVal == "是" || loopVal.compare("true", Qt::CaseInsensitive) == 0) loopVal = "yes";
                    else if (loopVal == "否" || loopVal.compare("false", Qt::CaseInsensitive) == 0) loopVal = "no";
                    action["loop"] = loopVal;
                } else if (actionType == "3") {
                    // 顶升: 传目标高度 lift_height (0-0.15m) 与升降线速度 lift_linear (上限 0.1 m/s)
                    double h = params.contains("lift_height") ? params.value("lift_height").toDouble()
                                                              : rawAction.value("lift_height").toDouble(0.05);
                    double v = params.contains("lift_linear") ? params.value("lift_linear").toDouble()
                                                              : rawAction.value("lift_linear").toDouble(0.05);
                    action["lift_height"] = qBound(0.0, h, kMaxLiftHeight);
                    action["lift_linear"] = qBound(0.0, v, kMaxLiftLinearSpeed);
                } else if (actionType == "11") {
                    // 停止播放音频: 不需要选择音频，直接传 action_type 即可 (无需携带 audio_name 字段)
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

        // 若是勾选下发执行模式, 且该子任务内没有勾选任何有效组, 则跳过该空子任务
        if (forExecution && onlyChecked && groupInfo.isEmpty()) {
            continue;
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
                actionType = normalizeActionType(actionType, actionDesc);
                if (actionDesc.isEmpty()) actionDesc = actionDescFromType(actionType);

                action["name"]        = actionDesc;
                action["action_desc"] = actionDesc;
                action["action_type"] = actionType;

                QJsonObject params;
                if (actionType != "11") {
                    for (auto it = rawAction.begin(); it != rawAction.end(); ++it) {
                        if (it.key() != "name" && it.key() != "action_desc" && it.key() != "action_type") {
                            params[it.key()] = it.value();
                            if (it.key() == "target_station") params["target"] = it.value();
                            if (it.key() == "audio_name") params["audioName"] = it.value();
                            // 界面参数键名与下发字段名对应
                            if (it.key() == "rotate_angle") params["angle"] = it.value();
                            if (it.key() == "rotate_speed") params["speed"] = it.value();
                        }
                    }
                }
                // 兼容历史平动数据: direction + speed 转换为 vx / vy / distance
                if (actionType == "6" && !params.contains("vx") && params.contains("direction")) {
                    const QString dir = params.value("direction").toString();
                    const double  spd = params.value("speed").toDouble(0.2);
                    double dist = params.value("distance").toDouble(1.0);
                    params.remove("direction");
                    params.remove("speed");
                    // 方向改为由 vx / vy 的正负表示, 距离只保留大小
                    if (dir == "左移") {
                        params["vx"] = 0.0;
                        params["vy"] = spd;
                    } else if (dir == "右移") {
                        params["vx"] = 0.0;
                        params["vy"] = -spd;
                    } else if (dir == "后退") {
                        params["vx"] = -spd;
                        params["vy"] = 0.0;
                    } else {
                        params["vx"] = spd;
                        params["vy"] = 0.0;
                    }
                    params["distance"] = std::abs(dist);
                }
                // 下发字段名与界面参数键名统一为 angle/speed, 避免参数摘要重复显示
                params.remove("rotate_angle");
                params.remove("rotate_speed");
                // 兼容历史顶升数据: operation + height 转换为 lift_height / lift_linear
                if (actionType == "3" && !params.contains("lift_height") && params.contains("height")) {
                    params["lift_height"] = params.value("height").toDouble(0.05);
                    params["lift_linear"] = kMaxLiftLinearSpeed;
                }
                if (actionType == "3") {
                    params.remove("operation");
                    params.remove("height");
                    params["lift_height"] = qBound(0.0, params.value("lift_height").toDouble(0.05), kMaxLiftHeight);
                    params["lift_linear"] = qBound(0.0, params.value("lift_linear").toDouble(0.05), kMaxLiftLinearSpeed);
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

    if (type == "99") {
        // 路径导航: set_robot_fix_path / set_third_fix_path
        QString targetStation = action.value("target_station").toString();
        if (targetStation.isEmpty()) {
            // 未设置目标站点时不使用任何默认站点, 直接跳过
            updateStatus(QStringLiteral("路径导航缺少目标站点, 已跳过该动作"));
            Toast::error(this, QStringLiteral("路径导航未设置目标站点, 请先选择站点"));
            return;
        }

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
    } else if (type == "6") {
        // 平动: 与“地图与控制-移动-平动”一致, 直接下发 distance / vx / vy
        const auto &mc = MotionConfigManager::instance();
        double vx   = mc.clampSignedLinearSpeed(action.value("vx").toDouble(0.2));
        double vy   = mc.clampSignedLinearSpeed(action.value("vy").toDouble(0.0));
        double dist = std::max(0.0, action.value("distance").toDouble(1.0));

        ApiService::instance().setLinearMotion(vx, vy, dist, actionId, [this, vx, vy, dist](int code, const QJsonObject &resp) {
            qDebug() << "[TaskChain] setLinearMotion (vx" << vx << "vy" << vy << dist << "m) code:" << code << resp;
        });
        updateStatus(QStringLiteral("正在向控制器下发平动动作: vx %1 m/s, vy %2 m/s, 距离 %3 m").arg(vx).arg(vy).arg(dist));
    } else if (type == "5") {
        // 转动: 旋转方向由旋转角度的正负决定
        double deg = action.value("rotate_angle").toDouble(-90.0);
        // 参数以 °/s 保存, 下发前换算为 rad/s
        double spdDeg = MotionConfigManager::instance().clampSignedAngularSpeedDeg(action.value("rotate_speed").toDouble(30.0));
        double spd    = spdDeg * M_PI / 180.0;
        double angleRad = deg * M_PI / 180.0;

        ApiService::instance().setRotateMotion(angleRad, spd, actionId, [this, angleRad, deg](int code, const QJsonObject &resp) {
            qDebug() << "[TaskChain] setRotateMotion (" << deg << "° =" << angleRad << "rad) code:" << code << resp;
        });
        updateStatus(QStringLiteral("正在向控制器下发转动动作: %1° (角速度 %2°/s)").arg(deg).arg(spdDeg));
    } else if (type == "3") {
        // 顶升: 传目标高度 lift_height (0-0.15m) 与升降线速度 lift_linear (上限 0.1 m/s)
        double h = qBound(0.0, action.value("lift_height").toDouble(0.05), kMaxLiftHeight);
        double v = qBound(0.0, action.value("lift_linear").toDouble(0.05), kMaxLiftLinearSpeed);

        ApiService::instance().setJackControl(h, v, actionId, [this, h, v](int code, const QJsonObject &resp) {
            qDebug() << "[TaskChain] setJackControl (lift_height" << h << "lift_linear" << v << ") code:" << code << resp;
        });
        updateStatus(QStringLiteral("正在向控制器下发顶升动作: 目标高度 %1m, 线速度 %2m/s").arg(h).arg(v));
    }
}

void TaskChainWidget::onSuspendResume()
{
    if (m_execStatus == Running || (m_chainWorking && !m_chainPaused)) {
        m_execStatus = Suspended;
        m_chainPaused = true;
        // PUT /task/chain/control?command=pause navigation
        ApiService::instance().controlTaskChain(QStringLiteral("pause navigation"), [this](int code, const QJsonObject &resp) {
            if (code != 200) {
                Toast::error(this, QStringLiteral("暂停失败(%1): %2")
                    .arg(code).arg(resp.value("msg").toString(QStringLiteral("请求失败"))));
            }
        });
        updateStatus(QStringLiteral("任务链已向服务端发送暂停指令"));
        Toast::info(this, QStringLiteral("任务链已暂停"));

        if (m_overlayTextLabel) {
            m_overlayTextLabel->setText(QStringLiteral("任务链已暂停"));
        }
    } else if (m_execStatus == Suspended || m_chainPaused) {
        m_execStatus = Running;
        m_chainPaused = false;
        // PUT /task/chain/control?command=continue navigation
        ApiService::instance().controlTaskChain(QStringLiteral("continue navigation"), [this](int code, const QJsonObject &resp) {
            if (code != 200) {
                Toast::error(this, QStringLiteral("继续失败(%1): %2")
                    .arg(code).arg(resp.value("msg").toString(QStringLiteral("请求失败"))));
            }
        });
        updateStatus(QStringLiteral("任务链已向服务端发送继续指令"));
        Toast::success(this, QStringLiteral("任务链已继续"));

        if (m_overlayTextLabel) {
            m_overlayTextLabel->setText(QStringLiteral("任务执行中"));
        }
    }
    updateBottomBar();
}

void TaskChainWidget::onCancelChain()
{
    m_execStatus = NotStarted;
    m_chainWorking = false;
    m_chainPaused = false;
    TaskChainManager::instance().resetExecutionState();

    // 取消任务链，隐藏遮罩解锁编辑区域
    setListsLocked(false);
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
    m_execStatus = Running;
    updateStatus(QStringLiteral("正在请求服务端执行 Next 任务..."));
    updateBottomBar();

    // POST /task/chain/sendNextTask (端口 8048)
    ApiService::instance().sendNextTask([this](int code, const QJsonObject &resp) {
        if (code == 200) {
            Toast::success(this, QStringLiteral("执行 Next 任务成功"));
            updateStatus(QStringLiteral("服务端已执行 Next 任务"));
        } else {
            QString msg = resp.value("msg").toString(QStringLiteral("请求失败"));
            Toast::error(this, QStringLiteral("执行 Next 任务失败(%1): %2").arg(code).arg(msg));
            updateStatus(QStringLiteral("执行 Next 任务失败: %1").arg(msg));
        }
        updateBottomBar();
    });
}

// ── 任务与组长按拖拽重新排序 ─────────────────────────────────────────────────

void TaskChainWidget::reorderGroup(int fromIdx, int toIdx)
{
    QJsonArray groups = currentTaskGroups();
    int count = groups.size();
    if (fromIdx < 0 || fromIdx >= count) return;
    if (toIdx < 0 || toIdx >= count) return;
    if (fromIdx == toIdx) return;

    QJsonValue item = groups.takeAt(fromIdx);
    groups.insert(toIdx, item);

    setCurrentTaskGroups(groups);
    refreshGroupList();
    if (toIdx >= 0 && toIdx < m_groupList->count()) {
        m_groupList->setCurrentRow(toIdx);
    }
    updateStatus(QStringLiteral("组已重新排序: 第 %1 组 -> 第 %2 组").arg(fromIdx + 1).arg(toIdx + 1));
}

void TaskChainWidget::reorderTask(int fromIdx, int toIdx)
{
    int cIdx = currentChainIndex();
    if (cIdx < 0 || cIdx >= m_chains.size()) return;
    auto &tasks = m_chains[cIdx].tasks;
    int count = tasks.size();
    if (fromIdx < 0 || fromIdx >= count) return;
    if (toIdx < 0 || toIdx >= count) return;
    if (fromIdx == toIdx) return;

    QJsonValue item = tasks.takeAt(fromIdx);
    tasks.insert(toIdx, item);

    refreshTaskList();
    if (toIdx >= 0 && toIdx < m_taskList->count()) {
        m_taskList->setCurrentRow(toIdx);
    }
    updateStatus(QStringLiteral("任务已重新排序: 第 %1 项 -> 第 %2 项").arg(fromIdx + 1).arg(toIdx + 1));
}