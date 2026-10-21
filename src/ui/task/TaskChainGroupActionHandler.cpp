/**
 * @file    TaskChainGroupActionHandler.cpp
 * @layer   GUILayer
 * @brief   Implementation of action dropping, group context menus, and parameter dialog.
 * @deps    TaskChainGroupActionHandler.h, TaskChainWidget.h, TaskChainClipboardHelper.h, TaskActionParamDialog.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "TaskChainGroupActionHandler.h"
#include "TaskChainWidget.h"
#include "TaskChainClipboardHelper.h"
#include "TaskActionParamDialog.h"
#include "core/TaskChainManager.h"
#include "core/MotionConfigManager.h"
#include <QMessageBox>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QListWidget>

void TaskChainGroupActionHandler::handleActionDropped(TaskChainWidget *w, const QString &actionName, int groupIdx)
{
    if (!w) return;
    if (w->m_currentTaskIndex < 0) {
        QMessageBox::warning(w, QStringLiteral("提示"), QStringLiteral("请先选择一个任务"));
        return;
    }

    QJsonArray groups = w->currentTaskGroups();
    if (groups.isEmpty()) {
        QJsonObject newGroup;
        newGroup["name"] = QStringLiteral("group1");
        newGroup["key"] = "1";
        groups.append(newGroup);
        w->setCurrentTaskGroups(groups);
    }

    if (groupIdx < 0 || groupIdx >= groups.size()) {
        groupIdx = groups.size() - 1;
    }

    QJsonObject group = groups[groupIdx].toObject();
    QString groupActionType = group.value("actionType").toString();

    if (groupActionType.isEmpty()) {
        group["actionType"] = actionName;
    } else if (groupActionType != actionName) {
        QMessageBox::warning(w, QStringLiteral("提示"),
            QStringLiteral("每个组只能添加一种标签，当前组已设置为 [%1]").arg(groupActionType));
        return;
    }

    QJsonArray actions = group.value("actions").toArray();
    QJsonObject action;
    action["name"]        = actionName;
    action["action_desc"] = actionName;
    action["action_type"] = TaskChainManager::actionTypeFromName(actionName);

    QJsonObject params;
    if (actionName == "路径导航") {
        QString stId, stName;
        if (!w->m_stations.isEmpty()) {
            stId   = w->m_stations[0].id;
            stName = w->m_stations[0].name;
        }
        action["target_station"] = stId;
        params["target_station"] = stId;
        params["target"]         = stName;
    } else if (actionName == "平动") {
        const auto &mc = MotionConfigManager::instance();
        double defVx = mc.maxSpeed();
        action["distance"]  = 1.0;
        action["vx"]        = defVx;
        action["vy"]        = 0.0;
        params["distance"]  = 1.0;
        params["vx"]        = defVx;
        params["vy"]        = 0.0;
    } else if (actionName == "转动") {
        const auto &mc = MotionConfigManager::instance();
        double defSpd = mc.maxAngularSpeedDeg();
        action["rotate_angle"] = 90.0;
        action["rotate_speed"] = defSpd;
        params["angle"]        = 90.0;
        params["speed"]        = defSpd;
    }
    action["params"] = params;
    actions.append(action);
    group["actions"] = actions;
    groups[groupIdx] = group;
    w->setCurrentTaskGroups(groups);

    w->refreshGroupList();
    w->updateStatus(QStringLiteral("已添加动作: %1").arg(actionName));
}

void TaskChainGroupActionHandler::showGroupContextMenu(TaskChainWidget *w, const QPoint &pos)
{
    if (!w || !w->m_groupList) return;
    if (w->m_listLocked || w->m_execStatus == TaskChainWidget::Running) return;
    auto *item = w->m_groupList->itemAt(pos);
    if (item) w->m_groupList->setCurrentItem(item);

    QMenu menu(w);
    menu.setStyleSheet(
        "QMenu { background:#1f232b; border:1px solid #2d3340; color:#c0c8d0; padding:4px; }"
        "QMenu::item { padding:6px 20px; border-radius:3px; }"
        "QMenu::item:selected { background:#2a5d8f; color:#fff; }"
        "QMenu::item:disabled { color:#555d6b; }");

    auto *copyAct = menu.addAction(QStringLiteral("复制组 (Ctrl+C)"));
    copyAct->setEnabled(w->m_groupList->count() > 0 && (item != nullptr || w->m_groupList->currentRow() >= 0));
    QObject::connect(copyAct, &QAction::triggered, w, [w]() {
        TaskChainClipboardHelper::copySelectedGroup(w);
    });

    auto *pasteAct = menu.addAction(QStringLiteral("粘贴组 (Ctrl+V)"));
    pasteAct->setEnabled(!w->m_copiedGroups.isEmpty() || (QApplication::clipboard()->mimeData() && QApplication::clipboard()->mimeData()->hasFormat("application/x-taskchain-groups")));
    QObject::connect(pasteAct, &QAction::triggered, w, [w]() {
        TaskChainClipboardHelper::pasteGroup(w);
    });

    menu.addSeparator();

    auto *insertAct = menu.addAction(QStringLiteral("插入新组"));
    QObject::connect(insertAct, &QAction::triggered, w, &TaskChainWidget::onGroupInsert);

    auto *delAct = menu.addAction(QStringLiteral("删除选中组"));
    delAct->setEnabled(item != nullptr || w->m_groupList->currentRow() >= 0);
    QObject::connect(delAct, &QAction::triggered, w, &TaskChainWidget::onGroupDelete);

    menu.exec(w->m_groupList->mapToGlobal(pos));
}

QString TaskChainGroupActionHandler::openActionDefDialog(TaskChainWidget *w, const ActionDef &def, const QJsonObject &existingParams)
{
    if (!w) return QString();
    return TaskActionParamDialog::editActionParams(def, existingParams, w->m_stations, &w->m_audioFiles, w);
}
