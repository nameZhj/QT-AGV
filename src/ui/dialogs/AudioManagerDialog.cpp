/**
 * @file    AudioManagerDialog.cpp
 * @layer   GUILayer
 * @brief   Implementation of audio file manager and notification dialog.
 * @deps    AudioManagerDialog.h, AudioManagerStyle.h, AudioManagerController.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "AudioManagerDialog.h"
#include "AudioManagerStyle.h"
#include "AudioManagerController.h"
#include "ui/common/Toast.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QToolButton>
#include <QMediaPlayer>
#include <QAudioOutput>

AudioManagerDialog::AudioManagerDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("音频管理器");
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);
    setFixedSize(560, 430);

    m_player = new QMediaPlayer(this);
    m_audioOut = new QAudioOutput(this);
    m_audioOut->setVolume(0.8);
    m_player->setAudioOutput(m_audioOut);

    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus st) {
        if (st == QMediaPlayer::EndOfMedia) {
            m_statusLabel->setText("播放结束");
            setPlayState(PlayState::Stopped);
        } else if (st == QMediaPlayer::InvalidMedia) {
            m_statusLabel->setText(QString("音频无法播放: %1").arg(m_playingName));
            Toast::error(this, "音频无法播放，请确认文件格式为.wav");
            setPlayState(PlayState::Stopped);
        }
    });
    connect(m_player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString &err) {
        m_statusLabel->setText(QString("音频播放失败: %1").arg(m_playingName));
        Toast::error(this, QString("音频播放失败: %1").arg(err));
        setPlayState(PlayState::Stopped);
    });

    setupUi();
}

AudioManagerDialog::~AudioManagerDialog()
{
    if (m_player) {
        m_player->stop();
    }
}

void AudioManagerDialog::setupUi()
{
    setStyleSheet(
        "QDialog { background: #111827; color: #e5e7eb; }"
        "QLabel { background: transparent; }"
    );
    setStyleSheet(styleSheet() + AudioManagerStyle::kListStyle);

    auto *rootLay = new QVBoxLayout(this);
    rootLay->setContentsMargins(10, 8, 10, 10);
    rootLay->setSpacing(8);

    m_statusLabel = new QLabel("获取音频列表完成", this);
    m_statusLabel->setStyleSheet("color:#9aa1ad; font-size:12px;");
    rootLay->addWidget(m_statusLabel);

    auto *noteLabel = new QLabel(
        "<span style='color:#e879f9; font-weight:bold;'>注:</span>"
        "<span style='color:#e5e7eb;'> 仅支持.wav格式音频文件.</span>", this);
    noteLabel->setTextFormat(Qt::RichText);
    noteLabel->setStyleSheet("font-size:12px;");
    rootLay->addWidget(noteLabel);

    auto *headRow = new QHBoxLayout;
    headRow->setSpacing(6);

    auto *listTitle = new QLabel("音频列表:", this);
    listTitle->setStyleSheet("color:#e5e7eb; font-size:12px;");
    headRow->addWidget(listTitle);

    auto *refreshBtn = new QToolButton(this);
    refreshBtn->setIcon(AudioManagerStyle::makeRefreshIcon());
    refreshBtn->setIconSize(QSize(20, 20));
    refreshBtn->setFixedSize(24, 24);
    refreshBtn->setCursor(Qt::PointingHandCursor);
    refreshBtn->setStyleSheet(AudioManagerStyle::kIconButtonStyle);
    refreshBtn->setToolTip("刷新音频列表");
    connect(refreshBtn, &QToolButton::clicked, this, &AudioManagerDialog::onRefreshList);
    headRow->addWidget(refreshBtn);

    headRow->addStretch();

    auto *openDirBtn = new QPushButton("打开文件目录", this);
    openDirBtn->setCursor(Qt::PointingHandCursor);
    openDirBtn->setStyleSheet(AudioManagerStyle::kLinkButtonStyle);
    {
        QFont f = openDirBtn->font();
        f.setUnderline(true);
        openDirBtn->setFont(f);
    }
    connect(openDirBtn, &QPushButton::clicked, this, &AudioManagerDialog::onOpenDirectory);
    headRow->addWidget(openDirBtn);

    auto *uploadBtn = new QPushButton("上传", this);
    uploadBtn->setCursor(Qt::PointingHandCursor);
    uploadBtn->setStyleSheet(AudioManagerStyle::kButtonStyle);
    uploadBtn->setToolTip("上传本地.wav音频文件");
    connect(uploadBtn, &QPushButton::clicked, this, &AudioManagerDialog::onUpload);
    headRow->addWidget(uploadBtn);

    auto *downloadBtn = new QPushButton("下载", this);
    downloadBtn->setCursor(Qt::PointingHandCursor);
    downloadBtn->setStyleSheet(AudioManagerStyle::kButtonStyle);
    downloadBtn->setToolTip("下载选中音频到 mediaFile 目录");
    connect(downloadBtn, &QPushButton::clicked, this, &AudioManagerDialog::onDownload);
    headRow->addWidget(downloadBtn);

    m_deleteBtn = new QPushButton(this);
    m_deleteBtn->setIcon(AudioManagerStyle::makeTrashIcon());
    m_deleteBtn->setIconSize(QSize(20, 20));
    m_deleteBtn->setCursor(Qt::PointingHandCursor);
    m_deleteBtn->setStyleSheet(AudioManagerStyle::kButtonStyle + QString("QPushButton { padding: 4px 8px; }"));
    m_deleteBtn->setToolTip("删除选中音频");
    connect(m_deleteBtn, &QPushButton::clicked, this, &AudioManagerDialog::onDeleteSelected);
    headRow->addWidget(m_deleteBtn);
    rootLay->addLayout(headRow);

    m_listWidget = new QListWidget(this);
    connect(m_listWidget, &QListWidget::itemSelectionChanged, this, &AudioManagerDialog::updateControlStates);
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) { onPlay(); });
    rootLay->addWidget(m_listWidget, 1);

    auto *bottomRow = new QHBoxLayout;
    bottomRow->setSpacing(8);
    bottomRow->addStretch();

    auto makeCtrlBtn = [&](const QString &text) {
        auto *b = new QPushButton(text, this);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(AudioManagerStyle::kButtonStyle);
        bottomRow->addWidget(b);
        return b;
    };
    m_playBtn     = makeCtrlBtn("播放");
    m_pauseBtn    = makeCtrlBtn("暂停");
    m_continueBtn = makeCtrlBtn("继续");
    m_stopBtn     = makeCtrlBtn("停止");

    connect(m_playBtn,     &QPushButton::clicked, this, &AudioManagerDialog::onPlay);
    connect(m_pauseBtn,    &QPushButton::clicked, this, &AudioManagerDialog::onPause);
    connect(m_continueBtn, &QPushButton::clicked, this, &AudioManagerDialog::onContinue);
    connect(m_stopBtn,     &QPushButton::clicked, this, &AudioManagerDialog::onStop);
    rootLay->addLayout(bottomRow);

    loadAudioList();
}

QString AudioManagerDialog::mediaDir()
{
    return AudioManagerController::mediaDir();
}

QString AudioManagerDialog::cachedPath(const QString &name)
{
    return AudioManagerController::cachedPath(name);
}

QString AudioManagerDialog::currentAudioName() const
{
    auto *item = m_listWidget ? m_listWidget->currentItem() : nullptr;
    return item ? item->text() : QString();
}

void AudioManagerDialog::loadAudioList()
{
    AudioManagerController::loadAudioList(this);
}

void AudioManagerDialog::updateControlStates()
{
    const bool hasSel = (m_listWidget && m_listWidget->currentItem() != nullptr);
    if (m_deleteBtn)   m_deleteBtn->setEnabled(hasSel);
    if (m_playBtn)     m_playBtn->setEnabled(hasSel);
    if (m_pauseBtn)    m_pauseBtn->setEnabled(hasSel && m_playState == PlayState::Playing);
    if (m_continueBtn) m_continueBtn->setEnabled(hasSel && m_playState == PlayState::Paused);
    if (m_stopBtn)     m_stopBtn->setEnabled(hasSel && m_playState != PlayState::Stopped);
}

void AudioManagerDialog::setPlayState(PlayState state)
{
    m_playState = state;
    updateControlStates();
}

void AudioManagerDialog::playLocal(const QString &name, const QString &filePath)
{
    AudioManagerController::playLocal(this, name, filePath);
}

void AudioManagerDialog::stopPlayback()
{
    AudioManagerController::stopPlayback(this);
}

void AudioManagerDialog::onRefreshList()
{
    AudioManagerController::onRefreshList(this);
}

void AudioManagerDialog::onUpload()
{
    AudioManagerController::onUpload(this);
}

void AudioManagerDialog::onDownload()
{
    AudioManagerController::onDownload(this);
}

void AudioManagerDialog::onDeleteSelected()
{
    AudioManagerController::onDeleteSelected(this);
}

void AudioManagerDialog::onOpenDirectory()
{
    AudioManagerController::onOpenDirectory(this);
}

void AudioManagerDialog::onPlay()
{
    AudioManagerController::onPlay(this);
}

void AudioManagerDialog::onPause()
{
    AudioManagerController::onPause(this);
}

void AudioManagerDialog::onContinue()
{
    AudioManagerController::onContinue(this);
}

void AudioManagerDialog::onStop()
{
    AudioManagerController::onStop(this);
}
