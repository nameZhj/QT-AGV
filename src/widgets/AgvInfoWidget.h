#ifndef AGVINFOWIDGET_H
#define AGVINFOWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QTimer>
#include <QJsonObject>

// 对应 agv-info/index.vue - AGV 设备信息与控制面板
// 包含 MQTT 实时状态、日志输出、控制操作（自检/启动终端/停止终端/顶升置零）
class AgvInfoWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AgvInfoWidget(QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void onStartCheck();
    void onStartTerminal();
    void onStopTerminal();
    void onResetLift();
    void onMqttConnected();
    void onMqttDisconnected();

private:
    void setupUI();
    void initMqtt();
    void handleMqttMessage(const QString &topic, const QByteArray &payload);
    void pushLog(const QString &line);
    void scrollToLast();

    // 设备信息
    struct MachineInfo {
        QString alarminfo;
        double linear_x = 0;
        double power = 0;
        double x = 0;
        double y = 0;
        double angle = 0;
    } m_machine;

    // UI - 信息面板
    QLabel *m_alarmLabel = nullptr;
    QLabel *m_linearLabel = nullptr;
    QLabel *m_powerLabel = nullptr;
    QLabel *m_xLabel = nullptr;
    QLabel *m_yLabel = nullptr;
    QLabel *m_angleLabel = nullptr;

    // UI - 控制按钮
    QPushButton *m_checkBtn = nullptr;
    QPushButton *m_startBtn = nullptr;
    QPushButton *m_stopBtn = nullptr;
    QPushButton *m_resetLiftBtn = nullptr;

    // UI - 日志
    QTextEdit *m_logEdit = nullptr;

    // UI - 状态
    QLabel *m_mqttStatusLabel = nullptr;
    QLabel *m_logCountLabel = nullptr;

    // 状态
    bool m_mqttConnected = false;
    QStringList m_logs;
    int m_maxLogLines = 1000;

    // 自检输出
    QStringList m_messageList;
    int m_currentIndex = 0;
    bool m_isLoop = false;
    QTimer *m_checkTimer = nullptr;

    // 自检期间暂停常规 MQTT /log 打印，日志窗口只显示自检相关信息
    bool m_selfCheckActive = false;
};

#endif // AGVINFOWIDGET_H
