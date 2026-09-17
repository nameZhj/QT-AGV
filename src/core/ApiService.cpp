#include "ApiService.h"
#include "AppConfig.h"

#include <QJsonDocument>
#include <QNetworkReply>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QUrlQuery>
#include <QDateTime>
#include <QMap>

ApiService &ApiService::instance()
{
    static ApiService inst;
    return inst;
}

ApiService::ApiService(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
{
}

ApiService::~ApiService() = default;

QString ApiService::buildUrl(const QString &path, int port) const
{
    const QString normalizedPath = path.startsWith('/') ? path : ("/" + path);
    return AppConfig::instance().robotApiUrl(port) + normalizedPath;
}

QNetworkRequest ApiService::buildRequest(const QString &url, bool withToken, int timeoutMs) const
{
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json;charset=utf-8");

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    req.setTransferTimeout(timeoutMs);
#endif

    if (withToken) {
        const QString token = AppConfig::instance().token().trimmed();
        if (!token.isEmpty()) {
            req.setRawHeader("Authorization", ("Bearer " + token).toUtf8());
        }
    }
    return req;
}

QString ApiService::appendQuery(const QString &basePath, const QJsonObject &query)
{
    if (query.isEmpty()) {
        return basePath;
    }
    QUrlQuery q;
    for (auto it = query.begin(); it != query.end(); ++it) {
        const QString valStr = it.value().isString() ? it.value().toString() : it.value().toVariant().toString();
        q.addQueryItem(it.key(), valStr);
    }
    const QString qs = q.toString(QUrl::FullyEncoded);
    if (qs.isEmpty()) {
        return basePath;
    }
    return basePath + (basePath.contains('?') ? '&' : '?') + qs;
}

// 抑制短时间内相同接口、相同错误码重复输出控制台刷屏日志
static bool shouldLogHttpError(const QString &urlStr, int errorCode)
{
    static QMap<QString, qint64> s_lastErrorTimes;
    static QMap<QString, int>    s_lastErrorCodes;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    const QUrl u(urlStr);
    const QString endpointKey = u.path();

    if (s_lastErrorCodes.value(endpointKey, -1) == errorCode) {
        const qint64 lastTime = s_lastErrorTimes.value(endpointKey, 0);
        if (now - lastTime < 3000) {
            return false;
        }
    }
    s_lastErrorTimes[endpointKey] = now;
    s_lastErrorCodes[endpointKey] = errorCode;
    return true;
}

void ApiService::handleReply(QNetworkReply *reply, const Callback &cb)
{
    connect(reply, &QNetworkReply::finished, this, [reply, cb]() {
        reply->deleteLater();
        const QString urlStr = reply->url().toString();

        if (reply->error() != QNetworkReply::NoError) {
            const int errCode = static_cast<int>(reply->error());
            QString errStr = QStringLiteral("Network error %1").arg(errCode);
            if (reply->error() == QNetworkReply::TimeoutError) {
                errStr = QStringLiteral("Network timeout");
            }
            if (shouldLogHttpError(urlStr, errCode)) {
                qWarning().noquote() << QStringLiteral("[HTTP ERROR] %1 -> %2").arg(urlStr, errStr);
            }
            cb(500, QJsonObject{{"msg", errStr}});
            return;
        }

        const QByteArray data = reply->readAll();
        qDebug().noquote() << QStringLiteral("[HTTP IN] %1 -> %2").arg(urlStr, QString::fromUtf8(data).left(500));

        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if (err.error != QJsonParseError::NoError) {
            cb(500, QJsonObject{{"msg", "JSON parse error"}});
            return;
        }

        const QJsonObject obj = doc.object();
        const int code = obj.value("code").toInt(200);
        cb(code, obj);
    });
}

void ApiService::handleRawReply(QNetworkReply *reply, const RawCallback &cb)
{
    connect(reply, &QNetworkReply::finished, this, [reply, cb]() {
        reply->deleteLater();
        const QString urlStr = reply->url().toString();

        if (reply->error() != QNetworkReply::NoError) {
            const int errCode = static_cast<int>(reply->error());
            const QString errStr = QStringLiteral("Network error %1").arg(errCode);
            if (shouldLogHttpError(urlStr, errCode)) {
                qWarning().noquote() << QStringLiteral("[HTTP RAW ERROR] %1 -> %2").arg(urlStr, errStr);
            }
            cb(500, QByteArray());
            return;
        }

        const QByteArray data = reply->readAll();
        qDebug().noquote() << QStringLiteral("[HTTP RAW IN] %1 -> (bytes: %2)").arg(urlStr).arg(data.size());
        cb(200, data);
    });
}

void ApiService::handleFileReply(QNetworkReply *reply, const FileCallback &cb)
{
    connect(reply, &QNetworkReply::finished, this, [reply, cb]() {
        reply->deleteLater();
        const QString urlStr = reply->url().toString();

        if (reply->error() != QNetworkReply::NoError) {
            const int errCode = static_cast<int>(reply->error());
            const QString errStr = QStringLiteral("Network error %1").arg(errCode);
            if (shouldLogHttpError(urlStr, errCode)) {
                qWarning().noquote() << QStringLiteral("[HTTP FILE ERROR] %1 -> %2").arg(urlStr, errStr);
            }
            cb(false, QByteArray());
            return;
        }

        const QByteArray data = reply->readAll();
        qDebug().noquote() << QStringLiteral("[HTTP FILE IN] %1 -> (size: %2 bytes)").arg(urlStr).arg(data.size());
        cb(true, data);
    });
}

// ========== 通用基础请求 ==========

void ApiService::get(const QString &url, const Callback &cb, int port, bool withToken)
{
    QString finalUrl = url;
    const QString timestampParam = "t=" + QString::number(QDateTime::currentMSecsSinceEpoch());
    finalUrl += (finalUrl.contains('?') ? '&' : '?') + timestampParam;

    const QString requestUrl = buildUrl(finalUrl, port);
    qDebug().noquote() << QStringLiteral("[HTTP GET] %1").arg(requestUrl);

    const QNetworkRequest req = buildRequest(requestUrl, withToken, kDefaultTimeoutMs);
    QNetworkReply *reply = m_manager->get(req);
    handleReply(reply, cb);
}

void ApiService::post(const QString &url, const QJsonObject &data, const Callback &cb, int port, bool withToken)
{
    const QString requestUrl = buildUrl(url, port);
    const QByteArray body = QJsonDocument(data).toJson(QJsonDocument::Compact);
    qDebug().noquote() << QStringLiteral("[HTTP POST] %1 -> %2").arg(requestUrl, QString::fromUtf8(body).left(300));

    const QNetworkRequest req = buildRequest(requestUrl, withToken, kDefaultTimeoutMs);
    QNetworkReply *reply = m_manager->post(req, body);
    handleReply(reply, cb);
}

void ApiService::put(const QString &url, const QJsonObject &data, const Callback &cb, int port, bool withToken)
{
    const QString requestUrl = buildUrl(url, port);
    const QByteArray body = QJsonDocument(data).toJson(QJsonDocument::Compact);
    qDebug().noquote() << QStringLiteral("[HTTP PUT] %1 -> %2").arg(requestUrl, QString::fromUtf8(body).left(300));

    const QNetworkRequest req = buildRequest(requestUrl, withToken, kDefaultTimeoutMs);
    QNetworkReply *reply = m_manager->put(req, body);
    handleReply(reply, cb);
}

void ApiService::del(const QString &url, const Callback &cb, int port, bool withToken)
{
    const QString requestUrl = buildUrl(url, port);
    qDebug().noquote() << QStringLiteral("[HTTP DELETE] %1").arg(requestUrl);

    const QNetworkRequest req = buildRequest(requestUrl, withToken, kDefaultTimeoutMs);
    QNetworkReply *reply = m_manager->deleteResource(req);
    handleReply(reply, cb);
}

void ApiService::uploadFile(const QString &url, const QString &filePath, const QString &fieldName,
                            const Callback &cb, int port)
{
    auto *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    auto *file = new QFile(filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        delete file;
        delete multiPart;
        cb(500, QJsonObject{{"msg", "Cannot open file"}});
        return;
    }

    QHttpPart part;
    part.setHeader(QNetworkRequest::ContentDispositionHeader,
                   QVariant("form-data; name=\"" + fieldName + "\"; filename=\"" + QFileInfo(filePath).fileName() + "\""));
    part.setBodyDevice(file);
    file->setParent(multiPart);
    multiPart->append(part);

    QNetworkRequest req = buildRequest(buildUrl(url, port), true, kUploadTimeoutMs);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QVariant()); // 由 QNetworkAccessManager 自动设置 boundary
    QNetworkReply *reply = m_manager->post(req, multiPart);
    multiPart->setParent(reply);
    handleReply(reply, cb);
}

