#include "MotionDialog.h"
#include "core/ApiService.h"
#include "core/RobotInfoManager.h"
#include "core/MotionConfigManager.h"
#include "core/TaskChainManager.h"
#include "core/TeleopController.h"
#include "core/Toast.h"

#include <QPointer>
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
    connect(&MotionConfigManager::instance(), &MotionConfigManager::configChanged,
            this, &MotionDialog::refreshSpeedLimits);

    m_motionWatchdog.setSingleShot(true);
    connect(&m_motionWatchdog, &QTimer::timeout, this, &MotionDialog::onMotionTimeout);

    refreshSpeedLimits();
    connect(&TeleopController::instance(), &TeleopController::activeChanged,
            this, [this](bool) { onCurrentRobotInfoUpdated(RobotInfoManager::instance().currentInfo()); });
    connect(&TeleopController::instance(), &TeleopController::userPreemptedChanged,
            this, [this](bool) { onCurrentRobotInfoUpdated(RobotInfoManager::instance().currentInfo()); });
    onCurrentRobotInfoUpdated(RobotInfoManager::instance().currentInfo());
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

void MotionDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    refreshSpeedLimits();
    onCurrentRobotInfoUpdated(RobotInfoManager::instance().currentInfo());
}

void MotionDialog::refreshSpeedLimits()
{
    const double maxSpd = MotionConfigManager::instance().maxSpeed();
    if (m_transVxSpin) {
        m_transVxSpin->setRange(-maxSpd, maxSpd);
        if (m_transVxSpin->value() > maxSpd) m_transVxSpin->setValue(maxSpd);
        if (m_transVxSpin->value() < -maxSpd) m_transVxSpin->setValue(-maxSpd);
    }
    if (m_transVySpin) {
        m_transVySpin->setRange(-maxSpd, maxSpd);
        if (m_transVySpin->value() > maxSpd) m_transVySpin->setValue(maxSpd);
        if (m_transVySpin->value() < -maxSpd) m_transVySpin->setValue(-maxSpd);
    }

    const double maxAngular = MotionConfigManager::instance().maxAngularSpeedDeg();
    if (m_rotSpeedSpin) {
        m_rotSpeedSpin->setRange(-maxAngular, maxAngular);
        if (m_rotSpeedSpin->value() > maxAngular) m_rotSpeedSpin->setValue(maxAngular);
        if (m_rotSpeedSpin->value() < -maxAngular) m_rotSpeedSpin->setValue(-maxAngular);
    }
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

    const RobotInfo &initInfo = RobotInfoManager::instance().currentInfo();
    m_statusLabel = new QLabel(initInfo.status == "1" ? "导航中" : "就绪", this);
    m_statusLabel->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 14px;")
                                     .arg(initInfo.status == "1" ? "#0ea5e9" : "#22c55e"));
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
    m_transDistSpin->setRange(0.0, 100.0);
    m_transDistSpin->setDecimals(3);
    m_transDistSpin->setSingleStep(0.1);
    m_transDistSpin->setValue(0.000);
    auto *distUnitLbl = new QLabel("m", panel);
    distUnitLbl->setStyleSheet("border: none; color: #a6b0be; font-size: 12px;");
    formLayout->addWidget(distLbl, 0, 0);
    formLayout->addWidget(m_transDistSpin, 0, 1);
    formLayout->addWidget(distUnitLbl, 0, 2);

    // 2. 设置 vx 速度
    auto *vxLbl = new QLabel("设置 vx 速度：", panel);
    vxLbl->setStyleSheet("border: none; color: #c5cbd5;");
    const double maxLinear = MotionConfigManager::instance().maxSpeed();
    m_transVxSpin = new QDoubleSpinBox(panel);
    m_transVxSpin->setRange(-maxLinear, maxLinear);
    m_transVxSpin->setDecimals(3);
    m_transVxSpin->setSingleStep(0.05);
    m_transVxSpin->setValue(0.000);
    auto *vxUnitLbl = new QLabel("m/s", panel);
    vxUnitLbl->setStyleSheet("border: none; color: #a6b0be; font-size: 12px;");
    formLayout->addWidget(vxLbl, 1, 0);
    formLayout->addWidget(m_transVxSpin, 1, 1);
    formLayout->addWidget(vxUnitLbl, 1, 2);

    // 3. 设置 vy 速度
    auto *vyLbl = new QLabel("设置 vy 速度：", panel);
    vyLbl->setStyleSheet("border: none; color: #c5cbd5;");
    m_transVySpin = new QDoubleSpinBox(panel);
    m_transVySpin->setRange(-maxLinear, maxLinear);
    m_transVySpin->setDecimals(3);
    m_transVySpin->setSingleStep(0.05);
    m_transVySpin->setValue(0.000);
    auto *vyUnitLbl = new QLabel("m/s", panel);
    vyUnitLbl->setStyleSheet("border: none; color: #a6b0be; font-size: 12px;");
    formLayout->addWidget(vyLbl, 2, 0);
    formLayout->addWidget(m_transVySpin, 2, 1);
    formLayout->addWidget(vyUnitLbl, 2, 2);

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
    formLayout->addWidget(radioWidget, 3, 1, 1, 2);

    formLayout->setColumnStretch(0, 0);
    formLayout->setColumnStretch(1, 1);
    formLayout->setColumnStretch(2, 0);
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
    auto *angleUnitLbl = new QLabel("°", panel);
    angleUnitLbl->setStyleSheet("border: none; color: #a6b0be; font-size: 12px;");
    formLayout->addWidget(angleLbl, 0, 0);
    formLayout->addWidget(m_rotAngleSpin, 0, 1);
    formLayout->addWidget(angleUnitLbl, 0, 2);

    // 2. 设置角速度
    auto *speedLbl = new QLabel("设置角速度：", panel);
    speedLbl->setStyleSheet("border: none; color: #c5cbd5;");
    const double maxAngular = MotionConfigManager::instance().maxAngularSpeedDeg();
    m_rotSpeedSpin = new QDoubleSpinBox(panel);
    m_rotSpeedSpin->setRange(-maxAngular, maxAngular);
    m_rotSpeedSpin->setDecimals(3);
    m_rotSpeedSpin->setSingleStep(2.0);
    m_rotSpeedSpin->setValue(0.000);
    auto *speedUnitLbl = new QLabel("°/s", panel);
    speedUnitLbl->setStyleSheet("border: none; color: #a6b0be; font-size: 12px;");
    formLayout->addWidget(speedLbl, 1, 0);
    formLayout->addWidget(m_rotSpeedSpin, 1, 1);
    formLayout->addWidget(speedUnitLbl, 1, 2);

    formLayout->setColumnStretch(0, 0);
    formLayout->setColumnStretch(1, 1);
    formLayout->setColumnStretch(2, 0);
    panelLayout->addLayout(formLayout);
    layout->addWidget(panel);

    return tab;
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
            m_statusLabel->setText("导航中");
            m_statusLabel->setStyleSheet("color: #0ea5e9; font-weight: bold; font-size: 14px;");
            m_cancelBtn->setEnabled(true);
            m_pauseBtn->setText("暂停导航");
            m_pauseBtn->setEnabled(true);
        }
    } else {
        onCurrentRobotInfoUpdated(RobotInfoManager::instance().currentInfo());
    }
}

