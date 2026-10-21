/**
 * @file    TaskChainPanelsBuilder.h
 * @layer   GUI
 * @brief   Builder for assembling the four workflow panels and bottom control bar.
 * @deps    TaskChainWidget.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QString>

class TaskChainWidget;
class QVBoxLayout;
class QWidget;
class QPushButton;

class TaskChainPanelsBuilder
{
public:
    static QPushButton *makeBtn(const QString &text);
    static void buildPanels(TaskChainWidget *owner, QWidget *panelsContainer);
    static void setupPanel1Chain(TaskChainWidget *owner, QVBoxLayout *layout);
    static void setupPanel2Task(TaskChainWidget *owner, QVBoxLayout *layout);
    static void setupPanel3Group(TaskChainWidget *owner, QVBoxLayout *layout);
    static void setupPanel4DragDrop(TaskChainWidget *owner, QVBoxLayout *layout);
    static void setupBottomBar(TaskChainWidget *owner, QVBoxLayout *layout);
    static void setupLockOverlay(TaskChainWidget *owner, QWidget *panelsContainer);
};