void ApiService::uploadMapPgm(const QString &mapId, const QByteArray &pgmData, const Callback &cb)
{
    auto *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
        QVariant(QStringLiteral("form-data; name=\"pgmFile\"; filename=\"map_%1.pgm\"").arg(mapId)));
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/octet-stream"));
    filePart.setBody(pgmData);
    multiPart->append(filePart);

    QHttpPart mapIdPart;
    mapIdPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"mapId\""));
    mapIdPart.setBody(mapId.toUtf8());
    multiPart->append(mapIdPart);

    QNetworkRequest req = buildRequest(buildUrl("/robot/indoor/map/pgm/update", 8048), true, kUploadTimeoutMs);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QVariant());
    QNetworkReply *reply = m_manager->post(req, multiPart);
    multiPart->setParent(reply);
    handleReply(reply, cb);
}

void ApiService::downloadFile(const QString &url, const FileCallback &cb, int port, bool withToken)
{
    const QString fullUrl = (url.startsWith("http://", Qt::CaseInsensitive) || url.startsWith("https://", Qt::CaseInsensitive))
                          ? url : buildUrl(url, port);
    QNetworkRequest req{QUrl(fullUrl)};
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    req.setTransferTimeout(kUploadTimeoutMs);
#endif
    if (withToken) {
        const QString token = AppConfig::instance().token().trimmed();
        if (!token.isEmpty()) {
            req.setRawHeader("Authorization", ("Bearer " + token).toUtf8());
        }
    }
    QNetworkReply *reply = m_manager->get(req);
    handleFileReply(reply, cb);
}