void MotionDialog::setCompletedState()
{
    m_isRunning = false;
    m_isPaused = false;
    m_motionWatchdog.stop();

    if (m_sendBtn) m_sendBtn->setEnabled(true);
    onCurrentRobotInfoUpdated(RobotInfoManager::instance().currentInfo());
}

void MotionDialog::setCanceledState()
{
    m_isRunning = false;
    m_isPaused = false;
    m_motionWatchdog.stop();

    if (m_sendBtn) m_sendBtn->setEnabled(true);
    onCurrentRobotInfoUpdated(RobotInfoManager::instance().currentInfo());
}

void MotionDialog::onSendClicked()
{
    // 注意通过 MQTT 的状态判断，执行任务链期间不可执行移动
    if (TaskChainManager::instance().isWorking()) {
        Toast::error(this, "当前正在执行任务链任务，不可下发移动指令！");
        return;
    }

    const RobotInfo &info = RobotInfoManager::instance().currentInfo();
    m_startX = info.car_x;
    m_startY = info.car_y;
    m_startAngle = info.car_yaw * 180.0 / M_PI;
    m_hasStartPose = info.hasPose;

    // 发送前前置禁用发送按钮，避免重复点击，状态更新为发送中
    m_sendBtn->setEnabled(false);
    m_statusLabel->setText("正在下发移动指令...");
    m_statusLabel->setStyleSheet("color: #3b82f6; font-weight: bold; font-size: 14px;");

    const int currentTab = m_tabWidget->currentIndex();
    QPointer<MotionDialog> self = this;

    if (currentTab == 0) {
        // 平动: 按照规范统一调用 POST 8048 /robot/navigation/move (actionType=6)
        // Query 参数: actionType, vx, vy, distance (平动不给转动参数赋值)
        self->m_motionType = 1;
        const double dist = m_transDistSpin->value();
        const double rawVx = m_transVxSpin->value();
        const double rawVy = m_transVySpin->value();
        double vx = MotionConfigManager::instance().clampSignedLinearSpeed(rawVx);
        double vy = MotionConfigManager::instance().clampSignedLinearSpeed(rawVy);
        self->m_targetDistance = std::abs(dist);

        double maxV = std::max(std::abs(vx), std::abs(vy));
        double durationSec = (maxV > 1e-3) ? ((std::abs(dist) / maxV) + 1.5) : 4.0;

        ApiService::instance().robotNavigationMoveLinear(vx, vy, dist, [self, durationSec](int code, const QJsonObject &resp) {
            if (!self) return;
            qDebug() << "[MotionDialog] 平动指令响应:" << code << resp;

            const QString msg = resp.value("msg").toString();
            const bool ok = (code == 200 || msg.compare("ok", Qt::CaseInsensitive) == 0 || resp.value("code").toInt() == 200);
            if (!ok) {
                qWarning() << "[MotionDialog] 平动指令下发失败, code:" << code << resp;
                self->m_sendBtn->setEnabled(true);
                self->m_statusLabel->setText("就绪");
                self->m_statusLabel->setStyleSheet("color: #94a3b8; font-weight: bold; font-size: 14px;");
                QString errMsg = msg.isEmpty() ? QString("错误码 %1").arg(code) : msg;
                Toast::error(self.data(), QString("平动指令失败(%1): %2").arg(code).arg(errMsg));
                return;
            }

            self->setRunningState(true, false);
            int watchdogMs = static_cast<int>(std::clamp(durationSec, 2.0, 120.0) * 1000.0);
            self->m_motionWatchdog.start(watchdogMs);
            Toast::success(self.data(), "平动指令已下发");
        });
    } else {
        // 转动: 按照规范统一调用 POST 8048 /robot/navigation/move (actionType=12)
        // Query 参数: actionType, rotateAngle, rotateSpeed (转动不给平动参数赋值)
        self->m_motionType = 2;
        const double angle = m_rotAngleSpin->value();
        const double rawSpeed = m_rotSpeedSpin->value();
        double speed = MotionConfigManager::instance().clampSignedAngularSpeedDeg(rawSpeed);
        self->m_targetAngle = std::abs(angle);

        double durationSec = (std::abs(speed) > 1e-3) ? ((std::abs(angle) / std::abs(speed)) + 1.5) : 4.0;

        ApiService::instance().robotNavigationMoveRotate(angle, speed, [self, durationSec](int code, const QJsonObject &resp) {
            if (!self) return;
            qDebug() << "[MotionDialog] 转动指令响应:" << code << resp;

            const QString msg = resp.value("msg").toString();
            const bool ok = (code == 200 || msg.compare("ok", Qt::CaseInsensitive) == 0 || resp.value("code").toInt() == 200);
            if (!ok) {
                qWarning() << "[MotionDialog] 转动指令下发失败, code:" << code << resp;
                self->m_sendBtn->setEnabled(true);
                self->m_statusLabel->setText("就绪");
                self->m_statusLabel->setStyleSheet("color: #94a3b8; font-weight: bold; font-size: 14px;");
                QString errMsg = msg.isEmpty() ? QString("错误码 %1").arg(code) : msg;
                Toast::error(self.data(), QString("转动指令失败(%1): %2").arg(code).arg(errMsg));
                return;
            }

            self->setRunningState(true, false);
            int watchdogMs = static_cast<int>(std::clamp(durationSec, 2.0, 120.0) * 1000.0);
            self->m_motionWatchdog.start(watchdogMs);
            Toast::success(self.data(), "转动指令已下发");
        });
    }
}

