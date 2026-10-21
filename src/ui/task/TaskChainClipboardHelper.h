/**
 * @file    TaskChainClipboardHelper.h
 * @layer   GUI
 * @brief   Helper for task/group clipboard operations (copy/paste) and reordering.
 * @deps    TaskChainWidget.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

class TaskChainWidget;
class QKeyEvent;

class TaskChainClipboardHelper
{
public:
    static void reorderGroup(TaskChainWidget *owner, int fromIdx, int toIdx);
    static void reorderTask(TaskChainWidget *owner, int fromIdx, int toIdx);
    static void copySelectedGroup(TaskChainWidget *owner);
    static void pasteGroup(TaskChainWidget *owner);
    static void updateGroupCardSelectionHighlight(TaskChainWidget *owner);
    static bool handleKeyPress(TaskChainWidget *owner, QKeyEvent *event);
};
