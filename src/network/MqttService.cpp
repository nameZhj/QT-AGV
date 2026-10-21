/**
 * @file    MqttService.cpp
 * @layer   DataComm
 * @brief   Core MQTT communication bus service with backoff reconnect and topic routing.
 * @deps    MqttService.h, MqttTopicRouter.h, AppConfig.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MqttService.h"
#include "MqttTopicRouter.h"
#include "AppConfig.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QDebug>
#include <QDateTime>
#include <QTimer>
#include <QNetworkProxy>

MqttService &MqttService::instance()
{
    static MqttService inst;
    return inst;
}

MqttService::MqttService(QObject *parent)
    : QObject(parent), m_client(new QMqttClient(this)), m_reconnectTimer(new QTimer(this))
{
    setupClient();
    m_reconnectTimer->setInterval(m_currentBackoffMs);
    connect(m_reconnectTimer, &QTimer::timeout, this, &MqttService::onReconnectTimeout);
}

MqttService::~MqttService()
{
    if (m_reconnectTimer) m_reconnectTimer->stop();
    if (m_client && m_client->state() == QMqttClient::Connected) m_client->disconnectFromHost();
}

void MqttService::setupClient()
{
    if (!m_client) m_client = new QMqttClient(this);
    connect(m_client, &QMqttClient::connected, this, &MqttService::onConnected);
    connect(m_client, &QMqttClient::disconnected, this, &MqttService::onDisconnected);
    connect(m_client, &QMqttClient::stateChanged, this, &MqttService::stateChanged);
    connect(m_client, &QMqttClient::errorChanged, this, &MqttService::onErrorChanged);
    connect(m_client, &QMqttClient::messageReceived, this, &MqttService::onMessageReceived);
}

void MqttService::connectToHost(const QString &host, int port)
{
    const QString trimmedHost = host.trimmed();
    if (trimmedHost.isEmpty()) return;

    m_explicitDisconnect = false;
    if (m_reconnectTimer) m_reconnectTimer->stop();
    m_currentBackoffMs = kMinBackoffMs;

    if (m_client && (m_client->state() == QMqttClient::Connected || m_client->state() == QMqttClient::Connecting)
        && m_client->hostname() == trimmedHost && m_client->port() == static_cast<quint16>(port)) {
        if (m_client->state() == QMqttClient::Connected) resubscribeAll();
        return;
    }

    if (m_client) {
        m_client->disconnect(this);
        m_client->disconnectFromHost();
        m_client->deleteLater();
        m_client = nullptr;
    }
    setupClient();

    m_lastHost = trimmedHost;
    m_lastPort = port;
    m_client->setHostname(trimmedHost);
    m_client->setPort(static_cast<quint16>(port));
    m_client->setUsername(QStringLiteral("admin"));
    m_client->setPassword(QStringLiteral("vsislab"));
    m_client->setClientId(QStringLiteral("rt-agv-%1").arg(QDateTime::currentMSecsSinceEpoch()));
    m_client->setKeepAlive(60);

    qInfo().noquote() << QStringLiteral("[MQTT Service] 正在连接设备 MQTT: %1:%2").arg(trimmedHost).arg(port);
    m_client->connectToHost();
}

void MqttService::disconnectFromHost()
{
    m_explicitDisconnect = true;
    if (m_reconnectTimer) m_reconnectTimer->stop();
    if (m_client) {
        m_client->disconnect(this);
        m_client->disconnectFromHost();
        m_client->deleteLater();
        m_client = nullptr;
        setupClient();
    }
    emit disconnected();
}

bool MqttService::isConnected() const
{
    return m_client && m_client->state() == QMqttClient::Connected;
}

QMqttClient::ClientState MqttService::state() const
{
    return m_client ? m_client->state() : QMqttClient::Disconnected;
}

void MqttService::subscribeTopic(const QString &topic, int qos, MessageCallback callback)
{
    if (!m_topics.contains(topic)) m_topics[topic] = {qos, {}};
    if (callback) m_topics[topic].callbacks.append(callback);

    if (isConnected()) {
        for (const QString &varTopic : getTopicVariants(topic)) {
            m_client->subscribe(varTopic, static_cast<quint8>(qos));
        }
    }
}

void MqttService::setTopicSubscription(const QString &topic, int qos, MessageCallback callback)
{
    m_topics[topic] = {qos, callback ? QList<MessageCallback>{callback} : QList<MessageCallback>{}};
    if (isConnected()) {
        for (const QString &varTopic : getTopicVariants(topic)) {
            m_client->subscribe(varTopic, static_cast<quint8>(qos));
        }
    }
}

bool MqttService::isGlobalTopic(const QString &topic) { return MqttTopicRouter::isGlobalTopic(topic); }
bool MqttService::matchTopic(const QString &filter, const QString &topic) { return MqttTopicRouter::matchTopic(filter, topic); }
QStringList MqttService::getTopicVariants(const QString &topic) { return MqttTopicRouter::getTopicVariants(topic); }

void MqttService::subscribeJsonTopic(const QString &topic, int qos, JsonMessageCallback callback)
{
    if (!callback) return;
    subscribeTopic(topic, qos, [callback](const QString &t, const QByteArray &payload) {
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
        if (err.error == QJsonParseError::NoError && doc.isObject()) callback(t, doc.object());
    });
}

void MqttService::unsubscribeTopic(const QString &topic)
{
    if (isGlobalTopic(topic)) return;
    if (m_topics.contains(topic)) m_topics.remove(topic);
    if (isConnected()) {
        for (const QString &varTopic : getTopicVariants(topic)) m_client->unsubscribe(varTopic);
    }
}

void MqttService::clearAllSubscriptions()
{
    QList<QString> keysToRemove;
    for (auto it = m_topics.cbegin(); it != m_topics.cend(); ++it) {
        if (!isGlobalTopic(it.key())) {
            keysToRemove.append(it.key());
            if (isConnected() && m_client) {
                for (const QString &varTopic : getTopicVariants(it.key())) m_client->unsubscribe(varTopic);
            }
        }
    }
    for (const QString &k : keysToRemove) m_topics.remove(k);
}

void MqttService::publish(const QString &topic, const QByteArray &payload, int qos)
{
    if (!isConnected()) {
        qWarning().noquote() << QStringLiteral("[MQTT PUB FAIL] Not connected, topic: ") << topic;
        return;
    }
    if (m_loggingEnabled && !topic.contains(QStringLiteral("/log")) && topic != QStringLiteral("log")) {
        qDebug().noquote() << QStringLiteral("[MQTT PUB] Topic: %1 | Payload: %2").arg(topic, QString::fromUtf8(payload));
    }
    m_client->publish(topic, payload, static_cast<quint8>(qos));
}

void MqttService::publishJson(const QString &topic, const QJsonObject &jsonObj, int qos)
{
    publish(topic, QJsonDocument(jsonObj).toJson(QJsonDocument::Compact), qos);
}

void MqttService::onConnected()
{
    if (m_loggingEnabled) qDebug().noquote() << QStringLiteral("[MQTT Connected] Connected to broker successfully");
    if (m_reconnectTimer) m_reconnectTimer->stop();
    m_currentBackoffMs = kMinBackoffMs;
    resubscribeAll();
    emit connected();
}

void MqttService::onDisconnected()
{
    if (m_loggingEnabled) qDebug().noquote() << QStringLiteral("[MQTT Disconnected] Disconnected from broker");
    emit disconnected();
    if (!m_explicitDisconnect && !m_lastHost.isEmpty() && m_reconnectTimer && !m_reconnectTimer->isActive()) {
        m_reconnectTimer->setInterval(m_currentBackoffMs);
        m_reconnectTimer->start();
    }
}

void MqttService::onErrorChanged(QMqttClient::ClientError error)
{
    const QString errorStr = MqttTopicRouter::clientErrorToString(error);
    qWarning().noquote() << QStringLiteral("[MQTT ERROR] %1 | state: %2 | host: %3:%4")
                                .arg(errorStr).arg(m_client->state()).arg(m_client->hostname()).arg(m_client->port());
    if (error != QMqttClient::NoError && !m_explicitDisconnect && !m_lastHost.isEmpty() && m_reconnectTimer && !m_reconnectTimer->isActive()) {
        m_reconnectTimer->setInterval(m_currentBackoffMs);
        m_reconnectTimer->start();
    }
    emit errorChanged(errorStr);
}

void MqttService::onReconnectTimeout()
{
    if (!m_explicitDisconnect && !m_lastHost.isEmpty() && m_client && m_client->state() == QMqttClient::Disconnected) {
        if (m_loggingEnabled) {
            qDebug().noquote() << QStringLiteral("[MQTT Reconnect] 尝试重新连接到 %1:%2 (退避周期: %3ms)")
                                      .arg(m_lastHost).arg(m_lastPort).arg(m_currentBackoffMs);
        }
        m_client->connectToHost();
        m_currentBackoffMs = qMin(kMaxBackoffMs, static_cast<int>(m_currentBackoffMs * 1.5));
        m_reconnectTimer->setInterval(m_currentBackoffMs);
    }
}

void MqttService::onMessageReceived(const QByteArray &message, const QMqttTopicName &topic)
{
    const QString topicStr = topic.name();
    if (m_loggingEnabled && !topicStr.contains(QStringLiteral("/log")) && topicStr != QStringLiteral("log")) {
        qDebug().noquote() << QStringLiteral("[MQTT RECV] Topic: %1 | Payload: %2").arg(topicStr, QString::fromUtf8(message));
    }

    QList<MessageCallback> callbacksToInvoke;
    for (auto it = m_topics.cbegin(); it != m_topics.cend(); ++it) {
        if (matchTopic(it.key(), topicStr)) callbacksToInvoke.append(it.value().callbacks);
    }

    for (const auto &cb : callbacksToInvoke) {
        if (cb) cb(topicStr, message);
    }
}

void MqttService::resubscribeAll()
{
    for (auto it = m_topics.cbegin(); it != m_topics.cend(); ++it) {
        if (!it.value().callbacks.isEmpty()) {
            for (const QString &varTopic : getTopicVariants(it.key())) {
                m_client->subscribe(varTopic, static_cast<quint8>(it.value().qos));
            }
        }
    }
}
