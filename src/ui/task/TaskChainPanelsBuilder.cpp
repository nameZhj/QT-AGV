/**
 * @file    TaskChainPanelsBuilder.cpp
 * @layer   GUI
 * @brief   Implementation of four workflow panels and bottom control bar assembly.
 * @deps    TaskChainPanelsBuilder.h, TaskChainWidget.h, TaskChainDragWidgets.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "TaskChainPanelsBuilder.h"
#include "TaskChainWidget.h"
#include "TaskChainDragWidgets.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include "TaskChainStyle.h"
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QListWidget>

QPushButton *TaskChainPanelsBuilder::makeBtn(const QString &text)
{
    auto *btn = new QPushButton(text);
    btn->setStyleSheet(S_BTN_STYLE);
    return btn;
}

void TaskChainPanelsBuilder::buildPanels(TaskChainWidget *owner, QWidget *panelsContainer)
{
    auto *containerLayout = new QVBoxLayout(panelsContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);

    auto *splitter = new QSplitter(Qt::Horizontal, panelsContainer);
    containerLayout->addWidget(splitter, 1);

    auto addPanel = [splitter](std::function<void(QVBoxLayout*)> setup) {
        auto *panel = new QWidget;
        auto *layout = new QVBoxLayout(panel);
        layout->setContentsMargins(6, 6, 6, 6);
        layout->setSpacing(6);
        setup(layout);
        splitter->addWidget(panel);
    };

    addPanel([owner](QVBoxLayout *l) { setupPanel1Chain(owner, l); });
    addPanel([owner](QVBoxLayout *l) { setupPanel2Task(owner, l); });
    addPanel([owner](QVBoxLayout *l) { setupPanel3Group(owner, l); });
    addPanel([owner](QVBoxLayout *l) { setupPanel4DragDrop(owner, l); });

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 3);
    splitter->setStretchFactor(3, 2);
    splitter->setSizes({200, 160, 400, 280});

    setupLockOverlay(owner, panelsContainer);
}

void TaskChainPanelsBuilder::setupLockOverlay(TaskChainWidget *owner, QWidget *panelsContainer)
{
    auto *overlay = new TaskChainLockOverlay(panelsContainer);
    overlay->setObjectName("taskChainLockOverlay");
    overlay->setStyleSheet("background: transparent;");

    auto *overlayLayout = new QVBoxLayout(overlay);
    overlayLayout->setAlignment(Qt::AlignCenter);
    overlayLayout->setContentsMargins(0, 0, 0, 0);

    owner->m_overlayTextLabel = new QLabel(QStringLiteral("任务执行中"), overlay);
    owner->m_overlayTextLabel->setStyleSheet(
        "color: #38bdf8; font-size: 15px; font-weight: bold; letter-spacing: 1px; "
        "background: rgba(15, 23, 42, 0.85); border: 1px solid rgba(56, 189, 248, 0.45); "
        "border-radius: 18px; padding: 6px 22px;");
    owner->m_overlayTextLabel->setAlignment(Qt::AlignCenter);
    owner->m_overlayTextLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    overlayLayout->addWidget(owner->m_overlayTextLabel);

    overlay->setVisible(false);
    owner->m_lockOverlay = overlay;
}

void TaskChainPanelsBuilder::setupPanel1Chain(TaskChainWidget *owner, QVBoxLayout *layout)
{
    auto *topRow = new QHBoxLayout;
    topRow->setContentsMargins(2, 2, 2, 4);
    topRow->setSpacing(6);

    owner->m_chainNewBtn  = makeBtn(QStringLiteral("新建"));
    owner->m_chainLoadBtn = makeBtn(QStringLiteral("加载"));
    owner->m_chainSaveBtn = makeBtn(QStringLiteral("保存"));
    topRow->addWidget(owner->m_chainNewBtn);
    topRow->addWidget(owner->m_chainLoadBtn);
    topRow->addWidget(owner->m_chainSaveBtn);

    auto *title = new QLabel(QStringLiteral("任务链列表"));
    title->setStyleSheet("color:#b0b8c0; font-size:12px; font-weight:bold; background:transparent; padding-left:4px;");
    topRow->addWidget(title);
    topRow->addStretch(1);
    layout->addLayout(topRow);

    owner->m_chainList = new QListWidget;
    owner->m_chainList->setObjectName("taskChainList");
    owner->m_chainList->setFocusPolicy(Qt::NoFocus);
    owner->m_chainList->setStyleSheet(
        "QListWidget#taskChainList { background:#14171b; border:1px solid #23272e; border-radius:4px; outline:none; padding:4px 2px; }"
        "QListWidget#taskChainList::item { background:transparent; border:none; padding:2px 0px; margin:2px 0px; }"
        "QListWidget#taskChainList::item:selected { background:transparent; border:none; }"
        "QListWidget#taskChainList::item:hover { background:transparent; border:none; }");
    layout->addWidget(owner->m_chainList, 1);

    owner->m_chainStatus = new QLabel;
    owner->m_chainStatus->setStyleSheet("color:#666; font-size:10px; padding-top:4px;");
    layout->addWidget(owner->m_chainStatus);

    QObject::connect(owner->m_chainNewBtn,  &QPushButton::clicked, owner, &TaskChainWidget::onChainNew);
    QObject::connect(owner->m_chainLoadBtn, &QPushButton::clicked, owner, &TaskChainWidget::onChainLoad);
    QObject::connect(owner->m_chainSaveBtn, &QPushButton::clicked, owner, &TaskChainWidget::onChainSave);
    QObject::connect(owner->m_chainList, &QListWidget::currentItemChanged, owner, &TaskChainWidget::onChainItemChanged);
}

void TaskChainPanelsBuilder::setupPanel2Task(TaskChainWidget *owner, QVBoxLayout *layout)
{
    auto *title = new QLabel(QStringLiteral("  任务"));
    title->setStyleSheet(S_PANEL_TITLE);
    layout->addWidget(title);

    auto *btnRow1 = new QHBoxLayout;
    btnRow1->setSpacing(6);
    owner->m_taskNewBtn    = makeBtn(QStringLiteral("新建任务"));
    owner->m_taskDeleteBtn = makeBtn(QStringLiteral("删除任务"));
    btnRow1->addWidget(owner->m_taskNewBtn);
    btnRow1->addWidget(owner->m_taskDeleteBtn);
    btnRow1->addStretch();
    layout->addLayout(btnRow1);

    auto *taskDropList = new DropTargetListWidget;
    taskDropList->setStyleSheet(S_LIST_STYLE);
    owner->m_taskList = taskDropList;
    taskDropList->onTaskReordered = [owner](int fromRow, int toRow) { owner->reorderTask(fromRow, toRow); };
    layout->addWidget(owner->m_taskList, 1);

    owner->m_taskCountLabel = new QLabel(QStringLiteral("共 [0] 项"));
    owner->m_taskCountLabel->setStyleSheet("color:#777; font-size:11px; padding:2px;");
    layout->addWidget(owner->m_taskCountLabel);

    owner->m_taskSelectAllBtn = makeBtn(QStringLiteral("全选任务"));
    layout->addWidget(owner->m_taskSelectAllBtn);

    QObject::connect(owner->m_taskNewBtn,       &QPushButton::clicked, owner, &TaskChainWidget::onTaskNew);
    QObject::connect(owner->m_taskDeleteBtn,     &QPushButton::clicked, owner, &TaskChainWidget::onTaskDelete);
    QObject::connect(owner->m_taskSelectAllBtn, &QPushButton::clicked, owner, &TaskChainWidget::onTaskSelectAll);
    QObject::connect(owner->m_taskList, &QListWidget::currentItemChanged, owner, [owner](QListWidgetItem *current, QListWidgetItem*) {
        owner->m_currentTaskIndex = current ? current->data(Qt::UserRole).toInt() : -1;
        owner->refreshGroupList();
        owner->updateBottomBar();
        owner->updateTaskSelectBtn();
    });
}

void TaskChainPanelsBuilder::setupPanel3Group(TaskChainWidget *owner, QVBoxLayout *layout)
{
    auto *title = new QLabel(QStringLiteral("  组"));
    title->setStyleSheet(S_PANEL_TITLE);
    layout->addWidget(title);

    auto *btnRow1 = new QHBoxLayout;
    btnRow1->setSpacing(6);
    owner->m_groupAddBtn      = makeBtn(QStringLiteral("添加组"));
    owner->m_groupInsertBtn   = makeBtn(QStringLiteral("插入组"));
    owner->m_groupDeleteBtn    = makeBtn(QStringLiteral("删除选中"));
    owner->m_groupSelectAllBtn = makeBtn(QStringLiteral("全选组"));
    btnRow1->addWidget(owner->m_groupAddBtn);
    btnRow1->addWidget(owner->m_groupInsertBtn);
    btnRow1->addWidget(owner->m_groupDeleteBtn);
    btnRow1->addWidget(owner->m_groupSelectAllBtn);
    btnRow1->addStretch();
    layout->addLayout(btnRow1);

    auto *dropList = new DropTargetListWidget;
    dropList->setStyleSheet(S_LIST_STYLE);
    owner->m_groupList = dropList;
    dropList->onActionDropped = [owner](const QString &actionName, int groupIdx) {
        owner->handleActionDropped(actionName, groupIdx);
    };
    dropList->onGroupReordered = [owner](int fromRow, int toRow) {
        owner->reorderGroup(fromRow, toRow);
    };
    layout->addWidget(owner->m_groupList, 1);

    auto *btnRow2 = new QHBoxLayout;
    btnRow2->setSpacing(6);
    btnRow2->addStretch();
    owner->m_loopCheck = new QCheckBox(QStringLiteral("循环"));
    owner->m_loopCheck->setToolTip(QStringLiteral("勾选后将以循环模式执行下发任务"));
    owner->m_groupSendSelBtn = makeBtn(QStringLiteral("发送勾选的任务"));
    owner->m_groupSendAllBtn = makeBtn(QStringLiteral("发送全部任务"));
    btnRow2->addWidget(owner->m_loopCheck);
    btnRow2->addWidget(owner->m_groupSendSelBtn);
    btnRow2->addWidget(owner->m_groupSendAllBtn);
    layout->addLayout(btnRow2);

    owner->m_groupHintLabel = new QLabel(QStringLiteral("拖拽标签到组中添加动作；长按组卡片可重新排序；选中组支持 Ctrl+C / Ctrl+V 复制粘贴"));
    owner->m_groupHintLabel->setStyleSheet("color:#666; font-size:11px; padding:2px;");
    layout->addWidget(owner->m_groupHintLabel);

    QObject::connect(owner->m_groupAddBtn,       &QPushButton::clicked, owner, &TaskChainWidget::onGroupAdd);
    QObject::connect(owner->m_groupInsertBtn,    &QPushButton::clicked, owner, &TaskChainWidget::onGroupInsert);
    QObject::connect(owner->m_groupDeleteBtn,     &QPushButton::clicked, owner, &TaskChainWidget::onGroupDelete);
    QObject::connect(owner->m_groupSelectAllBtn, &QPushButton::clicked, owner, &TaskChainWidget::onGroupSelectAll);
    QObject::connect(owner->m_groupSendSelBtn,   &QPushButton::clicked, owner, &TaskChainWidget::onGroupSendSelected);
    QObject::connect(owner->m_groupSendAllBtn,   &QPushButton::clicked, owner, &TaskChainWidget::onGroupSendAll);
    QObject::connect(owner->m_groupList, &QListWidget::currentItemChanged, owner, [owner](QListWidgetItem*, QListWidgetItem*) {
        owner->updateBottomBar();
        owner->updateGroupCardSelectionHighlight();
    });

    owner->m_groupList->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(owner->m_groupList, &QListWidget::customContextMenuRequested, owner, &TaskChainWidget::showGroupContextMenu);
}

void TaskChainPanelsBuilder::setupPanel4DragDrop(TaskChainWidget *owner, QVBoxLayout *layout)
{
    auto *title = new QLabel(QStringLiteral("  可拖拽动作标签"));
    title->setStyleSheet(S_PANEL_TITLE);
    layout->addWidget(title);

    owner->m_dragHintLabel = new QLabel(QStringLiteral("拖拽任务标签到组列表完成添加"));
    owner->m_dragHintLabel->setStyleSheet("color:#777; font-size:11px; padding:4px 6px;");
    owner->m_dragHintLabel->setWordWrap(true);
    layout->addWidget(owner->m_dragHintLabel);

    owner->m_actionTagList = new DraggableListWidget;
    owner->m_actionTagList->setStyleSheet(S_LIST_STYLE);
    layout->addWidget(owner->m_actionTagList, 1);
}

void TaskChainPanelsBuilder::setupBottomBar(TaskChainWidget *owner, QVBoxLayout *layout)
{
    auto *bar = new QWidget;
    bar->setFixedHeight(52);
    bar->setStyleSheet(S_BOTTOM_BAR);

    auto *hbox = new QHBoxLayout(bar);
    hbox->setContentsMargins(12, 4, 12, 4);
    hbox->setSpacing(10);

    owner->m_bottomInfoLabel = new QLabel(QStringLiteral("未选择任务链"));
    owner->m_bottomInfoLabel->setStyleSheet("font-size:12px;");
    hbox->addWidget(owner->m_bottomInfoLabel, 1);

    owner->m_suspendResumeBtn = new QPushButton(QStringLiteral("暂停任务链"));
    owner->m_cancelBtn        = new QPushButton(QStringLiteral("取消"));
    owner->m_executeNextBtn   = new QPushButton(QStringLiteral("执行 Next 任务"));
    owner->m_suspendResumeBtn->setEnabled(false);
    owner->m_cancelBtn->setEnabled(false);
    owner->m_executeNextBtn->setEnabled(false);

    hbox->addWidget(owner->m_suspendResumeBtn);
    hbox->addWidget(owner->m_cancelBtn);
    hbox->addWidget(owner->m_executeNextBtn);

    layout->addWidget(bar);

    QObject::connect(owner->m_suspendResumeBtn, &QPushButton::clicked, owner, &TaskChainWidget::onSuspendResume);
    QObject::connect(owner->m_cancelBtn,        &QPushButton::clicked, owner, &TaskChainWidget::onCancelChain);
    QObject::connect(owner->m_executeNextBtn,   &QPushButton::clicked, owner, &TaskChainWidget::onExecuteNext);
}
