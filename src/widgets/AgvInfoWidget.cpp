#include "AgvInfoWidget.h"
#include "core/AppConfig.h"
#include "core/MqttService.h"
#include "core/ApiService.h"
#include "core/Toast.h"
#include "core/RobotInfoManager.h"
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QScrollBar>
#include <QMessageBox>
#include <QJsonDocument>
#include <QSplitter>
#include <QFrame>
#include <QDateTime>

AgvInfoWidget::AgvInfoWidget(QWidget *parent)
    : QWidget(parent)
    , m_checkTimer(new QTimer(this))
{
    setupUI();
    m_checkTimer->setSingleShot(true);
    connect(m_checkTimer, &QTimer::timeout, this, [this]() {
        if (!m_isLoop) return;
        if (m_currentIndex >= m_messageList.size()) {
            m_isLoop = false;
            m_selfCheckActive = false;
            return;
        }
        pushLog(m_messageList[m_currentIndex]);
        m_currentIndex++;
        m_checkTimer->start(900);
    });

    connect(&MqttService::instance(), &MqttService::connected, this, &AgvInfoWidget::onMqttConnected);
    connect(&MqttService::instance(), &MqttService::disconnected, this, &AgvInfoWidget::onMqttDisconnected);

    connect(&RobotInfoManager::instance(), &RobotInfoManager::currentRobotInfoUpdated,
            this, &AgvInfoWidget::updateFromRobotInfo);
    updateFromRobotInfo(RobotInfoManager::instance().currentInfo());
}

void AgvInfoWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    updateFromRobotInfo(RobotInfoManager::instance().currentInfo());
    // 如果 MQTT 已连接但尚未初始化订阅，则初始化
    if (MqttService::instance().isConnected()) {
        initMqtt();
    }
}

