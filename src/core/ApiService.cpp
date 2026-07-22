#include "ApiService.h"
#include "AppConfig.h"
#include <QJsonDocument>
#include <QNetworkReply>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QUrlQuery>

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

ApiService::~ApiService() {}

QString ApiService::buildUrl(const QString &path, int port) const
{
    // 使用端口映射: 内部端口自动转换为外部映射端口
    int mappedPort = AppConfig::mapPort(port);
    return "http://" + AppConfig::instance().serverIp() + ":" + QString::number(mappedPort) + path;
}

QNetworkRequest ApiService::buildRequest(const QString &url, bool withToken) const
{
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json;charset=utf-8");
    if (withToken) {
        QString tok = AppConfig::instance().token();
        if (!tok.isEmpty()) {
            req.setRawHeader("Authorization", ("Bearer " + tok).toUtf8());
        }
    }
    return req;
}

void ApiService::handleReply(QNetworkReply *reply, const Callback &cb)
{
    connect(reply, &QNetworkReply::finished, this, [reply, cb]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "API error:" << reply->errorString();
            cb(500, QJsonObject{{"msg", reply->errorString()}});
            return;
        }
        QByteArray data = reply->readAll();
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if (err.error != QJsonParseError::NoError) {
            cb(500, QJsonObject{{"msg", "JSON parse error"}});
            return;
        }
        QJsonObject obj = doc.object();
        int code = obj.value("code").toInt(200);
        cb(code, obj);
    });
}

void ApiService::handleRawReply(QNetworkReply *reply, const RawCallback &cb)
{
    connect(reply, &QNetworkReply::finished, this, [reply, cb]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            cb(500, QByteArray());
            return;
        }
        QByteArray data = reply->readAll();
        cb(200, data);
    });
}

void ApiService::handleFileReply(QNetworkReply *reply, const FileCallback &cb)
{
    connect(reply, &QNetworkReply::finished, this, [reply, cb]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            cb(false, QByteArray());
            return;
        }
        QByteArray data = reply->readAll();
        cb(true, data);
    });
}

// ========== 通用请求 ==========
void ApiService::get(const QString &url, const Callback &cb, int port, bool withToken)
{
    QString finalUrl = url;
    if (finalUrl.contains("?")) {
        finalUrl += "&t=" + QString::number(QDateTime::currentMSecsSinceEpoch());
    } else {
        finalUrl += "?t=" + QString::number(QDateTime::currentMSecsSinceEpoch());
    }
    QNetworkRequest req = buildRequest(buildUrl(finalUrl, port), withToken);
    QNetworkReply *reply = m_manager->get(req);
    handleReply(reply, cb);
}

void ApiService::post(const QString &url, const QJsonObject &data, const Callback &cb, int port, bool withToken)
{
    QNetworkRequest req = buildRequest(buildUrl(url, port), withToken);
    QNetworkReply *reply = m_manager->post(req, QJsonDocument(data).toJson());
    handleReply(reply, cb);
}

void ApiService::put(const QString &url, const QJsonObject &data, const Callback &cb, int port, bool withToken)
{
    QNetworkRequest req = buildRequest(buildUrl(url, port), withToken);
    QNetworkReply *reply = m_manager->put(req, QJsonDocument(data).toJson());
    handleReply(reply, cb);
}

void ApiService::del(const QString &url, const Callback &cb, int port, bool withToken)
{
    QNetworkRequest req = buildRequest(buildUrl(url, port), withToken);
    QNetworkReply *reply = m_manager->deleteResource(req);
    handleReply(reply, cb);
}

void ApiService::uploadFile(const QString &url, const QString &filePath, const QString &fieldName,
                             const Callback &cb, int port)
{
    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    QFile *file = new QFile(filePath);
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

    QNetworkRequest req = buildRequest(buildUrl(url, port));
    QNetworkReply *reply = m_manager->post(req, multiPart);
    multiPart->setParent(reply);
    handleReply(reply, cb);
}

void ApiService::uploadMapPgm(const QString &mapId, const QByteArray &pgmData, const Callback &cb)
{
    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
        QVariant(QString("form-data; name=\"pgmFile\"; filename=\"map_%1.pgm\"").arg(mapId)));
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/octet-stream"));
    filePart.setBody(pgmData);
    multiPart->append(filePart);

    QHttpPart mapIdPart;
    mapIdPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"mapId\""));
    mapIdPart.setBody(mapId.toUtf8());
    multiPart->append(mapIdPart);

    QNetworkRequest req = buildRequest(buildUrl("/robot/indoor/map/pgm/update", 8048));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QVariant()); // let QNetworkAccessManager set multipart boundary
    QNetworkReply *reply = m_manager->post(req, multiPart);
    multiPart->setParent(reply);
    handleReply(reply, cb);
}

void ApiService::downloadFile(const QString &url, const FileCallback &cb, int port, bool withToken)
{
    // Build a plain request — no Content-Type for binary file downloads
    QString fullUrl = (url.startsWith("http://") || url.startsWith("https://"))
                        ? url : buildUrl(url, port);
    QNetworkRequest req(fullUrl);
    if (withToken) {
        QString tok = AppConfig::instance().token();
        if (!tok.isEmpty())
            req.setRawHeader("Authorization", ("Bearer " + tok).toUtf8());
    }
    QNetworkReply *reply = m_manager->get(req);
    handleFileReply(reply, cb);
}

