/**
 * @file    ApiServiceCore.cpp
 * @layer   ProtocolComm
 * @brief   Core HTTP client lifecycle, request builders, reply handlers, and generic HTTP verbs.
 * @deps    ApiService.h, AppConfig.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "ApiService.h"
#include "AppConfig.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <QMap>
#include <QNetworkReply>
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

static QString formatNetworkError(QNetworkReply::NetworkError err)
{
    if (err == QNetworkReply::TimeoutError) {
        return QStringLiteral("Network timeout");
    }
    return QStringLiteral("Network error %1").arg(static_cast<int>(err));
}

static inline QJsonObject makeErrorJson(const QString &msg)
{
    return QJsonObject{{QStringLiteral("msg"), msg}};
}

void ApiService::handleReply(QNetworkReply *reply, const Callback &cb)
{
    connect(reply, &QNetworkReply::finished, this, [reply, cb]() {
        reply->deleteLater();
        const QString urlStr = reply->url().toString();

        if (reply->error() != QNetworkReply::NoError) {
            const int errCode = static_cast<int>(reply->error());
            const QString errStr = formatNetworkError(reply->error());
            if (shouldLogHttpError(urlStr, errCode)) {
                qWarning().noquote() << QStringLiteral("[HTTP ERROR] %1 -> %2").arg(urlStr, errStr);
            }
            cb(500, makeErrorJson(errStr));
            return;
        }

        const QByteArray data = reply->readAll();
        qDebug().noquote() << QStringLiteral("[HTTP RESP] %1 <- %2").arg(urlStr, QString::fromUtf8(data));

        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if (err.error != QJsonParseError::NoError) {
            cb(500, makeErrorJson(QStringLiteral("JSON parse error")));
            return;
        }

        const QJsonObject obj = doc.object();
        const int code = obj.value(QStringLiteral("code")).toInt(200);
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
            const QString errStr = formatNetworkError(reply->error());
            if (shouldLogHttpError(urlStr, errCode)) {
                qWarning().noquote() << QStringLiteral("[HTTP RAW ERROR] %1 -> %2").arg(urlStr, errStr);
            }
            cb(500, QByteArray());
            return;
        }

        const QByteArray data = reply->readAll();
        qDebug().noquote() << QStringLiteral("[HTTP RAW RESP] %1 <- (bytes: %2)").arg(urlStr).arg(data.size());
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
            const QString errStr = formatNetworkError(reply->error());
            if (shouldLogHttpError(urlStr, errCode)) {
                qWarning().noquote() << QStringLiteral("[HTTP FILE ERROR] %1 -> %2").arg(urlStr, errStr);
            }
            cb(false, QByteArray());
            return;
        }

        const QByteArray data = reply->readAll();
        qDebug().noquote() << QStringLiteral("[HTTP FILE RESP] %1 <- (size: %2 bytes)").arg(urlStr).arg(data.size());
        cb(true, data);
    });
}

void ApiService::get(const QString &url, const Callback &cb, int port, bool withToken)
{
    QString finalUrl = url;
    const QString timestampParam = "t=" + QString::number(QDateTime::currentMSecsSinceEpoch());
    finalUrl += (finalUrl.contains('?') ? '&' : '?') + timestampParam;

    const QString requestUrl = buildUrl(finalUrl, port);
    qDebug().noquote() << QStringLiteral("[HTTP SEND] GET %1").arg(requestUrl);

    const QNetworkRequest req = buildRequest(requestUrl, withToken, kDefaultTimeoutMs);
    QNetworkReply *reply = m_manager->get(req);
    handleReply(reply, cb);
}

void ApiService::post(const QString &url, const QJsonObject &data, const Callback &cb, int port, bool withToken)
{
    const QString requestUrl = buildUrl(url, port);
    const QByteArray body = QJsonDocument(data).toJson(QJsonDocument::Compact);
    qDebug().noquote() << QStringLiteral("[HTTP SEND] POST %1 -> %2").arg(requestUrl, QString::fromUtf8(body));

    const QNetworkRequest req = buildRequest(requestUrl, withToken, kDefaultTimeoutMs);
    QNetworkReply *reply = m_manager->post(req, body);
    handleReply(reply, cb);
}

void ApiService::postArray(const QString &url, const QJsonArray &data, const Callback &cb, int port, bool withToken)
{
    const QString requestUrl = buildUrl(url, port);
    const QByteArray body = QJsonDocument(data).toJson(QJsonDocument::Compact);
    qDebug().noquote() << QStringLiteral("[HTTP SEND] POST %1 -> %2").arg(requestUrl, QString::fromUtf8(body));

    const QNetworkRequest req = buildRequest(requestUrl, withToken, kDefaultTimeoutMs);
    QNetworkReply *reply = m_manager->post(req, body);
    handleReply(reply, cb);
}

void ApiService::put(const QString &url, const QJsonObject &data, const Callback &cb, int port, bool withToken)
{
    const QString requestUrl = buildUrl(url, port);
    const QByteArray body = QJsonDocument(data).toJson(QJsonDocument::Compact);
    qDebug().noquote() << QStringLiteral("[HTTP SEND] PUT %1 -> %2").arg(requestUrl, QString::fromUtf8(body));

    const QNetworkRequest req = buildRequest(requestUrl, withToken, kDefaultTimeoutMs);
    QNetworkReply *reply = m_manager->put(req, body);
    handleReply(reply, cb);
}

void ApiService::del(const QString &url, const Callback &cb, int port, bool withToken)
{
    const QString requestUrl = buildUrl(url, port);
    qDebug().noquote() << QStringLiteral("[HTTP SEND] DELETE %1").arg(requestUrl);

    const QNetworkRequest req = buildRequest(requestUrl, withToken, kDefaultTimeoutMs);
    QNetworkReply *reply = m_manager->deleteResource(req);
    handleReply(reply, cb);
}

void ApiService::login(const QString &username, const QString &password, const Callback &cb)
{
    post("/login", QJsonObject{{"username", username}, {"password", password}}, cb, 8048, false);
}
