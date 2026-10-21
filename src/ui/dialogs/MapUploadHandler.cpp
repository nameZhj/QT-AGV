/**
 * @file    MapUploadHandler.cpp
 * @layer   GUILayer
 * @brief   Implementation of sibling file detection and API upload for MapUploadDialog.
 * @deps    MapUploadHandler.h, MapUploadDialog.h, ApiService.h, LoadingDialog.h, Toast.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapUploadHandler.h"
#include "MapUploadDialog.h"
#include "network/ApiService.h"
#include "ui/common/LoadingDialog.h"
#include "ui/common/Toast.h"

#include <QLineEdit>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QMessageBox>

void MapUploadHandler::autoDetectSiblings(MapUploadDialog *dlg, const QString &chosenPath)
{
    if (!dlg || chosenPath.isEmpty()) return;
    QFileInfo fi(chosenPath);
    QDir dir = fi.dir();
    QString stem = fi.completeBaseName();

    if (dlg->m_nameEdit->text().trimmed().isEmpty()) {
        dlg->m_nameEdit->setText(stem);
    }

    if (dlg->m_pgmEdit->text().isEmpty()) {
        const QStringList exts = {".pgm", ".jpg", ".jpeg", ".png"};
        for (const auto &ext : exts) {
            QString p = dir.filePath(stem + ext);
            if (QFile::exists(p)) { dlg->m_pgmEdit->setText(p); break; }
        }
    }

    if (dlg->m_yamlEdit->text().isEmpty()) {
        const QStringList exts = {".yaml", ".yml"};
        for (const auto &ext : exts) {
            QString p = dir.filePath(stem + ext);
            if (QFile::exists(p)) { dlg->m_yamlEdit->setText(p); break; }
        }
    }

    if (dlg->m_pbStreamEdit->text().isEmpty()) {
        QString p = dir.filePath(stem + ".pbstream");
        if (QFile::exists(p)) dlg->m_pbStreamEdit->setText(p);
    }

    if (dlg->m_reflectorBinEdit->text().isEmpty()) {
        const QStringList cands = {stem + ".bin", stem + "_reflector.bin", "reflector.bin"};
        for (const auto &c : cands) {
            QString p = dir.filePath(c);
            if (QFile::exists(p)) { dlg->m_reflectorBinEdit->setText(p); break; }
        }
    }

    if (dlg->m_reflectorJsonEdit->text().isEmpty()) {
        const QStringList cands = {stem + ".json", stem + "_reflector.json", "reflector.json"};
        for (const auto &c : cands) {
            QString p = dir.filePath(c);
            if (QFile::exists(p)) { dlg->m_reflectorJsonEdit->setText(p); break; }
        }
    }
}

void MapUploadHandler::executeUpload(MapUploadDialog *dlg)
{
    if (!dlg) return;

    QString mapName = dlg->m_nameEdit->text().trimmed();
    if (mapName.isEmpty()) {
        QMessageBox::warning(dlg, "提示", "请输入地图名称");
        dlg->m_nameEdit->setFocus();
        return;
    }

    QString pgmPath = dlg->m_pgmEdit->text().trimmed();
    if (pgmPath.isEmpty() || !QFile::exists(pgmPath)) {
        QMessageBox::warning(dlg, "提示", "请选择有效的 PGM / 图像文件");
        return;
    }

    QString yamlPath = dlg->m_yamlEdit->text().trimmed();
    if (yamlPath.isEmpty() || !QFile::exists(yamlPath)) {
        QMessageBox::warning(dlg, "提示", "请选择有效的 YAML 配置文件");
        return;
    }

    QString pbStreamPath = dlg->m_pbStreamEdit->text().trimmed();
    if (pbStreamPath.isEmpty() || !QFile::exists(pbStreamPath)) {
        QMessageBox::warning(dlg, "提示", "请选择有效的 PBStream 文件");
        return;
    }

    QString binPath = dlg->m_reflectorBinEdit->text().trimmed();
    if (!binPath.isEmpty() && !QFile::exists(binPath)) {
        QMessageBox::warning(dlg, "提示", "反光柱 BIN 文件路径无效或文件不存在");
        return;
    }

    QString jsonPath = dlg->m_reflectorJsonEdit->text().trimmed();
    if (!jsonPath.isEmpty() && !QFile::exists(jsonPath)) {
        QMessageBox::warning(dlg, "提示", "反光柱 JSON 文件路径无效或文件不存在");
        return;
    }

    ApiService::IndoorMapUploadParams params;
    params.mapName = mapName;
    params.pgmFilePath = pgmPath;
    params.yamlFilePath = yamlPath;
    params.pbStreamFilePath = pbStreamPath;
    params.reflectorBinFilePath = binPath;
    params.reflectorJsonFilePath = jsonPath;

    auto *loading = new LoadingDialog("正在上传地图文件，请稍候...", dlg);
    loading->show();

    ApiService::instance().uploadIndoorMap(params, [dlg, loading, mapName](int code, const QJsonObject &resp) {
        loading->close();
        loading->deleteLater();

        if (code == 200) {
            Toast::success(dlg->parentWidget() ? dlg->parentWidget() : dlg,
                           QString("地图「%1」上传成功").arg(mapName));
            emit dlg->uploadSucceeded();
            dlg->accept();
        } else {
            QString msg = resp.value("msg").toString("未知错误");
            QMessageBox::warning(dlg, "上传失败", QString("地图上传失败 (错误码 %1):\n%2").arg(code).arg(msg));
        }
    });
}
