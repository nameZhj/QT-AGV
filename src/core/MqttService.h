#ifndef MQTTSERVICE_H
#define MQTTSERVICE_H

#include <QObject>
#include <QMqttClient>
#include <QMap>
#include <QList>
#include <functional>

// MQTT 服务封装，对应 Electron 中的 @/utils/mqtt.js MqttClient
// 使用 Qt6 Mqtt 模块替代 mqtt.js (WebSocket 连接)
class MqttService : public QObject
{
    Q_OBJECT
public:
    static MqttService &instance();

    // 连接到 MQTT 服务器
    void connectToHost(const QString &host, int port = 1883);
    void disconnectFromHost();

    bool isConnected() const;

    // 订阅主题并添加回调 (对应 subscribeTopic)
    using MessageCallback = std::function<void(const QString &topic, const QByteArray &payload)>;
    void subscribeTopic(const QString &topic, int qos, MessageCallback callback);

    // 取消订阅 (对应 unsubscribeTopic)
    void unsubscribeTopic(const QString &topic);

    // 发布消息 (对应 publish)
    void publish(const QString &topic, const QByteArray &payload, int qos = 1);

signals:
    void connected();
    void disconnected();
    void errorChanged(const QString &error);

private slots:
    void onConnected();
    void onDisconnected();
    void onMessageReceived(const QByteArray &message, const QMqttTopicName &topic);
    void onErrorChanged(QMqttClient::ClientError error);

private:
    explicit MqttService(QObject *parent = nullptr);
    ~MqttService() override;
    MqttService(const MqttService &) = delete;
    MqttService &operator=(const MqttService &) = delete;

    void resubscribeAll();

    QMqttClient *m_client = nullptr;
    struct TopicConfig {
        int qos = 0;
        QList<MessageCallback> callbacks;
    };
    QMap<QString, TopicConfig> m_topics;
};

#endif // MQTTSERVICE_H
