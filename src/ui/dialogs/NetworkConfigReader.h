/**
 * @file    NetworkConfigReader.h
 * @layer   GUILayer
 * @brief   Remote execution routine for reading Ubuntu network parameters via SSH.
 * @deps    none
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

class NetworkConfigDialog;

class NetworkConfigReader
{
public:
    static void readConfig(NetworkConfigDialog *dlg);
};
