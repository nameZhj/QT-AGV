/**
 * @file    ApiServiceMap.cpp
 * @layer   ProtocolComm
 * @brief   Map lifecycle RESTful APIs including SLAM mapping control, map files upload, and route graph storage.
 * @deps    ApiService.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "ApiService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QUrlQuery>

// ========== AGV 建图与参数控制接口 (端口 8062) ==========

void ApiService::startMapping(const QString &mapName, const Callback &cb)
{
    post("/rbrainrobot/start_mapping", QJsonObject{{"map_name", mapName}}, cb, 8062, false);
}

void ApiService::saveMap(const Callback &cb)
{
    post("/rbrainrobot/save_map", QJsonObject(), cb, 8062, false);
}

void ApiService::startReflectorMapping(const Callback &cb)
{
    post("/rbrainrobot/start_reflector_mapping", QJsonObject(), cb, 8062, false);
}

void ApiService::resetReflectorAll(const Callback &cb)
{
    post("/rbrainrobot/reset_reflector_all", QJsonObject(), cb, 8062, false);
}

void ApiService::resetReflectorLocal(const Callback &cb)
{
    post("/rbrainrobot/reset_reflector_local", QJsonObject(), cb, 8062, false);
}

void ApiService::stopReflectorMapping(const Callback &cb)
{
    post("/rbrainrobot/stop_reflector_mapping", QJsonObject(), cb, 8062, false);
}

void ApiService::cancelMapping(const Callback &cb)
{
    post("/rbrainrobot/cancel_mapping", QJsonObject(), cb, 8062, false);
}

void ApiService::cancelReflectorMapping(const Callback &cb)
{
    post("/rbrainrobot/cancel_reflector_mapping", QJsonObject(), cb, 8062, false);
}

void ApiService::getAllConfigsPreserve(const RawCallback &cb)
{
    getRaw("/rbrainrobot/get_all_configs", cb, 8062, false);
}

void ApiService::getConfigMetadata(const RawCallback &cb)
{
    getRaw("/rbrainrobot/get_config_metadata", cb, 8062, false);
}

void ApiService::modifyParam(const QJsonObject &params, const Callback &cb)
{
    post("/rbrainrobot/modify_all_params", params, cb, 8062, false);
}

void ApiService::getConfigSummary(const FileCallback &cb)
{
    downloadFile("/rbrainrobot/get_config_summary", cb, 8062);
}

void ApiService::updateConfigSummary(const QString &filePath, const Callback &cb)
{
    uploadFile("/rbrainrobot/update_config_summary", filePath, "file", cb, 8062);
}

// ========== 地图基础 CRUD (端口 8048) ==========

void ApiService::listMapAll(const QJsonObject &query, const Callback &cb)
{
    get(appendQuery("/robot/map/listAll", query), cb);
}

void ApiService::listMap(const QJsonObject &query, const Callback &cb)
{
    get(appendQuery("/robot/map/list", query), cb);
}

void ApiService::getMap(const QString &mapId, const Callback &cb)
{
    get("/robot/map/" + mapId, cb);
}

void ApiService::updateMap(const QJsonObject &data, const Callback &cb)
{
    put("/robot/map", data, cb);
}

void ApiService::delMap(const QString &mapId, const Callback &cb)
{
    del("/robot/map/" + mapId, cb);
}

void ApiService::downloadMapResources(const QString &mapId, const FileCallback &cb)
{
    downloadFile("/robot/map/download/resources/" + mapId, cb, 8048);
}

void ApiService::downloadPcd(const QJsonObject &query, const Callback &cb)
{
    get(appendQuery("/common/download/resourcePCDpath", query), cb);
}

// ========== 地图与多部件文件上传 ==========

static QString cleanLocalFilePath(QString p)
{
    if (p.startsWith("file:///")) {
        p = p.mid(8);
    } else if (p.startsWith("file://")) {
        p = p.mid(7);
    }
    return QDir::cleanPath(p.trimmed());
}

static bool appendMultiPartFile(QHttpMultiPart *multiPart, const QString &fieldName, const QString &filePath)
{
    const QString cleanPath = cleanLocalFilePath(filePath);
    auto *file = new QFile(cleanPath);
    if (!file->open(QIODevice::ReadOnly)) {
        delete file;
        return false;
    }
    QHttpPart part;
    const QString fileName = QFileInfo(cleanPath).fileName();
    part.setHeader(QNetworkRequest::ContentDispositionHeader,
                   QVariant(QStringLiteral("form-data; name=\"%1\"; filename=\"%2\"").arg(fieldName, fileName)));
    part.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/octet-stream"));
    part.setBodyDevice(file);
    file->setParent(multiPart);
    multiPart->append(part);
    return true;
}

static void appendMultiPartText(QHttpMultiPart *multiPart, const QString &fieldName, const QString &text)
{
    QHttpPart part;
    part.setHeader(QNetworkRequest::ContentDispositionHeader,
                   QVariant(QStringLiteral("form-data; name=\"%1\"").arg(fieldName)));
    part.setBody(text.toUtf8());
    multiPart->append(part);
}

void ApiService::addAgvMap(const QJsonObject &data, const Callback &cb)
{
    post("/robot/indoor/map/web/add", data, cb);
}

void ApiService::uploadIndoorMap(const IndoorMapUploadParams &params, const Callback &cb, int port)
{
    auto *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    appendMultiPartText(multiPart, "mapName", params.mapName);

    if (!appendMultiPartFile(multiPart, "pgmFile", params.pgmFilePath)) {
        delete multiPart;
        cb(500, QJsonObject{{"msg", "无法读取PGM/图片文件: " + params.pgmFilePath}});
        return;
    }
    if (!appendMultiPartFile(multiPart, "yamlFile", params.yamlFilePath)) {
        delete multiPart;
        cb(500, QJsonObject{{"msg", "无法读取YAML文件: " + params.yamlFilePath}});
        return;
    }
    if (!appendMultiPartFile(multiPart, "pbStreamFile", params.pbStreamFilePath)) {
        delete multiPart;
        cb(500, QJsonObject{{"msg", "无法读取PBStream文件: " + params.pbStreamFilePath}});
        return;
    }
    if (!params.reflectorBinFilePath.trimmed().isEmpty()) {
        const QString binPath = cleanLocalFilePath(params.reflectorBinFilePath);
        if (QFile::exists(binPath)) {
            appendMultiPartFile(multiPart, "reflectorBinFile", binPath);
        }
    }
    if (!params.reflectorJsonFilePath.trimmed().isEmpty()) {
        const QString jsonPath = cleanLocalFilePath(params.reflectorJsonFilePath);
        if (QFile::exists(jsonPath)) {
            appendMultiPartFile(multiPart, "reflectorJsonFile", jsonPath);
        }
    }

    QNetworkRequest req = buildRequest(buildUrl("/robot/indoor/map/web/add", port), true, kUploadTimeoutMs);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QVariant());
    QNetworkReply *reply = m_manager->post(req, multiPart);
    multiPart->setParent(reply);
    handleReply(reply, cb);
}

void ApiService::uploadSmap(const QString &mapName, const QString &filePath, const Callback &cb, int port)
{
    auto *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    appendMultiPartText(multiPart, "mapName", mapName);

    if (!appendMultiPartFile(multiPart, "file", filePath)) {
        delete multiPart;
        cb(500, QJsonObject{{"msg", "无法读取smap文件: " + filePath}});
        return;
    }

    QNetworkRequest req = buildRequest(buildUrl("/robot/map/smap/upload", port), true, kUploadTimeoutMs);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QVariant());
    QNetworkReply *reply = m_manager->post(req, multiPart);
    multiPart->setParent(reply);
    handleReply(reply, cb);
}

// ========== 地图编辑与反光板特征 ==========

void ApiService::saveEditedMap(const QJsonObject &data, const Callback &cb)
{
    post("/robot/map/route/save", data, cb);
}

void ApiService::loadEditedMap(const Callback &cb)
{
    get("/robot/map/route/load", cb, 8048);
}

void ApiService::loadEditedMap(const QJsonObject &/*data*/, const Callback &cb)
{
    loadEditedMap(cb);
}

void ApiService::getReflectorColumnInfo(const QString &mapId, const Callback &cb)
{
    QUrlQuery q;
    q.addQueryItem("mapId", mapId);
    get("/robot/indoor/map/reflectorColumnInfo/get?" + q.toString(), cb);
}
