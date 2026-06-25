#include "WebSocketService.h"
#include "AppConfig.h"
#include <QJsonDocument>
#include <QDebug>

WebSocketService &WebSocketService::instance()
{
    static WebSocketService inst;
    return inst;
}

WebSocketService::WebSocketService(QObject *parent)
    : QObject(parent)
    , m_socket(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this))
{
    connect(m_socket, &QWebSocket::connected, this, &WebSocketService::onConnected);
    connect(m_socket, &QWebSocket::disconnected, this, &WebSocketService::onDisconnected);
    connect(m_socket, &QWebSocket::textMessageReceived, this, &WebSocketService::onTextMessageReceived);
}

WebSocketService::~WebSocketService()
{
    if (m_socket) m_socket->close();
}

void WebSocketService::connectToServer(const QString &url)
{
    QNetworkRequest req(url);
    QString tok = AppConfig::instance().token();
    if (!tok.isEmpty()) {
        req.setRawHeader("Authorization", ("Bearer " + tok).toUtf8());
    }
    m_socket->open(req);
}

void WebSocketService::disconnect()
{
    if (m_socket) m_socket->close();
}

bool WebSocketService::isConnected() const
{
    return m_socket && m_socket->state() == QAbstractSocket::ConnectedState;
}

void WebSocketService::onConnected()
{
    qDebug() << "WebSocket connected";
    emit connected();
}

void WebSocketService::onDisconnected()
{
    qDebug() << "WebSocket disconnected";
    emit disconnected();
}

void WebSocketService::onTextMessageReceived(const QString &message)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError) return;

    QJsonObject obj = doc.object();
    // 区分通知消息和普通消息
    if (obj.contains("type") && obj["type"].toString() == "notification") {
        emit notificationReceived(obj);
    } else {
        emit messageReceived(obj);
    }
}
