/**
 * @file    TaskChainCrudHandler.h
 * @layer   GUILayer
 * @brief   Handles creation, deletion, selection, loading, and saving of task chains, tasks, and groups.
 * @deps    TaskChainWidget.h, QListWidgetItem
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

class TaskChainWidget;
class QListWidgetItem;

class TaskChainCrudHandler
{
public:
    // 面板1: 任务链 CRUD
    static void onChainNew(TaskChainWidget *w);
    static void onChainLoad(TaskChainWidget *w);
    static void onChainSave(TaskChainWidget *w);
    static void onChainExport(TaskChainWidget *w);
    static void onChainItemChanged(TaskChainWidget *w, QListWidgetItem *current);

    // 面板2: 任务 CRUD
    static void onTaskNew(TaskChainWidget *w);
    static void onTaskDelete(TaskChainWidget *w);
    static void onTaskSelectAll(TaskChainWidget *w);

    // 面板3: 组 CRUD
    static void onGroupAdd(TaskChainWidget *w);
    static void onGroupInsert(TaskChainWidget *w);
    static void onGroupDelete(TaskChainWidget *w);
    static void onGroupSelectAll(TaskChainWidget *w);
};
