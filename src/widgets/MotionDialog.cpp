#include "MotionDialog.h"
#include "core/AppConfig.h"
#include "core/MqttService.h"
#include "core/ApiService.h"
#include "core/RobotInfoManager.h"
#include "core/Toast.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTabWidget>
#include <QDoubleSpinBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QMouseEvent>
#include <QPainter>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

MotionDialog::MotionDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setFixedSize(480, 310);

    setupUi();

    connect(&RobotInfoManager::instance(), &RobotInfoManager::currentRobotInfoUpdated,
            this, &MotionDialog::onCurrentRobotInfoUpdated);

    m_motionWatchdog.setSingleShot(true);
    connect(&m_motionWatchdog, &QTimer::timeout, this, &MotionDialog::onMotionTimeout);

    setCanceledState();
}

MotionDialog::~MotionDialog()
{
    m_motionWatchdog.stop();
}

void MotionDialog::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && event->pos().y() <= 38) {
        m_dragging = true;
        m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    } else {
        QDialog::mousePressEvent(event);
    }
}

void MotionDialog::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
    } else {
        QDialog::mouseMoveEvent(event);
    }
}

void MotionDialog::mouseReleaseEvent(QMouseEvent *event)
{
    m_dragging = false;
    QDialog::mouseReleaseEvent(event);
}

void MotionDialog::setupUi()
{
    setStyleSheet(
        "QDialog {"
        "  background-color: #24272c;"
        "  border: 1px solid #3d434d;"
        "  border-radius: 4px;"
        "}"
        "QLabel {"
        "  color: #c5cbd5;"
        "  font-size: 13px;"
        "}"
        "QTabWidget::pane {"
        "  border: none;"
        "  background: transparent;"
        "}"
        "QTabBar::tab {"
        "  background: #2a2e35;"
        "  color: #9aa1ad;"
        "  border: 1px solid #3a404b;"
        "  border-bottom: none;"
        "  padding: 5px 16px;"
        "  margin-right: 2px;"
        "  border-top-left-radius: 3px;"
        "  border-top-right-radius: 3px;"
        "  font-size: 13px;"
        "}"
        "QTabBar::tab:selected {"
        "  background: #24272c;"
        "  color: #ffffff;"
        "  border-color: #4b5360;"
        "}"
        "QDoubleSpinBox {"
        "  background-color: #1a1d21;"
        "  color: #e2e8f0;"
        "  border: 1px solid #3d434d;"
        "  border-radius: 3px;"
        "  padding: 3px 6px;"
        "  font-size: 13px;"
        "}"
        "QDoubleSpinBox:focus {"
        "  border: 1px solid #3b82f6;"
        "}"
        "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {"
        "  background-color: #2a2e35;"
        "  border-left: 1px solid #3d434d;"
        "  width: 16px;"
        "}"
        "QRadioButton {"
        "  color: #c5cbd5;"
        "  font-size: 13px;"
        "  spacing: 6px;"
        "}"
        "QRadioButton::indicator {"
        "  width: 14px;"
        "  height: 14px;"
        "}"
        "QRadioButton::indicator:unchecked {"
        "  border: 2px solid #64748b;"
        "  border-radius: 7px;"
        "  background: transparent;"
        "}"
        "QRadioButton::indicator:checked {"
        "  border: 2px solid #0ea5e9;"
        "  border-radius: 7px;"
        "  background: #0ea5e9;"
        "}"
        "QPushButton {"
        "  background-color: #2b2f36;"
        "  color: #e2e8f0;"
        "  border: 1px solid #454d59;"
        "  border-radius: 3px;"
        "  padding: 5px 14px;"
        "  font-size: 13px;"
        "}"
        "QPushButton:hover:!disabled {"
        "  background-color: #373d47;"
        "  border-color: #586373;"
        "}"
        "QPushButton:pressed:!disabled {"
        "  background-color: #1f2328;"
        "}"
        "QPushButton:disabled {"
        "  background-color: #22252a;"
        "  color: #555c68;"
        "  border-color: #323740;"
        "}"
    );

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(1, 1, 1, 1);
    rootLayout->setSpacing(0);

    // ── 1. 标题栏 (自定义暗黑拖拽栏) ──
    auto *titleBar = new QWidget(this);
    titleBar->setFixedHeight(36);
    titleBar->setStyleSheet("background-color: #1e2125; border-top-left-radius: 4px; border-top-right-radius: 4px;");

    auto *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(10, 0, 10, 0);
    titleLayout->setSpacing(8);

    // 标题左侧菱形双向箭头图标
    auto *iconLabel = new QLabel(this);
    QPixmap iconPix(18, 18);
    iconPix.fill(Qt::transparent);
    {
        QPainter p(&iconPix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(14, 165, 233));
        QPolygonF leftTri;
        leftTri << QPointF(9, 9) << QPointF(2, 4) << QPointF(2, 14);
        p.drawPolygon(leftTri);
        QPolygonF rightTri;
        rightTri << QPointF(9, 9) << QPointF(16, 4) << QPointF(16, 14);
        p.drawPolygon(rightTri);
    }
    iconLabel->setPixmap(iconPix);
    iconLabel->setFixedSize(18, 18);
    titleLayout->addWidget(iconLabel);

    auto *titleLabel = new QLabel("平动转动", this);
    titleLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: #ffffff;");
    titleLayout->addWidget(titleLabel);

    titleLayout->addStretch();

    auto *closeBtn = new QPushButton("✕", this);
    closeBtn->setFixedSize(24, 24);
    closeBtn->setStyleSheet(
        "QPushButton {"
        "  background: transparent;"
        "  color: #9aa1ad;"
        "  border: none;"
        "  font-size: 13px;"
        "  padding: 0;"
        "}"
        "QPushButton:hover {"
        "  background: #dc2626;"
        "  color: #ffffff;"
        "  border-radius: 2px;"
        "}"
    );
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    titleLayout->addWidget(closeBtn);

    rootLayout->addWidget(titleBar);

    // ── 2. 主体内容 (TabBar 与页面) ──
    auto *contentWidget = new QWidget(this);
    auto *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(12, 10, 12, 10);
    contentLayout->setSpacing(10);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->addTab(createTranslationTab(), "平动");
    m_tabWidget->addTab(createRotationTab(), "转动");
    contentLayout->addWidget(m_tabWidget);

    // ── 3. 底部控制栏 ──
    auto *bottomLayout = new QHBoxLayout;
    bottomLayout->setContentsMargins(4, 2, 4, 2);
    bottomLayout->setSpacing(10);

    m_statusLabel = new QLabel("取消导航", this);
    m_statusLabel->setStyleSheet("color: #94a3b8; font-weight: bold; font-size: 14px;");
    bottomLayout->addWidget(m_statusLabel);

    bottomLayout->addStretch();

    m_cancelBtn = new QPushButton("取消导航", this);
    m_cancelBtn->setEnabled(false);
    connect(m_cancelBtn, &QPushButton::clicked, this, &MotionDialog::onCancelNavClicked);
    bottomLayout->addWidget(m_cancelBtn);

    m_pauseBtn = new QPushButton("暂无", this);
    m_pauseBtn->setEnabled(false);
    connect(m_pauseBtn, &QPushButton::clicked, this, &MotionDialog::onPauseNavClicked);
    bottomLayout->addWidget(m_pauseBtn);

    m_sendBtn = new QPushButton("发送", this);
    connect(m_sendBtn, &QPushButton::clicked, this, &MotionDialog::onSendClicked);
    bottomLayout->addWidget(m_sendBtn);

    contentLayout->addLayout(bottomLayout);
    rootLayout->addWidget(contentWidget);
}

