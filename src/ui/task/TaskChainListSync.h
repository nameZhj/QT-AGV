/**
 * @file    TaskChainListSync.h
 * @layer   GUILayer
 * @brief   Synchronizes list widget views, row representations, and bottom bar status updates.
 * @deps    TaskChainWidget.h, QListWidget, QJsonArray
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QString>
#include <QJsonArray>

class TaskChainWidget;
class QListWidget;
class QListWidgetItem;

class TaskChainListSync
{
public:
    static bool allCheckBoxesChecked(QListWidget *list);
    static void setAllCheckBoxesChecked(QListWidget *list, bool checked);

    static void refreshChainList(TaskChainWidget *w);
    static void updateChainCardHighlight(TaskChainWidget *w);
    static void createChainRowWidget(TaskChainWidget *w, QListWidgetItem *item, int chainIndex);
    static void selectChain(TaskChainWidget *w, int index);
    static int  currentChainIndex(const TaskChainWidget *w);
    static QJsonArray currentTaskGroups(const TaskChainWidget *w);
    static void setCurrentTaskGroups(TaskChainWidget *w, const QJsonArray &groups);

    static void refreshTaskList(TaskChainWidget *w);
    static void refreshGroupList(TaskChainWidget *w);
    static void createGroupRowWidget(TaskChainWidget *w, QListWidgetItem *item, int groupIndex, bool checked);
    static void refreshActionTags(TaskChainWidget *w);

    static void updateStatus(TaskChainWidget *w, const QString &text);
    static void updateTaskSelectBtn(TaskChainWidget *w);
    static void updateGroupSelectBtn(TaskChainWidget *w);
    static void updateBottomBar(TaskChainWidget *w);
};
