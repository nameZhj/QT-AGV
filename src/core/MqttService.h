#pragma once

#include <QObject>
#include <QMqttClient>
#include <QMap>
#include <QList>
#include <QJsonObject>
#include <QStringList>
#include <functional>

/**
 * @brief 核心 MQTT 通信总线服务 (Thread-Safe Reactive Singleton)
 * 
 * 架构规范与特性：
 * 1. 完备的 MQTT 标准通配符匹配算法 (支持 '+' 单层与 '#' 多层通配，如 /rob/+/info 匹配 /rob/1/info)；
 * 2. 自动兼容 "/info" 别名至 "/rob/+/info"，彻底解决小车多机广播场景下的信息捕获；
 * 3. 强制 NoProxy 局域网直连，杜绝开发者/运维环境开启代理工具导致 TransportInvalid 掉线；
 * 4. 订阅回调快照执行 (Reentrancy-Safe)，杜绝回调内退订导致迭代崩溃；
 * 5. 智能退避重连 (Exponential Backoff) 与便捷 JSON 消息订阅通道；
 * 6. 控制台调试日志可控开关 (默认关闭，彻底杜绝高频数据帧刷屏)；
 * 7. 100% 保持所有既有业务 API 签名向下兼容。
 */
class MqttService final : public QObject
{
    Q_OBJECT

public:
    static MqttService &instance();

    using MessageCallback     = std::function<void(const QString &topic, const QByteArray &payload)>;
    using JsonMessageCallback = std::function<void(const QString &topic, const QJsonObject &jsonObj)>;

    // 连接与断开
    void connectToHost(const QString &host, int port = 1883);
    void disconnectFromHost();

    bool isConnected() const;
    QMqttClient::ClientState state() const;

    // 控制台日志开关 (默认 false 关闭)
    void setLoggingEnabled(bool enabled) noexcept { m_loggingEnabled = enabled; }
    bool isLoggingEnabled() const noexcept { return m_loggingEnabled; }

    // 原始字节消息订阅
    void subscribeTopic(const QString &topic, int qos, MessageCallback callback);

    // 便捷 JSON 消息订阅 (自动解析并在格式合法时派发)
    void subscribeJsonTopic(const QString &topic, int qos, JsonMessageCallback callback);

    // 取消订阅
    void unsubscribeTopic(const QString &topic);

    // 发布消息
    void publish(const QString &topic, const QByteArray &payload, int qos = 1);
    void publishJson(const QString &topic, const QJsonObject &jsonObj, int qos = 1);

    // MQTT 主题匹配工具函数 (支持 + / # 通配符及前缀容错)
    static bool matchTopic(const QString &filter, const QString &topic);

signals:
    void connected();
    void disconnected();
    void stateChanged(QMqttClient::ClientState state);
    void errorChanged(const QString &error);

private slots:
    void onConnected();
    void onDisconnected();
    void onMessageReceived(const QByteArray &message, const QMqttTopicName &topic);
    void onErrorChanged(QMqttClient::ClientError error);
    void onReconnectTimeout();

private:
    explicit MqttService(QObject *parent = nullptr);
    ~MqttService() override;
    MqttService(const MqttService &) = delete;
    MqttService &operator=(const MqttService &) = delete;

    void setupClient();
    void resubscribeAll();
    static QStringList getTopicVariants(const QString &topic);

    QMqttClient *m_client = nullptr;
    QString      m_lastHost;
    int          m_lastPort = 1883;
    bool         m_explicitDisconnect = false;
    bool         m_loggingEnabled = false;

    class QTimer *m_reconnectTimer = nullptr;
    int          m_currentBackoffMs = 2000;
    static constexpr int kMinBackoffMs = 2000;
    static constexpr int kMaxBackoffMs = 15000;

    struct TopicConfig {
        int qos = 0;
        QList<MessageCallback> callbacks;
    };
    QMap<QString, TopicConfig> m_topics;
};