QWidget* MotionDialog::createTranslationTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(0, 4, 0, 0);
    layout->setSpacing(6);

    // 外包细边框面板
    auto *panel = new QFrame(this);
    panel->setStyleSheet("QFrame { border: 1px solid #383e47; border-radius: 4px; background-color: #212428; }");
    auto *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(14, 10, 14, 12);
    panelLayout->setSpacing(8);

    auto *descLabel = new QLabel("机器人平动，以固定速度直线运动固定距离", panel);
    descLabel->setStyleSheet("border: none; color: #a6b0be; font-size: 12px;");
    panelLayout->addWidget(descLabel);

    auto *formLayout = new QGridLayout;
    formLayout->setHorizontalSpacing(10);
    formLayout->setVerticalSpacing(8);

    // 1. 设置距离
    auto *distLbl = new QLabel("设置距离：", panel);
    distLbl->setStyleSheet("border: none; color: #c5cbd5;");
    m_transDistSpin = new QDoubleSpinBox(panel);
    m_transDistSpin->setRange(-100.0, 100.0);
    m_transDistSpin->setDecimals(3);
    m_transDistSpin->setSingleStep(0.1);
    m_transDistSpin->setValue(0.000);
    m_transDistSpin->setSuffix(" m");
    formLayout->addWidget(distLbl, 0, 0);
    formLayout->addWidget(m_transDistSpin, 0, 1);

    // 2. 设置 vx 速度
    auto *vxLbl = new QLabel("设置 vx 速度：", panel);
    vxLbl->setStyleSheet("border: none; color: #c5cbd5;");
    m_transVxSpin = new QDoubleSpinBox(panel);
    m_transVxSpin->setRange(-5.0, 5.0);
    m_transVxSpin->setDecimals(3);
    m_transVxSpin->setSingleStep(0.05);
    m_transVxSpin->setValue(0.000);
    m_transVxSpin->setSuffix(" m/s");
    formLayout->addWidget(vxLbl, 1, 0);
    formLayout->addWidget(m_transVxSpin, 1, 1);

    // 3. 设置 vy 速度
    auto *vyLbl = new QLabel("设置 vy 速度：", panel);
    vyLbl->setStyleSheet("border: none; color: #c5cbd5;");
    m_transVySpin = new QDoubleSpinBox(panel);
    m_transVySpin->setRange(-5.0, 5.0);
    m_transVySpin->setDecimals(3);
    m_transVySpin->setSingleStep(0.05);
    m_transVySpin->setValue(0.000);
    m_transVySpin->setSuffix(" m/s");
    formLayout->addWidget(vyLbl, 2, 0);
    formLayout->addWidget(m_transVySpin, 2, 1);

    // 4. 模式
    auto *modeLbl = new QLabel("模式：", panel);
    modeLbl->setStyleSheet("border: none; color: #c5cbd5;");
    auto *radioWidget = new QWidget(panel);
    radioWidget->setStyleSheet("border: none; background: transparent;");
    auto *radioLayout = new QHBoxLayout(radioWidget);
    radioLayout->setContentsMargins(0, 0, 0, 0);
    radioLayout->setSpacing(24);

    m_odomModeRadio = new QRadioButton("里程模式", radioWidget);
    m_locModeRadio  = new QRadioButton("定位模式", radioWidget);
    m_odomModeRadio->setChecked(true);

    auto *modeGroup = new QButtonGroup(this);
    modeGroup->addButton(m_odomModeRadio);
    modeGroup->addButton(m_locModeRadio);

    radioLayout->addWidget(m_odomModeRadio);
    radioLayout->addWidget(m_locModeRadio);
    radioLayout->addStretch();

    formLayout->addWidget(modeLbl, 3, 0);
    formLayout->addWidget(radioWidget, 3, 1);

    formLayout->setColumnStretch(1, 1);
    panelLayout->addLayout(formLayout);
    layout->addWidget(panel);

    return tab;
}

