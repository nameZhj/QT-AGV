/**
 * @file    ApiServiceFile.cpp
 * @layer   ProtocolComm
 * @brief   Multipart file upload, map PGM payload transfer, file download, and raw byte stream APIs.
 * @deps    ApiService.h, AppConfig.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "ApiService.h"
#include "AppConfig.h"

#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QNetworkReply>

void ApiService::uploadFile(const QString &url, const QString &filePath, const QString &fieldName,
                            const Callback &cb, int port)
{
    auto *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    auto *file = new QFile(filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        delete file;
        delete multiPart;
        cb(500, QJsonObject{{QStringLiteral("msg"), QStringLiteral("Cannot open file")}});
        return;
    }

    QHttpPart part;
    part.setHeader(QNetworkRequest::ContentDispositionHeader,
                   QVariant("form-data; name=\"" + fieldName + "\"; filename=\"" + QFileInfo(filePath).fileName() + "\""));
    part.setBodyDevice(file);
    file->setParent(multiPart);
    multiPart->append(part);

    QNetworkRequest req = buildRequest(buildUrl(url, port), true, kUploadTimeoutMs);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QVariant());
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