void AgvInfoWidget::setupUI()
{
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setStyleSheet("QSplitter::handle { background: #333; }");

    // ===== 左侧面板 =====
    QWidget *leftPanel = new QWidget;
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(8, 8, 8, 8);

    // 设备信息卡片
    QGroupBox *infoGroup = new QGroupBox("设备信息");
    infoGroup->setStyleSheet("QGroupBox { font-weight: bold; color: #ddd; }");
    QFormLayout *infoLayout = new QFormLayout(infoGroup);
    infoLayout->setLabelAlignment(Qt::AlignRight);

    m_alarmLabel = new QLabel("无");
    m_linearLabel = new QLabel("0");
    m_powerLabel = new QLabel("0%");
    m_xLabel = new QLabel("0");
    m_yLabel = new QLabel("0");
    m_angleLabel = new QLabel("0");

    QString valueStyle = "color: #00ff66; font-family: Consolas, monospace; font-weight: bold;";
    for (auto lbl : {m_alarmLabel, m_linearLabel, m_powerLabel, m_xLabel, m_yLabel, m_angleLabel})
        lbl->setStyleSheet(valueStyle);

    infoLayout->addRow("告警:", m_alarmLabel);
    infoLayout->addRow("线速度:", m_linearLabel);
    infoLayout->addRow("电量:", m_powerLabel);
    infoLayout->addRow("X:", m_xLabel);
    infoLayout->addRow("Y:", m_yLabel);
    infoLayout->addRow("方位角:", m_angleLabel);

    leftLayout->addWidget(infoGroup);

    // 控制操作卡片
    QGroupBox *ctrlGroup = new QGroupBox("控制操作");
    ctrlGroup->setStyleSheet("QGroupBox { font-weight: bold; color: #ddd; }");
    QVBoxLayout *ctrlLayout = new QVBoxLayout(ctrlGroup);

    m_checkBtn = new QPushButton("开机自检");
    m_checkBtn->setStyleSheet("QPushButton { padding: 10px; font-weight: bold; }");
    connect(m_checkBtn, &QPushButton::clicked, this, &AgvInfoWidget::onStartCheck);

    QHBoxLayout *termRow = new QHBoxLayout;
    m_startBtn = new QPushButton("启动终端");
    m_startBtn->setStyleSheet("QPushButton { padding: 10px; font-weight: bold; background: #2196F3; color: white; }");
    m_stopBtn = new QPushButton("停止终端");
    m_stopBtn->setStyleSheet("QPushButton { padding: 10px; font-weight: bold; background: #f44336; color: white; }");
    connect(m_startBtn, &QPushButton::clicked, this, &AgvInfoWidget::onStartTerminal);
    connect(m_stopBtn, &QPushButton::clicked, this, &AgvInfoWidget::onStopTerminal);
    termRow->addWidget(m_startBtn);
    termRow->addWidget(m_stopBtn);

    m_resetLiftBtn = new QPushButton("顶升高度置零");
    m_resetLiftBtn->setStyleSheet("QPushButton { padding: 10px; font-weight: bold; background: #FF9800; color: white; }");
    connect(m_resetLiftBtn, &QPushButton::clicked, this, &AgvInfoWidget::onResetLift);

    ctrlLayout->addWidget(m_checkBtn);
    ctrlLayout->addLayout(termRow);
    ctrlLayout->addWidget(m_resetLiftBtn);

    leftLayout->addWidget(ctrlGroup);

    // 状态面板
    QHBoxLayout *statusLayout = new QHBoxLayout;
    m_mqttStatusLabel = new QLabel("MQTT: 离线");
    m_mqttStatusLabel->setStyleSheet("color: #999;");
    m_logCountLabel = new QLabel("日志行数: 0");
    m_logCountLabel->setStyleSheet("color: #999;");
    statusLayout->addWidget(m_mqttStatusLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(m_logCountLabel);
    leftLayout->addLayout(statusLayout);

    leftLayout->addStretch();

    // ===== 右侧日志面板 =====
    QWidget *rightPanel = new QWidget;
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(8, 8, 8, 8);

    QLabel *logTitle = new QLabel("系统日志");
    logTitle->setStyleSheet("font-weight: bold; font-size: 14px; color: #ddd;");
    rightLayout->addWidget(logTitle);

    m_logEdit = new QTextEdit;
    m_logEdit->setReadOnly(true);
    m_logEdit->setStyleSheet(
        "QTextEdit {"
        "  background: #000000;"
        "  color: #00ff66;"
        "  font-family: Consolas, 'DejaVu Sans Mono', monospace;"
        "  font-size: 13px;"
        "  border: 1px solid #333;"
        "}"
    );
    rightLayout->addWidget(m_logEdit);

    splitter->addWidget(leftPanel);
    splitter->addWidget(rightPanel);
    splitter->setSizes({360, 800});

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(splitter);
}

void AgvInfoWidget::initMqtt()
{
    auto &mqtt = MqttService::instance();

    // 如果MQTT未连接，使用 serverIp 和映射端口连接
    if (!mqtt.isConnected()) {
        QString ip = AppConfig::instance().serverIp();
        mqtt.connectToHost(ip, 1883);  // MqttService内部会映射到6016
    }

    QString agvId = AppConfig::instance().agvId();
    if (!agvId.isEmpty()) {
        QString base = QString("/rob/%1/").arg(agvId);
        mqtt.subscribeTopic(base + "log", 0, [this](const QString &t, const QByteArray &m) {
            handleMqttMessage(t, m);
        });
        mqtt.subscribeTopic(base + "status", 0, [this](const QString &t, const QByteArray &m) {
            handleMqttMessage(t, m);
        });
    }
}

void AgvInfoWidget::updateFromRobotInfo(const RobotInfo &info)
{
    if (info.hasPose) {
        m_machine.x = info.car_x;
        m_machine.y = info.car_y;
        m_machine.angle = info.car_yaw * 180.0 / M_PI;
        if (m_xLabel) m_xLabel->setText(QString::number(m_machine.x, 'f', 2));
        if (m_yLabel) m_yLabel->setText(QString::number(m_machine.y, 'f', 2));
        if (m_angleLabel) m_angleLabel->setText(QString::number(m_machine.angle, 'f', 2));
    }
    if (info.actual_vel.valid) {
        m_machine.linear_x = info.actualLinearSpeed();
        if (m_linearLabel) m_linearLabel->setText(QString::number(m_machine.linear_x, 'f', 2));
    }
    if (info.battery >= 0.0) {
        m_machine.power = info.batteryPercent();
        if (m_powerLabel) m_powerLabel->setText(QString::number(m_machine.power, 'f', 1) + "%");
    }
    if (!info.status.isEmpty()) {
        m_machine.alarminfo = info.statusText();
        if (m_alarmLabel) m_alarmLabel->setText(m_machine.alarminfo);
    }
}

void AgvInfoWidget::handleMqttMessage(const QString &topic, const QByteArray &payload)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
    if (err.error != QJsonParseError::NoError) {
        pushLog(QString("[%1] parse error: %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss"), QString::fromUtf8(payload)));
        return;
    }

    QJsonObject data = doc.object();

    auto fmt = [](const QJsonValue &v) -> double {
        if (v.isNull() || v.isUndefined()) return 0;
        double num = v.toDouble(-999999);
        if (num == -999999) return 0;
        return QString::number(num, 'f', 2).toDouble();
    };

    if (topic.contains("/status")) {
        const RobotInfo &curInfo = RobotInfoManager::instance().currentInfo();
        if (curInfo.status.isEmpty()) {
            m_machine.alarminfo = data.value("alarminfo").toString(data.value("alarmInfo").toString(m_machine.alarminfo.isEmpty() ? "无" : m_machine.alarminfo));
            if (m_alarmLabel) m_alarmLabel->setText(m_machine.alarminfo);
        }
        if (!curInfo.actual_vel.valid) {
            m_machine.linear_x = fmt(data.value("linear_x").isNull() ? data.value("linearX") : data.value("linear_x"));
            if (m_linearLabel) m_linearLabel->setText(QString::number(m_machine.linear_x, 'f', 2));
        }
        if (curInfo.battery < 0.0) {
            m_machine.power = fmt(data.value("power"));
            if (m_powerLabel) m_powerLabel->setText(QString::number(m_machine.power, 'f', 2) + "%");
        }
        if (!curInfo.hasPose) {
            m_machine.x = fmt(data.value("x"));
            m_machine.y = fmt(data.value("y"));
            m_machine.angle = fmt(data.value("angle"));
            if (m_xLabel) m_xLabel->setText(QString::number(m_machine.x, 'f', 2));
            if (m_yLabel) m_yLabel->setText(QString::number(m_machine.y, 'f', 2));
            if (m_angleLabel) m_angleLabel->setText(QString::number(m_machine.angle, 'f', 2));
        }
        return;
    }

    if (topic.contains("/log")) {
        if (m_selfCheckActive) return;  // 自检期间日志窗口只显示自检相关信息
        QString msg = data.value("message").toString(QString::fromUtf8(payload));
        QString time = QDateTime::currentDateTime().toString("HH:mm:ss");
        pushLog(QString("[%1] %2").arg(time, msg));
        return;
    }
}

