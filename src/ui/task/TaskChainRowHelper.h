/**
 * @file    TaskChainRowHelper.h
 * @layer   GUI
 * @brief   Row widget factory for task chain list items and sub-task cards.
 * @deps    QWidget, QListWidgetItem, QJsonObject
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QWidget>
#include <QJsonObject>

class QListWidgetItem;
class TaskChainWidget;

class TaskChainRowHelper
{
public:
    static void buildChainRowWidget(TaskChainWidget *owner,
                                    QListWidgetItem *item,
                                    int chainIndex);

    static void buildTaskRowWidget(TaskChainWidget *owner,
                                   QListWidgetItem *item,
                                   int taskIndex,
                                   const QJsonObject &task);
};
