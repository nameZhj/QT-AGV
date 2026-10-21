/**
 * @file    TaskChainGroupCardWidget.cpp
 * @layer   GUI
 * @brief   Implementation of TaskChainGroupCardWidget.
 * @deps    TaskChainGroupCardWidget.h, TaskChainWidget.h, TaskChainDragWidgets.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "TaskChainGroupCardWidget.h"
#include "TaskChainWidget.h"
#include "TaskChainDragWidgets.h"
#include "TaskChainActionDefs.h"
#include "TaskActionParamDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QToolButton>
#include <QPushButton>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonArray>
#include "TaskChainStyle.h"

void TaskChainGroupCardWidget::buildGroupRowWidget(TaskChainWidget *owner, QListWidgetItem *item, int groupIndex, bool checked)
{
    if (!owner || !item || !owner->m_groupList) return;
    new TaskChainGroupCardWidget(owner, item, groupIndex, checked);
}

TaskChainGroupCardWidget::TaskChainGroupCardWidget(TaskChainWidget *owner, QListWidgetItem *item, int groupIndex, bool checked, QWidget *parent)
    : QWidget(parent), m_owner(owner), m_item(item)
{
    setupUI(groupIndex, checked);
}

void TaskChainGroupCardWidget::setupUI(int groupIndex, bool checked)
{
    QJsonArray groups = m_owner->currentTaskGroups();
    if (groupIndex < 0 || groupIndex >= groups.size()) return;

    QJsonObject group = groups[groupIndex].toObject();
    QString groupName = group.value("group_name").toString();
    if (groupName.isEmpty()) groupName = group.value("name").toString();
    if (groupName.isEmpty()) groupName = QString("group_%1").arg(groupIndex + 1);

    QString actionType = group.value("actionType").toString();
    QJsonArray actions = group.value("actions").toArray();

    setObjectName("groupCardRoot");
    setStyleSheet(kGroupCardRoot);
    setAcceptDrops(true);

    auto *filter = new CardLongPressDragFilter("application/x-taskchain-group-reorder", groupIndex, this, m_owner->m_groupList);
    filter->onSelected = [this](int idx) {
        if (m_owner->m_groupList && m_owner->m_groupList->currentRow() != idx)
            m_owner->m_groupList->setCurrentRow(idx);
        m_owner->updateBottomBar();
        m_owner->updateGroupCardSelectionHighlight();
    };
    installEventFilter(filter);

    auto *vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(4, 4, 4, 4);
    vbox->setSpacing(3);

    setupHeader(vbox, groupIndex, checked, groupName, actionType);
    setupActions(vbox, groupIndex, group);

    int contentH = actionType.isEmpty() ? 60 : actions.size() * (24 + 32 + 8);
    int totalH = 28 + contentH + 16;
    m_item->setSizeHint(QSize(0, qMax(80, totalH)));
    m_owner->m_groupList->setItemWidget(m_item, this);
}

void TaskChainGroupCardWidget::setupHeader(QVBoxLayout *vbox, int groupIndex, bool checked,
                                           const QString &groupName, const QString &actionType)
{
    auto *headerRow = new QHBoxLayout;
    headerRow->setSpacing(6);

    auto *checkBox = new QCheckBox;
    checkBox->setChecked(checked);
    checkBox->setStyleSheet(kCheckBoxStyle);
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
    delBtn->setStyleSheet(kDelBtnStyle);
    headerRow->addWidget(delBtn);
    vbox->addLayout(headerRow);

    connect(checkBox, &QCheckBox::clicked, this, [this, groupIndex]() {
        if (m_owner->m_groupList && m_owner->m_groupList->currentRow() != groupIndex)
            m_owner->m_groupList->setCurrentRow(groupIndex);
    });

    connect(checkBox, &QCheckBox::toggled, this, [this, groupIndex](bool state) {
        QJsonArray groups = m_owner->currentTaskGroups();
        if (groupIndex < 0 || groupIndex >= groups.size()) return;
        QJsonObject grp = groups[groupIndex].toObject();
        grp["enable"] = state; grp["checked"] = state;
        groups[groupIndex] = grp;
        m_owner->setCurrentTaskGroups(groups);
        m_owner->updateGroupSelectBtn();
        m_owner->scheduleCacheWrite();
    });

    connect(delBtn, &QToolButton::clicked, this, [this, groupIndex]() { onDeleteGroup(groupIndex); });
}

void TaskChainGroupCardWidget::onDeleteGroup(int groupIndex)
{
    QJsonArray groups = m_owner->currentTaskGroups();
    if (groupIndex < 0 || groupIndex >= groups.size()) return;
    QString grpName = groups[groupIndex].toObject().value("name").toString();
    if (QMessageBox::question(this, QStringLiteral("确认"),
            QStringLiteral("确认删除组 [%1] 吗？").arg(grpName)) != QMessageBox::Yes) return;
    groups.removeAt(groupIndex);
    m_owner->setCurrentTaskGroups(groups);
    m_owner->refreshGroupList();
}

void TaskChainGroupCardWidget::setupActions(QVBoxLayout *vbox, int groupIndex, const QJsonObject &group)
{
    QString actionType = group.value("actionType").toString();
    QJsonArray actions = group.value("actions").toArray();

    if (actionType.isEmpty()) {
        auto *hint = new QLabel(QStringLiteral("  拖拽标签到此组..."));
        hint->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        hint->setStyleSheet("color:#555; font-size:11px; background:transparent; padding:8px 4px;");
        hint->setMinimumHeight(60);
        vbox->addWidget(hint);
        return;
    }

    for (int a = 0; a < actions.size(); ++a) {
        QJsonObject action = actions[a].toObject();
        QString actionName = action.value("name").toString();
        if (actionName.isEmpty()) actionName = action.value("action_desc").toString();
        QJsonObject params = action.value("params").toObject();

        QString targetId = params.value("target_station").toString();
        if (targetId.isEmpty()) targetId = params.value("target").toString();
        if (targetId.isEmpty()) targetId = action.value("target_station").toString();
        const QString displayTarget = targetId.isEmpty() ? QStringLiteral("未知") : m_owner->stationName(targetId);

        QString displayName = (actionName == "路径导航" || action.value("action_desc").toString() == "路径导航")
                                  ? QStringLiteral("路径导航 -> %1").arg(displayTarget) : actionName;

        auto *actRow = new QHBoxLayout;
        actRow->setSpacing(4);
        auto *actLabel = new QLabel(QStringLiteral("%1. %2").arg(a + 1).arg(displayName));
        actLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        actLabel->setStyleSheet("color:#c0c8d0; font-size:11px; background:transparent; padding:2px 8px;");
        actLabel->setMinimumHeight(24);
        actRow->addWidget(actLabel, 1);

        auto *delTagBtn = new QToolButton;
        delTagBtn->setText("✕");
        delTagBtn->setToolTip(QStringLiteral("删除标签"));
        delTagBtn->setAutoRaise(true);
        delTagBtn->setFixedSize(22, 22);
        delTagBtn->setStyleSheet(kDelTagStyle);
        actRow->addWidget(delTagBtn);
        vbox->addLayout(actRow);

        ActionDef *def = m_owner->findActionDef(actionName);
        bool isNoParam = TaskChainActionDefs::isActionNoParam(def, actionName);
        QString summary = TaskChainActionDefs::formatParamSummary(actionName, params,
            [this](const QString &id) { return m_owner->stationName(id); });

        auto *paramBtn = new QPushButton(summary);
        paramBtn->setStyleSheet(isNoParam ? kParamNoParamStyle : kParamEditStyle);
        paramBtn->setToolTip(isNoParam ? QStringLiteral("此动作直接下发 action_type，无需额外设置参数") : QStringLiteral("点击编辑参数"));
        paramBtn->setEnabled(!isNoParam);
        paramBtn->setMinimumHeight(32);

        connect(paramBtn, &QPushButton::pressed, this, [this, groupIndex]() {
            if (m_owner->m_groupList) m_owner->m_groupList->setCurrentRow(groupIndex);
            m_owner->updateBottomBar();
            m_owner->updateGroupCardSelectionHighlight();
        });

        if (!isNoParam) {
            connect(paramBtn, &QPushButton::clicked, this, [this, groupIndex, a, actionName]() {
                onEditActionParam(groupIndex, a, actionName);
            });
        }
        vbox->addWidget(paramBtn);

        connect(delTagBtn, &QToolButton::clicked, this, [this, groupIndex, a]() {
            onDeleteAction(groupIndex, a);
        });
    }
}

void TaskChainGroupCardWidget::onEditActionParam(int groupIndex, int actionIndex, const QString &actionName)
{
    ActionDef *def = m_owner->findActionDef(actionName);
    if (!def) return;
    QJsonArray groups = m_owner->currentTaskGroups();
    if (groupIndex < 0 || groupIndex >= groups.size()) return;
    QJsonObject grp = groups[groupIndex].toObject();
    QJsonArray acts = grp.value("actions").toArray();
    if (actionIndex < 0 || actionIndex >= acts.size()) return;
    QJsonObject act = acts[actionIndex].toObject();

    QString result = m_owner->openActionDefDialog(*def, act.value("params").toObject());
    if (result.isEmpty()) return;
    QJsonDocument doc = QJsonDocument::fromJson(result.toUtf8());
    if (!doc.isObject()) return;
    act["params"] = doc.object();
    acts[actionIndex] = act;
    grp["actions"] = acts;
    groups[groupIndex] = grp;
    m_owner->setCurrentTaskGroups(groups);
    m_owner->refreshGroupList();
}

void TaskChainGroupCardWidget::onDeleteAction(int groupIndex, int actionIndex)
{
    QJsonArray groups = m_owner->currentTaskGroups();
    if (groupIndex < 0 || groupIndex >= groups.size()) return;
    QJsonObject grp = groups[groupIndex].toObject();
    QJsonArray acts = grp.value("actions").toArray();
    if (actionIndex < 0 || actionIndex >= acts.size()) return;
    acts.removeAt(actionIndex);
    if (acts.isEmpty()) grp.remove("actionType");
    grp["actions"] = acts;
    groups[groupIndex] = grp;
    m_owner->setCurrentTaskGroups(groups);
    m_owner->refreshGroupList();
}
