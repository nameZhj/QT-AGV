/**
 * @file    DeviceManagerAddPanel.h
 * @layer   GUILayer
 * @brief   Constructs and manages the add-device sub-panel of DeviceManagerDialog.
 * @deps    QWidget
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

class DeviceManagerDialog;
class QWidget;

class DeviceManagerAddPanel
{
public:
    static QWidget* buildAddPanel(DeviceManagerDialog *dlg);
    static void onAddDevice(DeviceManagerDialog *dlg);
    static void onConfirmAdd(DeviceManagerDialog *dlg);
    static void onCancelAdd(DeviceManagerDialog *dlg);
};
