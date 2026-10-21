/**
 * @file    TaskChainListSync.cpp
 * @layer   GUILayer
 * @brief   Implementation of task chain list updates and bottom bar synchronization.
 * @deps    TaskChainListSync.h, TaskChainWidget.h, TaskChainRowHelper.h, TaskChainGroupCardWidget.h, TaskChainClipboardHelper.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "TaskChainListSync.h"
#include "TaskChainWidget.h"
#include "TaskChainRowHelper.h"
#include "TaskChainGroupCardWidget.h"
#include "TaskChainClipboardHelper.h"
#include "core/TaskChainManager.h"
#include <QListWidget>
#include <QListWidgetItem>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>

bool TaskChainListSync::allCheckBoxesChecked(QListWidget *list)
{
    if (!list || list->count() == 0) return false;
    for (int i = 0; i < list->count(); ++i) {
        auto *cb = list->itemWidget(list->item(i)) ? list->itemWidget(list->item(i))->findChild<QCheckBox*>() : nullptr;
        if (!cb || !cb->isChecked()) return false;
    }
    return true;
}

void TaskChainListSync::setAllCheckBoxesChecked(QListWidget *list, bool checked)
{
    if (!list) return;
    for (int i = 0; i < list->count(); ++i) {
        auto *cb = list->itemWidget(list->item(i)) ? list->itemWidget(list->item(i))->findChild<QCheckBox*>() : nullptr;
        if (cb) cb->setChecked(checked);
    }
}

void TaskChainListSync::refreshChainList(TaskChainWidget *w)
{
    if (!w || !w->m_chainList) return;
    w->m_chainList->blockSignals(true);
    w->m_chainList->clear();
    for (int i = 0; i < w->m_chains.size(); ++i) {
        auto *item = new QListWidgetItem(w->m_chainList);
        item->setData(Qt::UserRole, i);
        createChainRowWidget(w, item, i);
    }
    w->m_chainList->blockSignals(false);
    updateChainCardHighlight(w);
    w->scheduleCacheWrite();
    if (w->m_listLocked) w->installLockFilter(w->m_chainList, true);
}

void TaskChainListSync::updateChainCardHighlight(TaskChainWidget *w)
{
    if (!w || !w->m_chainList) return;
    int curIdx = currentChainIndex(w);
    for (int i = 0; i < w->m_chainList->count(); ++i) {
        auto *item = w->m_chainList->item(i);
        if (!item) continue;
        auto *widget = w->m_chainList->itemWidget(item);
        if (!widget) continue;

        bool isSelected = (i == curIdx);
        auto *indicator = widget->findChild<QWidget*>("chainIndicator");
        if (indicator) {
            indicator->setStyleSheet(isSelected
                ? "background-color: #00a8ff; border-top-left-radius: 4px; border-bottom-left-radius: 4px;"
                : "background-color: transparent;");
        }
        if (isSelected) {
            widget->setStyleSheet("#chainCardRoot { background: #21252d; border: 1px solid #323844; border-radius: 4px; }");
        } else {
            widget->setStyleSheet("#chainCardRoot { background: #191c21; border: 1px solid #23272e; border-radius: 4px; }"
                                  "#chainCardRoot:hover { background: #1e2228; border-color: #2b313b; }");
        }
    }
}

void TaskChainListSync::createChainRowWidget(TaskChainWidget *w, QListWidgetItem *item, int chainIndex)
{
    TaskChainRowHelper::buildChainRowWidget(w, item, chainIndex);
}

void TaskChainListSync::selectChain(TaskChainWidget *w, int index)
{
    if (!w || index < 0 || index >= w->m_chains.size()) return;
    w->m_currentChainIndex = index;
    w->m_currentTaskIndex = -1;

    if (w->m_chainList && w->m_chainList->currentRow() != index) {
        w->m_chainList->blockSignals(true);
        w->m_chainList->setCurrentRow(index);
        w->m_chainList->blockSignals(false);
    }

    w->refreshTaskList();
    w->refreshGroupList();
    w->updateBottomBar();
    updateChainCardHighlight(w);
}

int TaskChainListSync::currentChainIndex(const TaskChainWidget *w)
{
    if (!w) return -1;
    if (w->m_chainList) {
        auto *item = w->m_chainList->currentItem();
        if (item) return item->data(Qt::UserRole).toInt();
        if (w->m_currentChainIndex >= 0 && w->m_currentChainIndex < w->m_chains.size()) {
            return w->m_currentChainIndex;
        }
    }
    return -1;
}

QJsonArray TaskChainListSync::currentTaskGroups(const TaskChainWidget *w)
{
    if (!w) return QJsonArray();
    int chainIdx = w->currentChainIndex();
    if (chainIdx < 0 || chainIdx >= w->m_chains.size()) return QJsonArray();
    if (w->m_currentTaskIndex < 0 || w->m_currentTaskIndex >= w->m_chains[chainIdx].tasks.size()) {
        return QJsonArray();
    }
    QJsonObject task = w->m_chains[chainIdx].tasks[w->m_currentTaskIndex].toObject();
    return task.value("groups").toArray();
}

void TaskChainListSync::setCurrentTaskGroups(TaskChainWidget *w, const QJsonArray &groups)
{
    if (!w) return;
    int chainIdx = w->currentChainIndex();
    if (chainIdx < 0 || chainIdx >= w->m_chains.size()) return;
    if (w->m_currentTaskIndex < 0 || w->m_currentTaskIndex >= w->m_chains[chainIdx].tasks.size()) return;
    QJsonObject task = w->m_chains[chainIdx].tasks[w->m_currentTaskIndex].toObject();
    task["groups"] = groups;
    task["group_info"] = groups;
    w->m_chains[chainIdx].tasks[w->m_currentTaskIndex] = task;
}

void TaskChainListSync::refreshTaskList(TaskChainWidget *w)
{
    if (!w || !w->m_taskList) return;
    w->m_taskList->blockSignals(true);
    w->m_taskList->clear();

    int idx = w->currentChainIndex();
    if (idx >= 0 && idx < w->m_chains.size()) {
        const auto &chain = w->m_chains[idx];
        for (int i = 0; i < chain.tasks.size(); ++i) {
            QJsonObject task = chain.tasks[i].toObject();
            auto *item = new QListWidgetItem(w->m_taskList);
            item->setData(Qt::UserRole, i);
            TaskChainRowHelper::buildTaskRowWidget(w, item, i, task);
        }
    }
    if (w->m_taskCountLabel) {
        w->m_taskCountLabel->setText(QStringLiteral("共 [%1] 项").arg(w->m_taskList->count()));
    }
    w->m_taskList->blockSignals(false);
    w->updateTaskSelectBtn();
    w->scheduleCacheWrite();
    if (w->m_listLocked) {
        w->installLockFilter(w->m_taskList, true);
    }
}

void TaskChainListSync::refreshGroupList(TaskChainWidget *w)
{
    if (!w || !w->m_groupList) return;
    w->m_groupList->blockSignals(true);
    w->m_groupList->clear();

    QJsonArray groups = w->currentTaskGroups();
    for (int i = 0; i < groups.size(); ++i) {
        QJsonObject groupObj = groups[i].toObject();
        bool checked = groupObj.value("checked").toBool();

        auto *item = new QListWidgetItem(w->m_groupList);
        item->setData(Qt::UserRole, i);
        createGroupRowWidget(w, item, i, checked);
    }
    w->m_groupList->blockSignals(false);
    w->updateGroupSelectBtn();
    w->updateBottomBar();
    TaskChainClipboardHelper::updateGroupCardSelectionHighlight(w);
    w->scheduleCacheWrite();
    if (w->m_listLocked) {
        w->installLockFilter(w->m_groupList, true);
    }
}

void TaskChainListSync::createGroupRowWidget(TaskChainWidget *w, QListWidgetItem *item, int groupIndex, bool checked)
{
    TaskChainGroupCardWidget::buildGroupRowWidget(w, item, groupIndex, checked);
}

void TaskChainListSync::refreshActionTags(TaskChainWidget *w)
{
    if (!w || !w->m_actionTagList) return;
    w->m_actionTagList->clear();
    for (const auto &def : w->m_actionDefs) {
        auto *item = new QListWidgetItem(def.name, w->m_actionTagList);
        item->setToolTip(def.category);
    }
}

void TaskChainListSync::updateStatus(TaskChainWidget *w, const QString &text)
{
    if (w && w->m_chainStatus) {
        w->m_chainStatus->setText(text);
    }
}

void TaskChainListSync::updateTaskSelectBtn(TaskChainWidget *w)
{
    if (!w || !w->m_taskSelectAllBtn) return;
    w->m_taskSelectAllBtn->setText(allCheckBoxesChecked(w->m_taskList)
        ? QStringLiteral("取消选中") : QStringLiteral("全选任务"));
}

void TaskChainListSync::updateGroupSelectBtn(TaskChainWidget *w)
{
    if (!w || !w->m_groupSelectAllBtn) return;
    w->m_groupSelectAllBtn->setText(allCheckBoxesChecked(w->m_groupList)
        ? QStringLiteral("取消选中") : QStringLiteral("全选组"));
}

void TaskChainListSync::updateBottomBar(TaskChainWidget *w)
{
    if (!w || !w->m_bottomInfoLabel) return;
    int idx = w->currentChainIndex();
    QString chainName = (idx >= 0 && idx < w->m_chains.size()) ? w->m_chains[idx].name : QString();
    QString taskName;
    QString groupName;

    if (w->m_taskList) {
        auto *taskItem = w->m_taskList->currentItem();
        if (taskItem) {
            int taskIdx = taskItem->data(Qt::UserRole).toInt();
            if (idx >= 0 && idx < w->m_chains.size() && taskIdx >= 0 && taskIdx < w->m_chains[idx].tasks.size()) {
                QJsonObject task = w->m_chains[idx].tasks[taskIdx].toObject();
                taskName = task.value("name").toString();
            }
        }
    }

    if (w->m_groupList) {
        auto *groupItem = w->m_groupList->currentItem();
        if (groupItem) {
            int groupIdx = groupItem->data(Qt::UserRole).toInt();
            QJsonArray groups = w->currentTaskGroups();
            if (groupIdx >= 0 && groupIdx < groups.size()) {
                QJsonObject group = groups[groupIdx].toObject();
                groupName = group.value("name").toString();
            }
        }
    }

    if (!w->m_currentTaskName.isEmpty())  taskName  = w->m_currentTaskName;
    if (!w->m_currentGroupName.isEmpty()) groupName = w->m_currentGroupName;

    QString statusText;
    switch (w->m_execStatus) {
    case TaskChainWidget::NotStarted:  statusText = QStringLiteral("未开始"); break;
    case TaskChainWidget::Running:     statusText = QStringLiteral("执行中"); break;
    case TaskChainWidget::Suspended:   statusText = QStringLiteral("暂停中"); break;
    case TaskChainWidget::Completed:   statusText = QStringLiteral("已完成"); break;
    }

    QStringList parts;
    if (!chainName.isEmpty()) parts << QStringLiteral("任务链: %1 [%2]").arg(chainName, statusText);
    if (!taskName.isEmpty())   parts << QStringLiteral("任务: %1").arg(taskName);
    if (!groupName.isEmpty()) parts << QStringLiteral("组: %1").arg(groupName);
    if (parts.isEmpty()) parts << QStringLiteral("未选择任务链");

    w->m_bottomInfoLabel->setText(parts.join("  |  "));

    bool isWorking = (w->m_chainWorking || w->m_execStatus == TaskChainWidget::Running ||
                      w->m_execStatus == TaskChainWidget::Suspended || w->m_listLocked ||
                      TaskChainManager::instance().isWorking() || TaskChainManager::instance().isLocalExecutionActive());
    if (w->m_suspendResumeBtn) {
        w->m_suspendResumeBtn->setVisible(isWorking);
        w->m_suspendResumeBtn->setEnabled(isWorking);
        w->m_suspendResumeBtn->setText((w->m_chainPaused || w->m_execStatus == TaskChainWidget::Suspended)
                                       ? QStringLiteral("继续任务链") : QStringLiteral("暂停任务链"));
    }

    if (w->m_cancelBtn) {
        w->m_cancelBtn->setEnabled(isWorking);
    }
    if (w->m_executeNextBtn) {
        w->m_executeNextBtn->setEnabled(isWorking && !chainName.isEmpty());
    }
}
