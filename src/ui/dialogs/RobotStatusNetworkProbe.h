/**
 * @file    RobotStatusNetworkProbe.h
 * @layer   GUILayer
 * @brief   Network interface and Wi-Fi signal diagnostic probe for RobotStatusDialog.
 * @deps    none
 * @note    Refactored for AI readability; runtime behavior & diagnostic logic invariant.
 */

#pragma once

class RobotStatusDialog;

class RobotStatusNetworkProbe
{
public:
    static void refreshNetworkAndWifiInfo(RobotStatusDialog *dlg);
};
