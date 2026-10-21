/**
 * @file    AudioManagerController.cpp
 * @layer   GUILayer
 * @brief   Implementation of audio list querying, uploading, downloading, and playback.
 * @deps    AudioManagerController.h, AudioManagerDialog.h, ApiService.h, Toast.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "AudioManagerController.h"
#include "AudioManagerDialog.h"
#include "network/ApiService.h"
#include "ui/common/Toast.h"

#include <QLabel>
#include <QListWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QJsonArray>
#include <QJsonValue>
#include <QMediaPlayer>

QString AudioManagerController::mediaDir()
{
    const QString dir = QCoreApplication::applicationDirPath() + "/mediaFile";
    QDir().mkpath(dir);
    return dir;
}

QString AudioManagerController::cachedPath(const QString &name)
{
    return mediaDir() + "/" + QFileInfo(name).fileName();
}

void AudioManagerController::loadAudioList(AudioManagerDialog *dlg)
{
    if (!dlg) return;
    dlg->m_statusLabel->setText("正在获取音频列表...");

    ApiService::instance().listAudio([dlg](int code, const QJsonObject &resp) {
        if (code != 200) {
            dlg->m_listWidget->clear();
            dlg->m_statusLabel->setText(QString("获取音频列表失败 (code=%1)").arg(code));
            Toast::error(dlg, QString("获取音频列表失败: %1")
                .arg(resp.value("msg").toString(QStringLiteral("请求失败"))));
            dlg->updateControlStates();
            return;
        }

        dlg->m_listWidget->clear();
        const QJsonArray rows = resp.value("data").toArray();
        for (const QJsonValue &v : rows) {
            const QString name = QFileInfo(v.toString().trimmed()).fileName();
            if (!name.isEmpty()) dlg->m_listWidget->addItem(name);
        }
        dlg->m_statusLabel->setText("获取音频列表完成");
        dlg->updateControlStates();
    });
}

void AudioManagerController::onRefreshList(AudioManagerDialog *dlg)
{
    if (!dlg) return;
    stopPlayback(dlg);
    loadAudioList(dlg);
}

void AudioManagerController::onUpload(AudioManagerDialog *dlg)
{
    if (!dlg) return;
    const QString filePath = QFileDialog::getOpenFileName(
        dlg, QStringLiteral("选择要上传的音频文件"), QString(),
        QStringLiteral("音频文件 (*.wav)"));
    if (filePath.isEmpty()) return;

    dlg->m_statusLabel->setText(QString("正在上传: %1").arg(QFileInfo(filePath).fileName()));
    ApiService::instance().uploadAudio(filePath, [dlg](int code, const QJsonObject &resp) {
        if (code == 200) {
            Toast::success(dlg, "音频上传成功");
            loadAudioList(dlg);
        } else {
            dlg->m_statusLabel->setText(QString("音频上传失败 (code=%1)").arg(code));
            Toast::error(dlg, QString("音频上传失败: %1")
                .arg(resp.value("msg").toString(QStringLiteral("请求失败"))));
        }
    });
}

void AudioManagerController::onDownload(AudioManagerDialog *dlg)
{
    if (!dlg) return;
    const QString name = dlg->currentAudioName();
    if (name.isEmpty()) {
        Toast::info(dlg, "请先选择要下载的音频");
        return;
    }

    dlg->m_statusLabel->setText(QString("正在下载: %1").arg(name));
    ApiService::instance().downloadAudio(name, [dlg, name](bool ok, const QByteArray &data) {
        if (!ok || data.isEmpty()) {
            dlg->m_statusLabel->setText(QString("音频下载失败: %1").arg(name));
            Toast::error(dlg, QString("音频下载失败: %1").arg(name));
            return;
        }
        const QString path = cachedPath(name);
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) {
            dlg->m_statusLabel->setText("音频本地写入失败");
            Toast::error(dlg, "无法写入本地缓存目录 mediaFile");
            return;
        }
        f.write(data);
        f.close();
        dlg->m_statusLabel->setText(QString("已下载: %1").arg(name));
        Toast::success(dlg, QString("已下载到 mediaFile: %1").arg(name));
    });
}

void AudioManagerController::onDeleteSelected(AudioManagerDialog *dlg)
{
    if (!dlg) return;
    const QString name = dlg->currentAudioName();
    if (name.isEmpty()) {
        Toast::info(dlg, "请先选择要删除的音频");
        return;
    }

    dlg->m_statusLabel->setText(QString("正在删除: %1").arg(name));
    ApiService::instance().deleteAudio(name, [dlg, name](int code, const QJsonObject &resp) {
        if (code == 200) {
            const QString path = cachedPath(name);
            if (QFile::exists(path)) QFile::remove(path);

            if (dlg->m_playingName == name) stopPlayback(dlg);
            dlg->m_statusLabel->setText(QString("已删除: %1").arg(name));
            Toast::success(dlg, resp.value("msg").toString(QStringLiteral("删除成功")));
            loadAudioList(dlg);
        } else {
            dlg->m_statusLabel->setText(QString("删除失败 (code=%1)").arg(code));
            Toast::error(dlg, QString("删除失败: %1")
                .arg(resp.value("msg").toString(QStringLiteral("请求失败"))));
        }
    });
}

void AudioManagerController::onOpenDirectory(AudioManagerDialog *dlg)
{
    if (!dlg) return;
    const QString dir = mediaDir();
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(dir))) {
        Toast::error(dlg, QString("打开目录失败: %1").arg(QDir::toNativeSeparators(dir)));
    } else {
        dlg->m_statusLabel->setText(QString("音频缓存目录: %1").arg(QDir::toNativeSeparators(dir)));
    }
}

void AudioManagerController::onPlay(AudioManagerDialog *dlg)
{
    if (!dlg) return;
    const QString name = dlg->currentAudioName();
    if (name.isEmpty()) {
        Toast::info(dlg, "请先选择要播放的音频");
        return;
    }

    const QString path = cachedPath(name);
    if (QFile::exists(path)) {
        playLocal(dlg, name, path);
        return;
    }

    dlg->m_statusLabel->setText(QString("正在获取音频: %1").arg(name));
    ApiService::instance().downloadAudio(name, [dlg, name](bool ok, const QByteArray &data) {
        if (!ok || data.isEmpty()) {
            dlg->m_statusLabel->setText(QString("音频获取失败: %1").arg(name));
            Toast::error(dlg, QString("音频获取失败: %1").arg(name));
            return;
        }
        const QString path = cachedPath(name);
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) {
            dlg->m_statusLabel->setText("音频本地写入失败");
            Toast::error(dlg, "无法写入本地缓存目录 mediaFile");
            return;
        }
        f.write(data);
        f.close();
        playLocal(dlg, name, path);
    });
}

void AudioManagerController::playLocal(AudioManagerDialog *dlg, const QString &name, const QString &filePath)
{
    if (!dlg || !dlg->m_player) return;
    dlg->m_playingName = name;
    dlg->m_player->setSource(QUrl::fromLocalFile(filePath));
    dlg->m_player->play();
    dlg->setPlayState(AudioManagerDialog::PlayState::Playing);
    dlg->m_statusLabel->setText(QString("正在播放: %1").arg(name));
}

void AudioManagerController::stopPlayback(AudioManagerDialog *dlg)
{
    if (!dlg) return;
    if (dlg->m_player) dlg->m_player->stop();
    dlg->m_playingName.clear();
    dlg->setPlayState(AudioManagerDialog::PlayState::Stopped);
}

void AudioManagerController::onPause(AudioManagerDialog *dlg)
{
    if (!dlg || !dlg->m_player) return;
    if (dlg->m_playState != AudioManagerDialog::PlayState::Playing) return;
    dlg->m_player->pause();
    dlg->setPlayState(AudioManagerDialog::PlayState::Paused);
    dlg->m_statusLabel->setText(QString("已暂停: %1").arg(dlg->m_playingName));
}

void AudioManagerController::onContinue(AudioManagerDialog *dlg)
{
    if (!dlg || !dlg->m_player) return;
    if (dlg->m_playState != AudioManagerDialog::PlayState::Paused) return;
    dlg->m_player->play();
    dlg->setPlayState(AudioManagerDialog::PlayState::Playing);
    dlg->m_statusLabel->setText(QString("继续播放: %1").arg(dlg->m_playingName));
}

void AudioManagerController::onStop(AudioManagerDialog *dlg)
{
    if (!dlg) return;
    if (dlg->m_playState == AudioManagerDialog::PlayState::Stopped) return;
    stopPlayback(dlg);
    dlg->m_statusLabel->setText("已停止播放");
}