QWidget* MotionDialog::createRotationTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(0, 4, 0, 0);
    layout->setSpacing(6);

    auto *panel = new QFrame(this);
    panel->setStyleSheet("QFrame { border: 1px solid #383e47; border-radius: 4px; background-color: #212428; }");
    auto *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(14, 10, 14, 12);
    panelLayout->setSpacing(8);

    auto *descLabel = new QLabel("机器人转动，以固定角速度旋转固定角度", panel);
    descLabel->setStyleSheet("border: none; color: #a6b0be; font-size: 12px;");
    panelLayout->addWidget(descLabel);

    auto *formLayout = new QGridLayout;
    formLayout->setHorizontalSpacing(10);
    formLayout->setVerticalSpacing(10);

    // 1. 设置旋转角度
    auto *angleLbl = new QLabel("设置旋转角度：", panel);
    angleLbl->setStyleSheet("border: none; color: #c5cbd5;");
    m_rotAngleSpin = new QDoubleSpinBox(panel);
    m_rotAngleSpin->setRange(-360.0, 360.0);
    m_rotAngleSpin->setDecimals(3);
    m_rotAngleSpin->setSingleStep(5.0);
    m_rotAngleSpin->setValue(0.000);
    m_rotAngleSpin->setSuffix(" °");
    formLayout->addWidget(angleLbl, 0, 0);
    formLayout->addWidget(m_rotAngleSpin, 0, 1);

    // 2. 设置角速度
    auto *speedLbl = new QLabel("设置角速度：", panel);
    speedLbl->setStyleSheet("border: none; color: #c5cbd5;");
    m_rotSpeedSpin = new QDoubleSpinBox(panel);
    m_rotSpeedSpin->setRange(-180.0, 180.0);
    m_rotSpeedSpin->setDecimals(3);
    m_rotSpeedSpin->setSingleStep(2.0);
    m_rotSpeedSpin->setValue(0.000);
    m_rotSpeedSpin->setSuffix(" °/s");
    formLayout->addWidget(speedLbl, 1, 0);
    formLayout->addWidget(m_rotSpeedSpin, 1, 1);

    formLayout->setColumnStretch(1, 1);
    panelLayout->addLayout(formLayout);
    layout->addWidget(panel);

    return tab;
}