void ApiService::getRaw(const QString &url, const RawCallback &cb, int port, bool withToken)
{
    QNetworkRequest req = buildRequest(buildUrl(url, port), withToken);
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
    QString url = "/robot/car/listAll";
    if (!query.isEmpty()) {
        QUrlQuery q;
        for (auto it = query.begin(); it != query.end(); ++it) {
            QString valStr = it.value().isString() ? it.value().toString() : it.value().toVariant().toString();
            q.addQueryItem(it.key(), valStr);
        }
        url += "?" + q.toString();
    }
    get(url, cb);
}

void ApiService::listCar(const QJsonObject &query, const Callback &cb)
{
    QString url = "/robot/car/list";
    if (!query.isEmpty()) {
        QUrlQuery q;
        for (auto it = query.begin(); it != query.end(); ++it) {
            QString valStr = it.value().isString() ? it.value().toString() : it.value().toVariant().toString();
            q.addQueryItem(it.key(), valStr);
        }
        url += "?" + q.toString();
    }
    get(url, cb);
}

void ApiService::getCar(const QString &carId, const Callback &cb)
{
    get("/robot/car/" + carId, cb);
}

void ApiService::addCar(const QJsonObject &data, const Callback &cb)
{
    post("/robot/car", data, cb);
}

void ApiService::addCarWithoutKey(const QJsonObject &data, const Callback &cb)
{
    // 对应 Vue addCarWithoutKey: POST /robot/car/withoutCertificationKey
    // 无需设备认证码即可创建设备，需传入 carName 等基本信息
    post("/robot/car/withoutCertificationKey", data, cb);
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

// ========== 地图接口 ==========
void ApiService::listMapAll(const QJsonObject &query, const Callback &cb)
{
    QString url = "/robot/map/listAll";
    if (!query.isEmpty()) {
        QUrlQuery q;
        for (auto it = query.begin(); it != query.end(); ++it) {
            QString valStr = it.value().isString() ? it.value().toString() : it.value().toVariant().toString();
            q.addQueryItem(it.key(), valStr);
        }
        url += "?" + q.toString();
    }
    get(url, cb);
}

void ApiService::listMap(const QJsonObject &query, const Callback &cb)
{
    QString url = "/robot/map/list";
    if (!query.isEmpty()) {
        QUrlQuery q;
        for (auto it = query.begin(); it != query.end(); ++it) {
            QString valStr = it.value().isString() ? it.value().toString() : it.value().toVariant().toString();
            q.addQueryItem(it.key(), valStr);
        }
        url += "?" + q.toString();
    }
    get(url, cb);
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

// ========== PCD / 地图文件接口 ==========
void ApiService::downloadPcd(const QJsonObject &query, const Callback &cb)
{
    QString url = "/common/download/resourcePCDpath";
    if (!query.isEmpty()) {
        QUrlQuery q;
        for (auto it = query.begin(); it != query.end(); ++it) {
            QString valStr = it.value().isString() ? it.value().toString() : it.value().toVariant().toString();
            q.addQueryItem(it.key(), valStr);
        }
        url += "?" + q.toString();
    }
    get(url, cb);
}

void ApiService::addAgvMap(const QJsonObject &data, const Callback &cb)
{
    post("/robot/indoor/map/web/add", data, cb);
}

void ApiService::sendMap2Car(const QJsonObject &params, const Callback &cb)
{
    QString url = "/robot/indoor/map/agv/manual/upload";
    if (!params.isEmpty()) {
        QUrlQuery q;
        for (auto it = params.begin(); it != params.end(); ++it)
            q.addQueryItem(it.key(), it.value().toString());
        url += "?" + q.toString();
    }
    post(url, QJsonObject(), cb);
}

// ========== 地图编辑接口 ==========
void ApiService::saveEditedMap(const QJsonObject &data, const Callback &cb)
{
    post("/robot/map/route/save", data, cb);
}

void ApiService::loadEditedMap(const QJsonObject &data, const Callback &cb)
{
    post("/robot/map/route/load", data, cb);
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
    QString url = "/robot/task/list";
    if (!query.isEmpty()) {
        QUrlQuery q;
        for (auto it = query.begin(); it != query.end(); ++it) {
            QString valStr = it.value().isString() ? it.value().toString() : it.value().toVariant().toString();
            q.addQueryItem(it.key(), valStr);
        }
        url += "?" + q.toString();
    }
    get(url, cb);
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
    QString url = "/robot/navigation/currentTask";
    if (!query.isEmpty()) {
        QUrlQuery q;
        for (auto it = query.begin(); it != query.end(); ++it) {
            QString valStr = it.value().isString() ? it.value().toString() : it.value().toVariant().toString();
            q.addQueryItem(it.key(), valStr);
        }
        url += "?" + q.toString();
    }
    get(url, cb);
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
    QString url = "/robot/tasklog/list";
    if (!query.isEmpty()) {
        QUrlQuery q;
        for (auto it = query.begin(); it != query.end(); ++it) {
            QString valStr = it.value().isString() ? it.value().toString() : it.value().toVariant().toString();
            q.addQueryItem(it.key(), valStr);
        }
        url += "?" + q.toString();
    }
    get(url, cb);
}

void ApiService::getTasklog(const QString &tasklogId, const Callback &cb)
{
    get("/robot/tasklog/" + tasklogId, cb);
}

// ========== 登录接口 ==========
void ApiService::login(const QString &username, const QString &password, const Callback &cb)
{
    post("/login", QJsonObject{{"username", username}, {"password", password}}, cb, 8048, false);
}

void ApiService::listCarAllNoLogin(const Callback &cb)
{
    get("/robot/car/listAll/noLogin", cb, 8048, false);
}

// ========== 导航/位姿接口 ==========
void ApiService::initPose(const QString &carId, const QJsonObject &pose, const Callback &cb)
{
    QJsonObject data = pose;
    data["carId"] = carId;
    post("/robot/navigation/initPose", data, cb);
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
