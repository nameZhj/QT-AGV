#include "MqttService.h"
#include <QJsonDocument>
#include <QJsonParseError>
#include <QDebug>
#include <QDateTime>

MqttService &MqttService::instance()
{
    static MqttService inst;
    return inst;
}

MqttService::MqttService(QObject *parent)
    : QObject(parent)
    , m_client(new QMqttClient(this))
{
    connect(m_client, &QMqttClient::connected, this, &MqttService::onConnected);
    connect(m_client, &QMqttClient::disconnected, this, &MqttService::onDisconnected);
    connect(m_client, &QMqttClient::errorChanged, this, &MqttService::onErrorChanged);
    connect(m_client, &QMqttClient::messageReceived, this, &MqttService::onMessageReceived);
}

MqttService::~MqttService()
{
    if (m_client && m_client->state() == QMqttClient::Connected) {
        m_client->disconnectFromHost();
    }
}

void MqttService::connectToHost(const QString &host, int port)
{
    const QString trimmedHost = host.trimmed();

    // 已在连接/已连接到同一服务器时直接返回。
    // 关键：不能在非 Disconnected 状态下调用 disconnectFromHost()——
    // 启动时多个组件几乎同时调用本单例的 connectToHost，若每次都对处于
    // ConnectingToBroker（TCP 握手中）的传输层执行 disconnect，会强行拆除
    // 正在握手的 socket，从而触发 TransportInvalid 错误。
    if (m_client->state() != QMqttClient::Disconnected
        && m_client->hostname() == trimmedHost
        && m_client->port() == quint16(port)) {
        return;
    }

    // 只有切换到不同的服务器时才需要先断开旧连接。
    // QMqttClient 的所有 setter 在非 Disconnected 状态下会静默失败，
    // 必须先确保回到 Disconnected。
    if (m_client->state() != QMqttClient::Disconnected) {
        m_client->disconnectFromHost();
        m_client->deleteLater();
        m_client = new QMqttClient(this);
        connect(m_client, &QMqttClient::connected, this, &MqttService::onConnected);
        connect(m_client, &QMqttClient::disconnected, this, &MqttService::onDisconnected);
        connect(m_client, &QMqttClient::errorChanged, this, &MqttService::onErrorChanged);
        connect(m_client, &QMqttClient::messageReceived, this, &MqttService::onMessageReceived);
    }

    m_client->setHostname(trimmedHost);
    m_client->setPort(port);
    m_client->setUsername("admin");
    m_client->setPassword("vsislab");
    m_client->setClientId(QString("qt-agv-%1").arg(QDateTime::currentMSecsSinceEpoch()));
    m_client->setKeepAlive(60);
    m_client->connectToHost();
}

void MqttService::disconnectFromHost()
{
    if (m_client) {
        m_client->disconnectFromHost();
    }
}

bool MqttService::isConnected() const
{
    return m_client && m_client->state() == QMqttClient::Connected;
}

void MqttService::subscribeTopic(const QString &topic, int qos, MessageCallback callback)
{
    if (!m_topics.contains(topic)) {
        m_topics[topic] = {qos, {}};
    }
    m_topics[topic].callbacks.append(callback);

    if (isConnected()) {
        m_client->subscribe(topic, qos);
    }
}

void MqttService::unsubscribeTopic(const QString &topic)
{
    if (m_topics.contains(topic)) {
        m_topics.remove(topic);
    }
    if (isConnected()) {
        m_client->unsubscribe(topic);
    }
}

void MqttService::publish(const QString &topic, const QByteArray &payload, int qos)
{
    if (!isConnected()) {
        qWarning() << "MQTT not connected, cannot publish to" << topic;
        return;
    }
    m_client->publish(topic, payload, qos);
}

void MqttService::onConnected()
{
    qDebug() << "MQTT connected";
    resubscribeAll();
    emit connected();
}

void MqttService::onDisconnected()
{
    qDebug() << "MQTT disconnected";
    emit disconnected();
}

void MqttService::onErrorChanged(QMqttClient::ClientError error)
{
    // 将错误码转换为可读字符串，便于排查
    QString errorStr;
    switch (error) {
    case QMqttClient::NoError:                errorStr = "NoError"; break;
    case QMqttClient::InvalidProtocolVersion: errorStr = "InvalidProtocolVersion"; break;
    case QMqttClient::IdRejected:             errorStr = "IdRejected"; break;
    case QMqttClient::ServerUnavailable:      errorStr = "ServerUnavailable"; break;
    case QMqttClient::BadUsernameOrPassword:  errorStr = "BadUsernameOrPassword"; break;
    case QMqttClient::NotAuthorized:          errorStr = "NotAuthorized"; break;
    case QMqttClient::TransportInvalid:       errorStr = "TransportInvalid"; break;
    case QMqttClient::ProtocolViolation:      errorStr = "ProtocolViolation"; break;
    case QMqttClient::UnknownError:           errorStr = "UnknownError"; break;
    case QMqttClient::Mqtt5SpecificError:     errorStr = "Mqtt5SpecificError"; break;
    default: errorStr = QString("Unknown(%1)").arg(static_cast<int>(error)); break;
    }
    qWarning() << "MQTT error:" << errorStr << "| client state:" << m_client->state()
               << "| hostname:" << m_client->hostname() << "| port:" << m_client->port();
    emit errorChanged(errorStr);
}

void MqttService::onMessageReceived(const QByteArray &message, const QMqttTopicName &topic)
{
    QString topicStr = topic.name();
    if (!m_topics.contains(topicStr)) return;

    const auto &config = m_topics[topicStr];
    for (const auto &cb : config.callbacks) {
        cb(topicStr, message);
    }
}

void MqttService::resubscribeAll()
{
    for (auto it = m_topics.begin(); it != m_topics.end(); ++it) {
        if (!it.value().callbacks.isEmpty()) {
            m_client->subscribe(it.key(), it.value().qos);
        }
    }
}

