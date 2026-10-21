/**
 * @file    MotionDialog.cpp
 * @layer   GUILayer
 * @brief   Implementation of quick modal dialog for motion parameters.
 * @deps    MotionDialog.h, MotionDialogStyle.h, MotionDialogTabs.h, MotionDialogController.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MotionDialog.h"
#include "MotionDialogStyle.h"
#include "MotionDialogTabs.h"
#include "MotionDialogController.h"
#include "core/RobotInfoManager.h"
#include "core/MotionConfigManager.h"
#include "core/TeleopController.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QDoubleSpinBox>
#include <QMouseEvent>
#include <QPainter>

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

    auto updateCb = [this](bool) { onCurrentRobotInfoUpdated(RobotInfoManager::instance().currentInfo()); };
    connect(&TeleopController::instance(), &TeleopController::activeChanged, this, updateCb);
    connect(&TeleopController::instance(), &TeleopController::userPreemptedChanged, this, updateCb);
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
    const auto &mc = MotionConfigManager::instance();
    const double limitMaxSpd = mc.paramMax(QStringLiteral("max_speed"), 1.0);
    const double limitMaxAngular = mc.paramMax(QStringLiteral("max_angular_speed"), 60.0);
    const double defSpd = mc.maxSpeed();
    const double defAngular = mc.maxAngularSpeedDeg();

    if (m_transVxSpin) {
        m_transVxSpin->setRange(-limitMaxSpd, limitMaxSpd);
        if (!m_isRunning) m_transVxSpin->setValue(defSpd);
        else m_transVxSpin->setValue(qBound(-limitMaxSpd, m_transVxSpin->value(), limitMaxSpd));
    }
    if (m_transVySpin) {
        m_transVySpin->setRange(-limitMaxSpd, limitMaxSpd);
        if (!m_isRunning) m_transVySpin->setValue(0.000);
        else m_transVySpin->setValue(qBound(-limitMaxSpd, m_transVySpin->value(), limitMaxSpd));
    }
    if (m_transDistSpin) {
        m_transDistSpin->setRange(0.0, 100.0);
        if (!m_isRunning && (qFuzzyIsNull(m_transDistSpin->value()) || m_transDistSpin->value() < 0.001)) {
            m_transDistSpin->setValue(1.000);
        }
    }
    if (m_rotSpeedSpin) {
        m_rotSpeedSpin->setRange(-limitMaxAngular, limitMaxAngular);
        if (!m_isRunning) m_rotSpeedSpin->setValue(defAngular);
        else m_rotSpeedSpin->setValue(qBound(-limitMaxAngular, m_rotSpeedSpin->value(), limitMaxAngular));
    }
    if (m_rotAngleSpin) {
        m_rotAngleSpin->setRange(0.0, 360.0);
        if (!m_isRunning && (qFuzzyIsNull(m_rotAngleSpin->value()) || m_rotAngleSpin->value() < 0.001)) {
            m_rotAngleSpin->setValue(90.000);
        }
    }
    MotionDialogTabs::updateParamToolTips(this);
}

void MotionDialog::setupUi()
{
    setStyleSheet(MotionDialogStyle::kDialogStyle);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(1, 1, 1, 1);
    rootLayout->setSpacing(0);

    // 1. 标题栏 (自定义暗黑拖拽栏)
    auto *titleBar = new QWidget(this);
    titleBar->setFixedHeight(36);
    titleBar->setStyleSheet(MotionDialogStyle::kTitleBarStyle);

    auto *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(10, 0, 10, 0);
    titleLayout->setSpacing(8);

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
    closeBtn->setStyleSheet(MotionDialogStyle::kCloseBtnStyle);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    titleLayout->addWidget(closeBtn);
    rootLayout->addWidget(titleBar);

    // 2. 主体内容 (TabBar 与页面)
    auto *contentWidget = new QWidget(this);
    auto *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(12, 10, 12, 10);
    contentLayout->setSpacing(10);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->addTab(MotionDialogTabs::createTranslationTab(this), "平动");
    m_tabWidget->addTab(MotionDialogTabs::createRotationTab(this), "转动");
    contentLayout->addWidget(m_tabWidget);

    // 3. 底部控制栏
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
    emit motionCompleted();
}

void MotionDialog::setCanceledState()
{
    m_isRunning = false;
    m_isPaused = false;
    m_motionWatchdog.stop();

    if (m_sendBtn) m_sendBtn->setEnabled(true);
    onCurrentRobotInfoUpdated(RobotInfoManager::instance().currentInfo());
    emit motionCanceled();
}

void MotionDialog::onSendClicked()
{
    MotionDialogController::onSendClicked(this);
}

void MotionDialog::onCancelNavClicked()
{
    MotionDialogController::onCancelNavClicked(this);
}

void MotionDialog::onPauseNavClicked()
{
    MotionDialogController::onPauseNavClicked(this);
}

void MotionDialog::onCurrentRobotInfoUpdated(const RobotInfo &info)
{
    MotionDialogController::onCurrentRobotInfoUpdated(this, info);
}

void MotionDialog::onMotionTimeout()
{
    MotionDialogController::onMotionTimeout(this);
}