void AgvInfoWidget::pushLog(const QString &line)
{
    m_logs.append(line);
    if (m_logs.size() > m_maxLogLines) {
        m_logs = m_logs.mid(m_logs.size() - m_maxLogLines);
    }
    m_logEdit->append(line);
    m_logCountLabel->setText(QString("日志行数: %1").arg(m_logs.size()));
    scrollToLast();
}

void AgvInfoWidget::scrollToLast()
{
    QScrollBar *sb = m_logEdit->verticalScrollBar();
    sb->setValue(sb->maximum());
}

void AgvInfoWidget::onStartCheck()
{
    m_checkBtn->setEnabled(false);
    m_logs.clear();
    m_logEdit->clear();
    // 自检期间暂停常规 MQTT /log 打印，日志窗口只显示自检相关信息
    m_selfCheckActive = true;
    pushLog("正在自检中...");

    ApiService::instance().startCheck([this](int code, const QJsonObject &data) {
        m_checkBtn->setEnabled(true);
        if (code != 200) {
            pushLog("[自检失败] 请求错误");
            m_selfCheckActive = false;
            return;
        }
        QJsonObject body = data.value("data").toObject(data);
        if (body.value("msg").toString() == "ok") {
            QJsonValue rawInfo = body.value("info");
            QStringList list;

            if (rawInfo.isArray()) {
                for (const auto &item : rawInfo.toArray())
                    list.append(item.toString().trimmed());
            } else if (rawInfo.isString()) {
                QStringList parts = rawInfo.toString().split(QRegularExpression("[,\\r\\n]+"));
                for (auto &s : parts) {
                    s = s.trimmed();
                    s.remove(QRegularExpression("^[\\-\\s]+"));
                    if (!s.isEmpty()) list.append(s);
                }
            } else if (rawInfo.isObject()) {
                for (auto it = rawInfo.toObject().begin(); it != rawInfo.toObject().end(); ++it)
                    list.append(QString("%1: %2").arg(it.key(), it.value().toString()));
            }

            if (list.isEmpty()) {
                pushLog("自检完成（后端未返回详细信息）");
                m_selfCheckActive = false;
                return;
            }

            m_messageList = list;
            m_messageList.append("--------- 自检完成 ---------");
            m_currentIndex = 0;
            m_isLoop = true;
            m_checkTimer->start(900);
        } else {
            QString err = body.value("error_info").toString("自检失败");
            pushLog(QString("[自检失败] %1").arg(err));
            m_selfCheckActive = false;
        }
    });
}

