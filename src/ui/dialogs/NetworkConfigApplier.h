/**
 * @file    NetworkConfigApplier.h
 * @layer   GUILayer
 * @brief   Remote execution routine for applying and testing Ubuntu network configuration changes.
 * @deps    none
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

class NetworkConfigDialog;

class NetworkConfigApplier
{
public:
    static void applyConfig(NetworkConfigDialog *dlg);
};
