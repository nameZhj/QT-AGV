/**
 * @file    TaskChainGroupActionHandler.h
 * @layer   GUILayer
 * @brief   Handles drag-and-drop of actions into groups, context menus, and parameter dialog invocation.
 * @deps    TaskChainWidget.h, TaskChainActionDefs.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QString>
#include <QPoint>
#include <QJsonObject>
#include "TaskChainActionDefs.h"

class TaskChainWidget;

class TaskChainGroupActionHandler
{
public:
    static void handleActionDropped(TaskChainWidget *w, const QString &actionName, int groupIdx);
    static void showGroupContextMenu(TaskChainWidget *w, const QPoint &pos);
    static QString openActionDefDialog(TaskChainWidget *w, const ActionDef &def, const QJsonObject &existingParams);
};
