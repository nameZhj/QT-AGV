/**
 * @file    AudioManagerDialog.h
 * @layer   GUILayer
 * @brief   Audio file manager and sound notification settings dialog.
 * @deps    none
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QDialog>
#include <QStringList>

class QLabel;
class QListWidget;
class QPushButton;
class QMediaPlayer;
class QAudioOutput;

/**
 * @brief 音频管理器弹窗：从服务端拉取音频列表，支持上传/下载/删除与在线播放控制。
 *
 * 接口 (端口 8048):
 *  - GET  /robot/audio/list                    音频列表
 *  - POST /robot/audio/upload                  上传音频 (multipart, 字段 file)
 *  - GET  /robot/audio/download/{name}         下载音频
 * 下载的文件统一缓存到 exe 同级 mediaFile 目录，播放时优先使用本地缓存。
 */
class AudioManagerDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AudioManagerDialog(QWidget *parent = nullptr);
    ~AudioManagerDialog() override;

private slots:
    void onRefreshList();
    void onUpload();
    void onDownload();
    void onDeleteSelected();
    void onOpenDirectory();
    void onPlay();
    void onPause();
    void onContinue();
    void onStop();

private:
    enum class PlayState { Stopped, Playing, Paused };

    void setupUi();
    void loadAudioList();
    void updateControlStates();
    void setPlayState(PlayState state);
    void playLocal(const QString &name, const QString &filePath);
    void stopPlayback();

    static QString mediaDir();
    static QString cachedPath(const QString &name);
    QString currentAudioName() const;

    QLabel      *m_statusLabel = nullptr;
    QListWidget *m_listWidget  = nullptr;
    QPushButton *m_playBtn     = nullptr;
    QPushButton *m_pauseBtn    = nullptr;
    QPushButton *m_continueBtn = nullptr;
    QPushButton *m_stopBtn     = nullptr;
    QPushButton *m_deleteBtn   = nullptr;

    QMediaPlayer *m_player    = nullptr;
    QAudioOutput *m_audioOut  = nullptr;
    QString       m_playingName;
    PlayState     m_playState = PlayState::Stopped;

    friend class AudioManagerController;
};
