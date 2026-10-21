/**
 * @file    MotionDialogController.h
 * @layer   GUILayer
 * @brief   Navigation and rotation command dispatcher and status observer for MotionDialog.
 * @deps    none
 * @note    Refactored for AI readability; API and motion semantics invariant.
 */

#pragma once

class MotionDialog;
struct RobotInfo;

class MotionDialogController
{
public:
    static void onSendClicked(MotionDialog *dlg);
    static void onCancelNavClicked(MotionDialog *dlg);
    static void onPauseNavClicked(MotionDialog *dlg);
    static void onCurrentRobotInfoUpdated(MotionDialog *dlg, const RobotInfo &info);
    static void onMotionTimeout(MotionDialog *dlg);
};
