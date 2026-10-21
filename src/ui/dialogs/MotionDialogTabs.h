/**
 * @file    MotionDialogTabs.h
 * @layer   GUILayer
 * @brief   Builder for translation and rotation tabs in MotionDialog.
 * @deps    none
 * @note    Refactored for AI readability; visual layout & widget hierarchy invariant.
 */

#pragma once

class QWidget;
class MotionDialog;

class MotionDialogTabs
{
public:
    static QWidget *createTranslationTab(MotionDialog *dlg);
    static QWidget *createRotationTab(MotionDialog *dlg);
    static void updateParamToolTips(MotionDialog *dlg);
};
