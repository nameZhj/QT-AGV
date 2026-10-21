/**
 * @file    TaskChainActionExecutor.h
 * @layer   GUI
 * @brief   Executor for single actions and batch task chain execution requests.
 * @deps    QJsonObject, TaskChainWidget.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QJsonObject>

class TaskChainWidget;

class TaskChainActionExecutor
{
public:
    static void executeSingleAction(TaskChainWidget *owner, const QJsonObject &action);
    static void sendTaskChain(TaskChainWidget *owner, bool onlyChecked);
};