QString MotionDialog::currentCarId() const
{
    QString carId = AppConfig::instance().currentDevice().carId;
    if (carId.isEmpty()) carId = AppConfig::instance().agvId();
    if (carId.isEmpty()) carId = "1";
    return carId;
}

void MotionDialog::setRunningState(bool running, bool paused)
{
    m_isRunning = running;
    m_isPaused = paused;

    if (running) {
        if (paused) {
            m_statusLabel->setText("已暂停");
            m_statusLabel->setStyleSheet("color: #f59e0b; font-weight: bold; font-size: 14px;");
            m_cancelBtn->setEnabled(true);
            m_pauseBtn->setText("继续导航");
            m_pauseBtn->setEnabled(true);
        } else {
            m_statusLabel->setText("运行中...");
            m_statusLabel->setStyleSheet("color: #0ea5e9; font-weight: bold; font-size: 14px;");
            m_cancelBtn->setEnabled(true);
            m_pauseBtn->setText("暂停导航");
            m_pauseBtn->setEnabled(true);
        }
    } else {
        m_cancelBtn->setEnabled(false);
        m_pauseBtn->setText("暂无");
        m_pauseBtn->setEnabled(false);
    }
}

void MotionDialog::setCompletedState()
{
    m_isRunning = false;
    m_isPaused = false;
    m_motionWatchdog.stop();

    m_statusLabel->setText("导航已完成");
    m_statusLabel->setStyleSheet("color: #22c55e; font-weight: bold; font-size: 14px;");

    m_cancelBtn->setEnabled(false);
    m_pauseBtn->setText("暂无");
    m_pauseBtn->setEnabled(false);
}

void MotionDialog::setCanceledState()
{
    m_isRunning = false;
    m_isPaused = false;
    m_motionWatchdog.stop();

    m_statusLabel->setText("取消导航");
    m_statusLabel->setStyleSheet("color: #94a3b8; font-weight: bold; font-size: 14px;");

    m_cancelBtn->setEnabled(false);
    m_pauseBtn->setText("暂无");
    m_pauseBtn->setEnabled(false);
}

void MotionDialog::onSendClicked()
{
    QString carId = currentCarId();
    const RobotInfo &info = RobotInfoManager::instance().currentInfo();
    m_startX = info.car_x;
    m_startY = info.car_y;
    m_startAngle = info.car_yaw * 180.0 / M_PI;
    m_hasStartPose = info.hasPose;

    int currentTab = m_tabWidget->currentIndex();
    QJsonObject mqttPayload;
    double durationSec = 3.0;

    if (currentTab == 0) {
        // 平动: 按照规范调用控制器 set_action_control 接口 (action_type: 6)
        m_motionType = 1;
        double dist = m_transDistSpin->value();
        double vx   = m_transVxSpin->value();
        double vy   = m_transVySpin->value();
        QString modeStr = m_odomModeRadio->isChecked() ? "odom" : "location";

        m_targetDistance = std::abs(dist);

        // 调用 8061 控制器平动动作控制接口 (action_type: 6)
        int actionId = static_cast<int>(QDateTime::currentMSecsSinceEpoch() % 1000000 + 1);
        ApiService::instance().setLinearMotion(vx, vy, dist, actionId, [this](int code, const QJsonObject &resp) {
            qDebug() << "[MotionDialog] set_action_control (linear) code:" << code << resp;
        });

        mqttPayload["command"]     = "motion";
        mqttPayload["action_type"] = "1";
        mqttPayload["action_desc"] = "平动";
        mqttPayload["type"]        = "translate";
        mqttPayload["mode"]        = modeStr;
        mqttPayload["distance"]    = dist;
        mqttPayload["vx"]          = vx;
        mqttPayload["vy"]          = vy;

        double maxV = std::max(std::abs(vx), std::abs(vy));
        if (maxV > 1e-3) {
            durationSec = (std::abs(dist) / maxV) + 1.5;
        } else {
            durationSec = 4.0;
        }
    } else {
        // 转动: 按照规范调用控制器 set_action_control 接口 (action_type: 5)
        m_motionType = 2;
        double angle = m_rotAngleSpin->value();
        double speed = m_rotSpeedSpin->value();

        m_targetAngle = std::abs(angle);

        // 旋转角度转弧度 (rad)
        double angleRad = angle * M_PI / 180.0;
        int actionId = static_cast<int>(QDateTime::currentMSecsSinceEpoch() % 1000000 + 1);
        ApiService::instance().setRotateMotion(angleRad, speed, actionId, [this](int code, const QJsonObject &resp) {
            qDebug() << "[MotionDialog] set_action_control (rotate) code:" << code << resp;
        });

        mqttPayload["command"]       = "motion";
        mqttPayload["action_type"]   = "2";
        mqttPayload["action_desc"]   = "转动";
        mqttPayload["type"]          = "rotate";
        mqttPayload["angle"]         = angle;
        mqttPayload["speed"]         = speed;
        mqttPayload["angular_speed"] = speed;

        if (std::abs(speed) > 1e-3) {
            durationSec = (std::abs(angle) / std::abs(speed)) + 1.5;
        } else {
            durationSec = 4.0;
        }
    }

    // 1. 发布 MQTT 指令至 /rob/{carId}/motion 与 /rob/{carId}/navigation
    QByteArray payloadBytes = QJsonDocument(mqttPayload).toJson(QJsonDocument::Compact);
    MqttService::instance().publish(QString("/rob/%1/motion").arg(carId), payloadBytes);
    MqttService::instance().publish(QString("/rob/%1/navigation").arg(carId), payloadBytes);

    // 2. 切换至运行中状态
    setRunningState(true, false);

    // 3. 启动看门狗超时定时器
    int watchdogMs = static_cast<int>(std::clamp(durationSec, 2.0, 120.0) * 1000.0);
    m_motionWatchdog.start(watchdogMs);

    Toast::success(this, (m_motionType == 1) ? "平动指令已下发" : "转动指令已下发");
}