void MotionDialog::onCancelNavClicked()
{
    // 统一使用 /robot/navigation/control?command=stop navigation 结束/取消导航
    ApiService::instance().controlNavigation(QStringLiteral("stop navigation"), [](int, const QJsonObject &) {});

    setCanceledState();
    Toast::info(this, "已取消导航");
}

void MotionDialog::onPauseNavClicked()
{
    const RobotInfo &info = RobotInfoManager::instance().currentInfo();
    const bool isCurrentlyPaused = (m_isPaused || info.status == "2" || info.status == "5");

    if (isCurrentlyPaused) {
        // 恢复导航统一使用 /robot/navigation/control?command=continue navigation
        ApiService::instance().controlNavigation(QStringLiteral("continue navigation"), [](int, const QJsonObject &) {});
        setRunningState(true, false);
        Toast::success(this, "已继续导航");
    } else {
        // 暂停导航统一使用 /robot/navigation/control?command=pause navigation
        ApiService::instance().controlNavigation(QStringLiteral("pause navigation"), [](int, const QJsonObject &) {});
        setRunningState(true, true);
        Toast::info(this, "导航已暂停");
    }
}

void MotionDialog::onCurrentRobotInfoUpdated(const RobotInfo &info)
{
    // 1. 移动弹窗底部的导航状态文字通过 /rob/navigate/status 获取并更新
    const QString st = info.status;
    const bool isPaused = (st == "2" || st == "5" || (st != "0" && st != "1" && m_isPaused));

    if (m_statusLabel) {
        if (st == "2" || st == "5") {
            m_statusLabel->setText("暂停导航");
            m_statusLabel->setStyleSheet("color: #f59e0b; font-weight: bold; font-size: 14px;");
        } else if (st == "1") {
            m_statusLabel->setText("导航中");
            m_statusLabel->setStyleSheet("color: #0ea5e9; font-weight: bold; font-size: 14px;");
        } else if (st == "0") {
            m_statusLabel->setText("就绪");
            m_statusLabel->setStyleSheet("color: #22c55e; font-weight: bold; font-size: 14px;");
        } else {
            m_statusLabel->setText(info.statusText());
            m_statusLabel->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 14px;").arg(info.statusColor()));
        }
    }

    // 2. 根据 /rob/navigate/status 状态控制取消和暂停按钮状态
    // status == "1" 导航中, status == "2" 暂停导航，两者都属于正在进行导航流程
    // 用户标记：当用户点击抢占控制权时，不要让取消导航和暂停导航按钮可点击
    const bool isNav = (st == "1" || st == "2" || st == "5" || st == "3");
    const bool isUserPreempted = TeleopController::instance().isUserPreempted() || TeleopController::instance().isActive();
    const bool canControlNav = isNav && !isUserPreempted;

    if (m_cancelBtn) {
        m_cancelBtn->setEnabled(canControlNav);
    }
    if (m_pauseBtn) {
        m_pauseBtn->setEnabled(canControlNav);
        if (isPaused) {
            m_pauseBtn->setText("继续导航");
        } else if (isNav) {
            m_pauseBtn->setText("暂停导航");
        } else {
            m_pauseBtn->setText("暂无");
        }
    }

    if (st == "2" || st == "5") {
        m_isPaused = true;
    } else if (st == "1") {
        m_isPaused = false;
    }

    if (!isNav && st == "0") {
        if (m_isRunning) {
            m_isRunning = false;
            m_isPaused = false;
            m_motionWatchdog.stop();
        }
        if (m_sendBtn) {
            m_sendBtn->setEnabled(true);
        }
    }

    if (!m_isRunning || m_isPaused) return;

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
