#include "MqttService.h"
#include <QJsonDocument>
#include <QJsonParseError>
#include <QDebug>

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
    // MQTT 端口 1883 → 映射端口 6016
    int mappedPort = (port == 1883) ? 6016 : port;
    m_client->setHostname(host);
    m_client->setPort(mappedPort);
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
    qDebug() << "MQTT error:" << error;
    emit errorChanged(QString::number(error));
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
