#include "MqttService.h"
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
    : QObject(parent)
    , m_client(new QMqttClient(this))
    , m_reconnectTimer(new QTimer(this))
{
    setupClient();

    m_reconnectTimer->setInterval(m_currentBackoffMs);
    connect(m_reconnectTimer, &QTimer::timeout, this, &MqttService::onReconnectTimeout);
}

MqttService::~MqttService()
{
    if (m_reconnectTimer) {
        m_reconnectTimer->stop();
    }
    if (m_client && m_client->state() == QMqttClient::Connected) {
        m_client->disconnectFromHost();
    }
}

void MqttService::setupClient()
{
    if (!m_client) {
        m_client = new QMqttClient(this);
    }

    connect(m_client, &QMqttClient::connected, this, &MqttService::onConnected);
    connect(m_client, &QMqttClient::disconnected, this, &MqttService::onDisconnected);
    connect(m_client, &QMqttClient::stateChanged, this, &MqttService::stateChanged);
    connect(m_client, &QMqttClient::errorChanged, this, &MqttService::onErrorChanged);
    connect(m_client, &QMqttClient::messageReceived, this, &MqttService::onMessageReceived);
}

void MqttService::connectToHost(const QString &host, int port)
{
    const QString trimmedHost = host.trimmed();
    if (trimmedHost.isEmpty()) {
        return;
    }

    m_explicitDisconnect = false;

    // 切换设备或重启: 停止正在进行的重连定时器并重置退避周期
    if (m_reconnectTimer) {
        m_reconnectTimer->stop();
    }
    m_currentBackoffMs = kMinBackoffMs;

    // 若当前已成功连接至同一主机IP，避免重复破坏握手
    if (m_client && m_client->state() == QMqttClient::Connected
        && m_client->hostname() == trimmedHost
        && m_client->port() == static_cast<quint16>(port)) {
        return;
    }

    // IP 是区分设备的唯一标识，切换设备时彻底关闭旧连接、销毁旧 client 并重启
    if (m_client) {
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
    m_client->setClientId(QStringLiteral("qt-agv-%1").arg(QDateTime::currentMSecsSinceEpoch()));
    m_client->setKeepAlive(60);

    qInfo().noquote() << QStringLiteral("[MQTT Service] 正在连接设备 MQTT: %1:%2").arg(trimmedHost).arg(port);
    m_client->connectToHost();
}

void MqttService::disconnectFromHost()
{
    m_explicitDisconnect = true;
    if (m_reconnectTimer) {
        m_reconnectTimer->stop();
    }
    if (m_client) {
        m_client->disconnectFromHost();
        m_client->deleteLater();
        m_client = nullptr;
        setupClient();
    }
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
    if (!m_topics.contains(topic)) {
        m_topics[topic] = {qos, {}};
    }
    m_topics[topic].callbacks.append(callback);

    if (isConnected()) {
        const QStringList variants = getTopicVariants(topic);
        for (const QString &varTopic : variants) {
            m_client->subscribe(varTopic, static_cast<quint8>(qos));
        }
    }
}

void MqttService::subscribeJsonTopic(const QString &topic, int qos, JsonMessageCallback callback)
{
    if (!callback) {
        return;
    }
    subscribeTopic(topic, qos, [callback](const QString &t, const QByteArray &payload) {
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
        if (err.error == QJsonParseError::NoError && doc.isObject()) {
            callback(t, doc.object());
        }
    });
}

void MqttService::unsubscribeTopic(const QString &topic)
{
    if (m_topics.contains(topic)) {
        m_topics.remove(topic);
    }
    if (isConnected()) {
        const QStringList variants = getTopicVariants(topic);
        for (const QString &varTopic : variants) {
            m_client->unsubscribe(varTopic);
        }
    }
}

void MqttService::clearAllSubscriptions()
{
    if (isConnected() && m_client) {
        for (auto it = m_topics.cbegin(); it != m_topics.cend(); ++it) {
            const QStringList variants = getTopicVariants(it.key());
            for (const QString &varTopic : variants) {
                m_client->unsubscribe(varTopic);
            }
        }
    }
    m_topics.clear();
}

void MqttService::publish(const QString &topic, const QByteArray &payload, int qos)
{
    if (!isConnected()) {
        qWarning().noquote() << QStringLiteral("[MQTT PUB FAIL] Not connected, topic: ") << topic;
        return;
    }
    if (m_loggingEnabled && !topic.contains(QStringLiteral("/log")) && topic != QStringLiteral("log")) {
        qDebug().noquote() << QStringLiteral("[MQTT PUB] Topic: %1 | Payload: %2")
                              .arg(topic, QString::fromUtf8(payload));
    }
    m_client->publish(topic, payload, static_cast<quint8>(qos));
}

void MqttService::publishJson(const QString &topic, const QJsonObject &jsonObj, int qos)
{
    publish(topic, QJsonDocument(jsonObj).toJson(QJsonDocument::Compact), qos);
}

void MqttService::onConnected()
{
    if (m_loggingEnabled) {
        qDebug().noquote() << QStringLiteral("[MQTT Connected] Connected to broker successfully");
    }
    if (m_reconnectTimer) {
        m_reconnectTimer->stop();
    }
    m_currentBackoffMs = kMinBackoffMs;
    resubscribeAll();
    emit connected();
}

void MqttService::onDisconnected()
{
    if (m_loggingEnabled) {
        qDebug().noquote() << QStringLiteral("[MQTT Disconnected] Disconnected from broker");
    }
    emit disconnected();
    if (!m_explicitDisconnect && !m_lastHost.isEmpty() && m_reconnectTimer && !m_reconnectTimer->isActive()) {
        m_reconnectTimer->setInterval(m_currentBackoffMs);
        m_reconnectTimer->start();
    }
}

void MqttService::onErrorChanged(QMqttClient::ClientError error)
{
    QString errorStr;
    switch (error) {
    case QMqttClient::NoError:                errorStr = QStringLiteral("NoError"); break;
    case QMqttClient::InvalidProtocolVersion: errorStr = QStringLiteral("InvalidProtocolVersion"); break;
    case QMqttClient::IdRejected:             errorStr = QStringLiteral("IdRejected"); break;
    case QMqttClient::ServerUnavailable:      errorStr = QStringLiteral("ServerUnavailable"); break;
    case QMqttClient::BadUsernameOrPassword:  errorStr = QStringLiteral("BadUsernameOrPassword"); break;
    case QMqttClient::NotAuthorized:          errorStr = QStringLiteral("NotAuthorized"); break;
    case QMqttClient::TransportInvalid:       errorStr = QStringLiteral("TransportInvalid"); break;
    case QMqttClient::ProtocolViolation:      errorStr = QStringLiteral("ProtocolViolation"); break;
    case QMqttClient::UnknownError:           errorStr = QStringLiteral("UnknownError"); break;
    case QMqttClient::Mqtt5SpecificError:     errorStr = QStringLiteral("Mqtt5SpecificError"); break;
    default: errorStr = QStringLiteral("Unknown(%1)").arg(static_cast<int>(error)); break;
    }

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

bool MqttService::matchTopic(const QString &filter, const QString &topic)
{
    if (filter == topic) {
        return true;
    }

    // 归一化：剥离前导冒号与多余前导斜杠，确保标准绝对路径格式
    auto clean = [](QString s) -> QString {
        s = s.trimmed();
        while (s.startsWith(':')) s = s.mid(1);
        if (!s.startsWith('/')) s = '/' + s;
        while (s.endsWith('/') && s.length() > 1) s.chop(1);
        return s;
    };

    const QString cleanFilter = clean(filter);
    const QString cleanTopic  = clean(topic);

    if (cleanFilter == cleanTopic) {
        return true;
    }

    // 不存在 "/rob/info" 和 "/info"，只有 "/rob/1/info"
    // 若有遗留接口传入 "/info"、"info" 或 "/rob/info" 订阅过滤器，严格仅匹配真实唯一的 "/rob/1/info" 话题
    if (cleanFilter == QStringLiteral("/info") || cleanFilter == QStringLiteral("info") || cleanFilter == QStringLiteral("/rob/info")) {
        return cleanTopic == QStringLiteral("/rob/1/info");
    }

    // MQTT 标准分层通配符匹配 ('+' 匹配单级, '#' 匹配多级)
    const QStringList filterParts = cleanFilter.split('/', Qt::KeepEmptyParts);
    const QStringList topicParts  = cleanTopic.split('/', Qt::KeepEmptyParts);

    int f = 0;
    int t = 0;
    while (f < filterParts.size() && t < topicParts.size()) {
        const QString &fp = filterParts[f];
        const QString &tp = topicParts[t];

        if (fp == QStringLiteral("#")) {
            return true;
        } else if (fp == QStringLiteral("+") || fp == tp) {
            ++f;
            ++t;
        } else {
            return false;
        }
    }

    if (f == filterParts.size() - 1 && filterParts[f] == QStringLiteral("#")) {
        return true;
    }

    return (f == filterParts.size() && t == topicParts.size());
}

void MqttService::onMessageReceived(const QByteArray &message, const QMqttTopicName &topic)
{
    const QString topicStr = topic.name();

    // 打印接收日志 (受 m_loggingEnabled 开关控制，默认关闭以避免刷屏)
    if (m_loggingEnabled && !topicStr.contains(QStringLiteral("/log")) && topicStr != QStringLiteral("log")) {
        qDebug().noquote() << QStringLiteral("[MQTT RECV] Topic: %1 | Payload: %2")
                              .arg(topicStr, QString::fromUtf8(message));
    }

    // 快照式拷贝待执行回调列表，杜绝业务回调中退订/订阅导致容器迭代器失效崩溃 (Reentrancy Safety)
    QList<MessageCallback> callbacksToInvoke;
    for (auto it = m_topics.cbegin(); it != m_topics.cend(); ++it) {
        if (matchTopic(it.key(), topicStr)) {
            callbacksToInvoke.append(it.value().callbacks);
        }
    }

    for (const auto &cb : callbacksToInvoke) {
        if (cb) {
            cb(topicStr, message);
        }
    }
}

void MqttService::resubscribeAll()
{
    for (auto it = m_topics.cbegin(); it != m_topics.cend(); ++it) {
        if (!it.value().callbacks.isEmpty()) {
            const QStringList variants = getTopicVariants(it.key());
            for (const QString &varTopic : variants) {
                m_client->subscribe(varTopic, static_cast<quint8>(it.value().qos));
            }
        }
    }
}

QStringList MqttService::getTopicVariants(const QString &topic)
{
    QStringList variants;
    const QString trimmed = topic.trimmed();
    if (trimmed.isEmpty()) {
        return variants;
    }
    variants.append(trimmed);

    // 标准前缀容错
    if (trimmed.startsWith(QStringLiteral(":/")) && trimmed.length() > 2) {
        variants.append(trimmed.mid(1));
        variants.append(trimmed.mid(2));
    } else if (trimmed.startsWith('/') && trimmed.length() > 1) {
        variants.append(trimmed.mid(1));
        variants.append(QStringLiteral(":") + trimmed);
    } else if (!trimmed.startsWith('/')) {
        variants.append(QStringLiteral("/") + trimmed);
        variants.append(QStringLiteral(":/") + trimmed);
    }

    // 保持单车专用订阅，绝不自动扩充多车通配符 /rob/+/info，彻底避免多台车数据窜扰
    variants.removeDuplicates();
    return variants;
}
