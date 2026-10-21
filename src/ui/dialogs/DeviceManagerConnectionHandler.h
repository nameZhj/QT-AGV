/**
 * @file    DeviceManagerConnectionHandler.h
 * @layer   GUILayer
 * @brief   Handles device connection, disconnection, deletion, and SSH network configuration.
 * @deps    DeviceManagerDialog.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

class DeviceManagerDialog;

class DeviceManagerConnectionHandler
{
public:
    static void onConnectCard(DeviceManagerDialog *dlg, int index);
    static void onDisconnectCard(DeviceManagerDialog *dlg, int index);
    static void onRemoveCard(DeviceManagerDialog *dlg, int index);
    static void connectDevice(DeviceManagerDialog *dlg, int index);
    static void onNetworkConfig(DeviceManagerDialog *dlg, int index);
};