void AgvInfoWidget::onStartTerminal()
{
    m_startBtn->setEnabled(false);
    m_logs.clear();
    m_logEdit->clear();
    pushLog("启动终端中...");

    ApiService::instance().startTerminal([this](int code, const QJsonObject &data) {
        m_startBtn->setEnabled(true);
        if (code != 200) {
            pushLog("[启动终端异常] 请求错误");
            return;
        }
        QJsonObject body = data.value("data").toObject(data);
        if (body.value("msg").toString() == "ok") {
            pushLog("启动终端成功");
            Toast::success(this, "启动终端成功");
        } else {
            QString err = body.value("error_info").toString("启动失败");
            pushLog(QString("[启动终端失败] %1").arg(err));
        }
    });
}

void AgvInfoWidget::onStopTerminal()
{
    m_stopBtn->setEnabled(false);
    ApiService::instance().stopTerminal([this](int code, const QJsonObject &data) {
        m_stopBtn->setEnabled(true);
        if (code != 200) {
            pushLog("[停止终端异常] 请求错误");
            return;
        }
        QJsonObject body = data.value("data").toObject(data);
        if (body.value("msg").toString() == "ok") {
            pushLog("终端已停止");
            Toast::success(this, "终端已停止");
        } else {
            QString err = body.value("error_info").toString("停止失败");
            pushLog(QString("[停止终端失败] %1").arg(err));
        }
    });
}

void AgvInfoWidget::onResetLift()
{
    auto &mqtt = MqttService::instance();
    if (!mqtt.isConnected()) {
        pushLog("[置零失败] MQTT 未连接");
        return;
    }
    QString agvId = AppConfig::instance().agvId();
    QString topic = QString("/rob/%1/set_lift").arg(agvId);
    mqtt.publish(topic, "ResetLift");
    pushLog("[顶升] 已发送置零命令");
    Toast::success(this, "顶升高度置零成功");
}

void AgvInfoWidget::onMqttConnected()
{
    m_mqttConnected = true;
    m_mqttStatusLabel->setText("MQTT: 在线");
    m_mqttStatusLabel->setStyleSheet("color: #00ff66; font-weight: bold;");
    initMqtt();
}

void AgvInfoWidget::onMqttDisconnected()
{
    m_mqttConnected = false;
    m_mqttStatusLabel->setText("MQTT: 离线");
    m_mqttStatusLabel->setStyleSheet("color: #999;");
}