void MotionDialog::onCancelNavClicked()
{
    QString carId = currentCarId();
    MqttService::instance().publish(QString("/rob/%1/cancel").arg(carId), "1");
    ApiService::instance().suspendWork([](int, const QJsonObject &) {});
    ApiService::instance().stopRobotTask(false, [](int, const QJsonObject &) {});

    setCanceledState();
    Toast::info(this, "已取消导航");
}

void MotionDialog::onPauseNavClicked()
{
    QString carId = currentCarId();

    if (m_isPaused) {
        // 继续导航 (同时下发 MQTT、8061 控制器通道)
        MqttService::instance().publish(QString("/rob/%1/navigationContinue").arg(carId), "1");
        ApiService::instance().continueWork([](int, const QJsonObject &) {});
        ApiService::instance().continueRobot([](int, const QJsonObject &) {});
        setRunningState(true, false);
        Toast::success(this, "已继续导航");
    } else {
        // 暂停导航 (同时下发 MQTT、8061 控制器通道)
        MqttService::instance().publish(QString("/rob/%1/cancel").arg(carId), "1");
        ApiService::instance().suspendWork([](int, const QJsonObject &) {});
        ApiService::instance().suspendRobot([](int, const QJsonObject &) {});
        setRunningState(true, true);
        Toast::info(this, "导航已暂停");
    }
}

void MotionDialog::onCurrentRobotInfoUpdated(const RobotInfo &info)
{
    if (!m_isRunning || m_isPaused) return;

    // 判定依据 1: 后端显式上报到达状态 (status == "4" 停止导航，作业中 / status == "2" 在线未执行任务)
    if (info.status == "4") {
        setCompletedState();
        return;
    }

    // 判定依据 2: 位移或旋转角度已达成且速度归零
    if (m_hasStartPose && info.hasPose) {
        if (m_motionType == 1) { // 平动
            double dx = info.car_x - m_startX;
            double dy = info.car_y - m_startY;
            double moved = std::hypot(dx, dy);
            bool speedZero = (std::abs(info.actual_vel.vx) < 0.02 && std::abs(info.actual_vel.vy) < 0.02);

            if (moved >= m_targetDistance * 0.96 && speedZero) {
                setCompletedState();
                return;
            }
        } else if (m_motionType == 2) { // 转动
            double currentDeg = info.car_yaw * 180.0 / M_PI;
            double dAngle = std::abs(currentDeg - m_startAngle);
            while (dAngle > 180.0) dAngle = std::abs(360.0 - dAngle);
            bool speedZero = (std::abs(info.actual_vel.wz) < 0.03);

            if (dAngle >= m_targetAngle * 0.95 && speedZero) {
                setCompletedState();
                return;
            }
        }
    }
}

void MotionDialog::onMotionTimeout()
{
    if (m_isRunning) {
        setCompletedState();
    }
}
