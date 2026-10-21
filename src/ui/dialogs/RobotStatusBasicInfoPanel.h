/**
 * @file    RobotStatusBasicInfoPanel.h
 * @layer   GUILayer
 * @brief   Builder for the 4-column basic info scroll panel in RobotStatusDialog.
 * @deps    none
 * @note    Refactored for AI readability; visual layout & widget hierarchy invariant.
 */

#pragma once

class QWidget;
class RobotStatusDialog;

class RobotStatusBasicInfoPanel
{
public:
    static QWidget *createBasicInfoTab(RobotStatusDialog *dlg);

private:
    static QWidget *buildCol1(RobotStatusDialog *dlg);
    static QWidget *buildCol2(RobotStatusDialog *dlg);
    static QWidget *buildCol3(RobotStatusDialog *dlg);
    static QWidget *buildCol4(RobotStatusDialog *dlg);
};
