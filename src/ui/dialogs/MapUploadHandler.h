/**
 * @file    MapUploadHandler.h
 * @layer   GUILayer
 * @brief   Helper logic for sibling file detection and API upload in MapUploadDialog.
 * @deps    MapUploadDialog.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QString>

class MapUploadDialog;

class MapUploadHandler
{
public:
    static void autoDetectSiblings(MapUploadDialog *dlg, const QString &chosenPath);
    static void executeUpload(MapUploadDialog *dlg);
};
