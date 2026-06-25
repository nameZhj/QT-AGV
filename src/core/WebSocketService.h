#ifndef WEBSOCKETSERVICE_H
#define WEBSOCKETSERVICE_H

#include <QObject>
#include <QWebSocket>
#include <QJsonObject>

// WebSocket 服务封装，对应 Electron 中的 Vuex websocketMessage store
// 用于接收服务端推送的通知消息
class WebSocketService : public QObject
{
    Q_OBJECT
public:
    static WebSocketService &instance();

    void connectToServer(const QString &url);
    void disconnect();
    bool isConnected() const;

signals:
    void messageReceived(const QJsonObject &msg);
    void notificationReceived(const QJsonObject &msg);
    void connected();
    void disconnected();

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString &message);

private:
    explicit WebSocketService(QObject *parent = nullptr);
    ~WebSocketService() override;
    WebSocketService(const WebSocketService &) = delete;
    WebSocketService &operator=(const WebSocketService &) = delete;

    QWebSocket *m_socket;
};

#endif // WEBSOCKETSERVICE_H
