/**
 * @file    EditConfigDataLoader.h
 * @layer   GUILayer
 * @brief   Loads, parses, saves, downloads, and uploads configuration files and metadata for EditConfigWidget.
 * @deps    EditConfigWidget.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QByteArray>

class EditConfigWidget;

class EditConfigDataLoader
{
public:
    static void loadAll(EditConfigWidget *w);
    static void parseMetadata(EditConfigWidget *w, const QByteArray &rawJson);
    static void onSave(EditConfigWidget *w);
    static void onDownload(EditConfigWidget *w);
    static void onUpload(EditConfigWidget *w);
};
