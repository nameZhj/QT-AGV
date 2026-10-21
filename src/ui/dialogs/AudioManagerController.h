/**
 * @file    AudioManagerController.h
 * @layer   GUILayer
 * @brief   Controller for audio file management (list, upload, download, delete, play).
 * @deps    none
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QString>

class AudioManagerDialog;

class AudioManagerController
{
public:
    static QString mediaDir();
    static QString cachedPath(const QString &name);

    static void loadAudioList(AudioManagerDialog *dlg);
    static void onRefreshList(AudioManagerDialog *dlg);
    static void onUpload(AudioManagerDialog *dlg);
    static void onDownload(AudioManagerDialog *dlg);
    static void onDeleteSelected(AudioManagerDialog *dlg);
    static void onOpenDirectory(AudioManagerDialog *dlg);

    static void onPlay(AudioManagerDialog *dlg);
    static void playLocal(AudioManagerDialog *dlg, const QString &name, const QString &filePath);
    static void stopPlayback(AudioManagerDialog *dlg);
    static void onPause(AudioManagerDialog *dlg);
    static void onContinue(AudioManagerDialog *dlg);
    static void onStop(AudioManagerDialog *dlg);
};