void ApiService::getRaw(const QString &url, const RawCallback &cb, int port, bool withToken)
{
    const QNetworkRequest req = buildRequest(buildUrl(url, port), withToken, kDefaultTimeoutMs);
    QNetworkReply *reply = m_manager->get(req);
    handleRawReply(reply, cb);
}

// ========== AGV 控制接口 ==========

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

void ApiService::suspendWork(const Callback &cb)
{
    post("/rbrainrobot/suspend", QJsonObject(), cb, 8061, false);
}

void ApiService::continueWork(const Callback &cb)
{
    post("/rbrainrobot/continue", QJsonObject(), cb, 8061, false);
}

void ApiService::startCheck(const Callback &cb)
{
    get("/rbrainrobot/self_check", cb, 8062, false);
}

void ApiService::startTerminal(const Callback &cb)
{
    post("/rbrainrobot/start_terminal", QJsonObject(), cb, 8062, false);
}

void ApiService::stopTerminal(const Callback &cb)
{
    post("/rbrainrobot/stop_terminal", QJsonObject(), cb, 8062, false);
}

void ApiService::getAllConfigsPreserve(const RawCallback &cb)
{
    getRaw("/rbrainrobot/get_all_configs", cb, 8062, false);
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

// ========== 设备接口 ==========

void ApiService::listCarAll(const QJsonObject &query, const Callback &cb)
{
    get(appendQuery("/robot/car/listAll", query), cb);
}

void ApiService::listCar(const QJsonObject &query, const Callback &cb)
{
    get(appendQuery("/robot/car/list", query), cb);
}

void ApiService::getCar(const QString &carId, const Callback &cb)
{
    get("/robot/car/" + carId, cb);
}

void ApiService::addCar(const QJsonObject &data, const Callback &cb)
{
    post("/robot/car", data, cb);
}

void ApiService::linkCar(const QJsonObject &data, const Callback &cb)
{
    put("/robot/car/link", data, cb);
}

void ApiService::updateCar(const QJsonObject &data, const Callback &cb)
{
    put("/robot/car", data, cb);
}

void ApiService::changeCarStatus(const QString &carId, const QString &mapId, const QString &status, const Callback &cb)
{
    put("/robot/car/changeStatus", QJsonObject{{"carId", carId}, {"mapId", mapId}, {"status", status}}, cb);
}

void ApiService::delCar(const QString &carId, const Callback &cb)
{
    del("/robot/car/" + carId, cb);
}

void ApiService::checkCertification(const QString &ckey, const Callback &cb)
{
    get("/robot/car/checkC/" + ckey, cb);
}

void ApiService::sendMqtt(const QJsonObject &data, const Callback &cb)
{
    post("/robot/mqtt/pub", data, cb);
}

void ApiService::editCarMap(const QString &mapId, const Callback &cb)
{
    const QString url = "/robot/car/edit/map/" + mapId;
    put(url, QJsonObject{}, [this, url, cb](int code, const QJsonObject &data) {
        if (code == 200) {
            cb(code, data);
        } else {
            get(url, [this, url, cb](int code2, const QJsonObject &data2) {
                if (code2 == 200) {
                    cb(code2, data2);
                } else {
                    post(url, QJsonObject{}, cb);
                }
            });
        }
    });
}

// ========== 地图接口 ==========

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

// ========== PCD / 地图文件接口 ==========

void ApiService::downloadPcd(const QJsonObject &query, const Callback &cb)
{
    get(appendQuery("/common/download/resourcePCDpath", query), cb);
}

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

// ========== 地图编辑接口 ==========

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

// ========== 任务接口 ==========

void ApiService::listTask(const QJsonObject &query, const Callback &cb)
{
    get(appendQuery("/robot/task/list", query), cb);
}

void ApiService::getTask(const QString &taskId, const Callback &cb)
{
    get("/robot/task/" + taskId, cb);
}

void ApiService::getTaskInfo(const QString &taskId, const Callback &cb)
{
    get("/robot/task/taskInfo/" + taskId, cb);
}

void ApiService::addTask(const QJsonObject &data, const Callback &cb)
{
    post("/robot/task", data, cb);
}

void ApiService::updateTask(const QJsonObject &data, const Callback &cb)
{
    put("/robot/task", data, cb);
}

void ApiService::delTask(const QString &taskId, const Callback &cb)
{
    del("/robot/task/" + taskId, cb);
}

void ApiService::listTaskAllStatus(const QString &carId, const Callback &cb)
{
    get("/robot/navigation/listAllStatus/" + carId, cb);
}

void ApiService::getCurrentTask(const QJsonObject &query, const Callback &cb)
{
    get(appendQuery("/robot/navigation/currentTask", query), cb);
}

void ApiService::loadTask(const QJsonObject &data, const Callback &cb)
{
    put("/robot/navigation/loadTaskQueue", data, cb);
}

void ApiService::startTask(const QJsonObject &data, const Callback &cb)
{
    post("/robot/navigation/start", data, cb);
}

void ApiService::removeTask(const QJsonObject &data, const Callback &cb)
{
    put("/robot/navigation/quitTaskQueue", data, cb);
}

void ApiService::commandTask(const QJsonObject &data, const Callback &cb)
{
    post("/robot/navigation/command", data, cb);
}

// ========== 任务日志接口 ==========

void ApiService::listTasklog(const QJsonObject &query, const Callback &cb)
{
    get(appendQuery("/robot/tasklog/list", query), cb);
}

void ApiService::getTasklog(const QString &tasklogId, const Callback &cb)
{
    get("/robot/tasklog/" + tasklogId, cb);
}

// ========== 登录认证接口 ==========

void ApiService::login(const QString &username, const QString &password, const Callback &cb)
{
    post("/login", QJsonObject{{"username", username}, {"password", password}}, cb, 8048, false);
}

// ========== 导航 / 位姿接口 ==========

void ApiService::initPose(const QString &carId, const QJsonObject &pose, const Callback &cb)
{
    QJsonObject data = pose;
    data["carId"] = carId;
    post("/robot/navigation/initPose", data, cb);
}

void ApiService::navigateTo(const QString &locationId, const Callback &cb)
{
    QUrlQuery q;
    q.addQueryItem("locationId", locationId);
    post("/robot/navigation/navigate/to?" + q.toString(), QJsonObject(), cb);
}

// ========== 便捷重载 ==========

void ApiService::listCarAll(const Callback &cb)
{
    listCarAll(QJsonObject(), cb);
}

void ApiService::startTask(const QString &taskId, const Callback &cb)
{
    startTask(QJsonObject{{"taskId", taskId}}, cb);
}
