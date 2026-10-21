/**
 * @file    EditConfigDataLoader.cpp
 * @layer   GUILayer
 * @brief   Implementation of EditConfigDataLoader remote API requests and persistence.
 * @deps    EditConfigDataLoader.h, EditConfigWidget.h, EditConfigRouteSync.h, ConfigMetadataHelper.h, ApiService.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "EditConfigDataLoader.h"
#include "EditConfigWidget.h"
#include "EditConfigRouteSync.h"
#include "ConfigMetadataHelper.h"
#include "network/ApiService.h"
#include "core/MotionConfigManager.h"
#include "ui/common/LoadingDialog.h"
#include "ui/common/Toast.h"
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileDialog>
#include <QFile>
#include <QMessageBox>
#include <QTableWidget>
#include <QToolButton>

void EditConfigDataLoader::loadAll(EditConfigWidget *w)
{
    if (!w) return;
    QSettings settings;
    QByteArray cachedMeta = settings.value("editConfig/cachedMetadata").toByteArray();
    if (!cachedMeta.isEmpty() && w->m_metadataList.isEmpty()) {
        parseMetadata(w, cachedMeta);
    }

    ApiService::instance().getConfigMetadata([w](int mCode, const QByteArray &mRaw) {
        if (!w) return;
        if (mCode == 200) {
            parseMetadata(w, mRaw);
            w->updateDefaultValuesInTable();
        }
    });

    ApiService::instance().getAllConfigsPreserve([w](int code, const QByteArray &rawData) {
        if (!w) return;
        QJsonArray list;
        bool loadedFromNetwork = false;

        if (code == 200) {
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(rawData, &err);
            if (err.error == QJsonParseError::NoError) {
                QJsonObject root = doc.object();
                if (root.contains("data")) {
                    if (root.value("data").isArray()) {
                        list = root.value("data").toArray();
                    } else if (root.value("data").isObject()) {
                        QJsonObject dataObj = root.value("data").toObject();
                        if (dataObj.contains("data") && dataObj.value("data").isArray()) {
                            list = dataObj.value("data").toArray();
                        } else {
                            for (const auto &k : dataObj.keys()) {
                                if (dataObj.value(k).isArray()) {
                                    list = dataObj.value(k).toArray();
                                    break;
                                }
                            }
                        }
                    }
                }
                loadedFromNetwork = true;
            }
        }

        if (!loadedFromNetwork || list.isEmpty()) {
            QSettings settings;
            QByteArray cached = settings.value("editConfig/cachedConfigs").toByteArray();
            if (!cached.isEmpty()) {
                QJsonDocument cDoc = QJsonDocument::fromJson(cached);
                if (cDoc.isArray()) {
                    list = cDoc.array();
                }
            }
        }

        ConfigMetadataHelper::ensureMotionControlConfig(list);
        ConfigMetadataHelper::ensureAlignConfig(list);
        ConfigMetadataHelper::ensureObstacleConfig(list);

        MotionConfigManager::instance().updateFromConfigsArray(list);

        w->m_configs = list;
        w->refreshGroups();
    });
}

void EditConfigDataLoader::parseMetadata(EditConfigWidget *w, const QByteArray &rawJson)
{
    if (!w) return;
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(rawJson, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return;
    }
    QJsonObject root = doc.object();
    QJsonArray list;
    if (root.contains("data")) {
        QJsonValue dv = root.value("data");
        if (dv.isArray()) {
            list = dv.toArray();
        } else if (dv.isObject()) {
            QJsonObject dataObj = dv.toObject();
            if (dataObj.contains("data") && dataObj.value("data").isArray()) {
                list = dataObj.value("data").toArray();
            } else {
                for (const auto &k : dataObj.keys()) {
                    if (dataObj.value(k).isArray()) {
                        list = dataObj.value(k).toArray();
                        break;
                    }
                }
            }
        }
    }
    if (!list.isEmpty()) {
        w->m_metadataList = list;
        QSettings settings;
        settings.setValue("editConfig/cachedMetadata", rawJson);
        MotionConfigManager::instance().updateFromMetadataArray(list);
    }
}

void EditConfigDataLoader::onSave(EditConfigWidget *w)
{
    if (!w) return;
    if (QWidget *fw = w->focusWidget()) {
        fw->clearFocus();
    }
    if (w->m_paramTable) {
        w->m_paramTable->clearFocus();
        w->m_paramTable->setCurrentIndex(QModelIndex());

        for (int r = 0; r < w->m_paramTable->rowCount(); ++r) {
            w->onParamCellChanged(r, 1);
        }
    }

    MotionConfigManager::instance().updateFromConfigsArray(w->m_configs);

    QJsonObject payload;
    for (const auto &item : w->m_configs) {
        QJsonObject entry = item.toObject();
        QString key = entry.value("file_path").toString(entry.value("name").toString());
        QJsonObject val;
        val["name"] = entry.value("name");
        val["status"] = entry.value("status");
        val["params"] = entry.value("params");
        val["file_path"] = entry.value("file_path");
        payload[key] = val;
    }

    QSettings settings;
    settings.setValue("editConfig/cachedConfigs", QJsonDocument(w->m_configs).toJson(QJsonDocument::Compact));

    const auto &mc = MotionConfigManager::instance();
    EditConfigRouteSync::syncObstacleParamsToRouteNetwork(w, mc.defaultObsStopLength(), mc.defaultObsStopWidth(),
                                                         mc.defaultObsDecelLength(), mc.defaultObsDecelWidth());

    if (w->m_saveBtn) w->m_saveBtn->setEnabled(false);
    auto *loading = new LoadingDialog("正在保存配置参数并同步路网...", w);
    loading->show();
    ApiService::instance().modifyParam(payload, [w, loading](int code, const QJsonObject &) {
        loading->close();
        loading->deleteLater();
        if (w && w->m_saveBtn) w->m_saveBtn->setEnabled(true);
        if (code == 200) {
            Toast::success(w, "配置参数与路网避障参数已保存");
        } else {
            QMessageBox::warning(w, "错误", "保存配置参数失败 (已在本地缓存最新配置)");
        }
    });
}

void EditConfigDataLoader::onDownload(EditConfigWidget *w)
{
    if (!w) return;
    if (w->m_downloadBtn) w->m_downloadBtn->setEnabled(false);
    auto *loading = new LoadingDialog("正在下载配置文件...", w);
    loading->show();
    ApiService::instance().getConfigSummary([w, loading](bool ok, const QByteArray &data) {
        loading->close();
        loading->deleteLater();
        if (w && w->m_downloadBtn) w->m_downloadBtn->setEnabled(true);
        if (!ok) {
            QMessageBox::warning(w, "错误", "下载失败");
            return;
        }
        QString path = QFileDialog::getSaveFileName(w, "保存配置文件", "config_summary.json", "JSON Files (*.json)");
        if (path.isEmpty()) return;
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(data);
            file.close();
            Toast::success(w, "下载成功");
        }
    });
}

void EditConfigDataLoader::onUpload(EditConfigWidget *w)
{
    if (!w) return;
    QString path = QFileDialog::getOpenFileName(w, "选择配置文件", "", "JSON Files (*.json)");
    if (path.isEmpty()) return;

    if (w->m_uploadBtn) w->m_uploadBtn->setEnabled(false);
    auto *loading = new LoadingDialog("正在上传配置文件...", w);
    loading->show();
    ApiService::instance().updateConfigSummary(path, [w, loading](int code, const QJsonObject &) {
        loading->close();
        loading->deleteLater();
        if (w && w->m_uploadBtn) w->m_uploadBtn->setEnabled(true);
        if (code == 200) {
            Toast::success(w, "上传成功");
            loadAll(w);
        } else {
            QMessageBox::warning(w, "错误", "上传失败");
        }
    });
}
