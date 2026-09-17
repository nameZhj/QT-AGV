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
    // 与 Vue 一致: 非存储类消息(messageType != "storage")都作为通知分发，
    // 通知类型由消息体内的 type 字段区分(navigation/pcd/agv/connect/crud 等)。
    // 存储类消息(messageType == "storage")走 messageReceived。
    QString messageType = obj.value("messageType").toString();
    if (messageType == "storage") {
        emit messageReceived(obj);
    } else {
        emit notificationReceived(obj);
    }
}
