/**
 * @file    TeleopController.cpp
 * @layer   AGVModel
 * @brief   Implementation of teleoperation speed calculator with ramp acceleration.
 * @deps    TeleopController.h, TeleopInputHandler.h, ApiService.h, MqttService.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "TeleopController.h"
#include "TeleopInputHandler.h"
#include "app/AppConfig.h"
#include "network/ApiService.h"
#include "network/MqttService.h"
#include "core/MotionConfigManager.h"

#include <QApplication>
#include <QTimer>
#include <QKeyEvent>
#include <QEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QDebug>

TeleopController &TeleopController::instance()
{
    static TeleopController inst;
    return inst;
}

TeleopController::TeleopController(QObject *parent)
    : QObject(parent), m_timer(new QTimer(this))
{
    m_timer->setInterval(kDefaultTickIntervalMs);
    connect(m_timer, &QTimer::timeout, this, &TeleopController::onTick);

    QSettings s;
    const double maxLin = MotionConfigManager::instance().maxSpeed();
    const double maxAng = MotionConfigManager::instance().maxAngularSpeedRad();
    const double defaultAngRad = kDefaultAngularSpeedDeg * MotionConfigManager::kDegToRad;
    const double savedLin = s.value(QStringLiteral("teleop/baseLinearSpeed"), kDefaultBaseLinear).toDouble();
    const double savedAng = s.value(QStringLiteral("teleop/baseAngularSpeed"), defaultAngRad).toDouble();
    m_baseLinearSpeed  = qBound(kMinSpeedLimit, savedLin, maxLin);
    m_baseAngularSpeed = qBound(kMinSpeedLimit, savedAng, maxAng);
}

TeleopController::~TeleopController()
{
    if (m_active) setActive(false);
}

void TeleopController::setBaseSpeeds(double linear, double angular)
{
    const double maxLin = MotionConfigManager::instance().maxSpeed();
    const double maxAng = MotionConfigManager::instance().maxAngularSpeedRad();
    m_baseLinearSpeed  = qBound(kMinSpeedLimit, linear, maxLin);
    m_baseAngularSpeed = qBound(kMinSpeedLimit, angular, maxAng);

    QSettings s;
    s.setValue(QStringLiteral("teleop/baseLinearSpeed"), m_baseLinearSpeed);
    s.setValue(QStringLiteral("teleop/baseAngularSpeed"), m_baseAngularSpeed);
}

void TeleopController::setMaxBoostMultiplier(double mult)
{
    m_maxBoostMultiplier = qMax(1.0, mult);
}

void TeleopController::setUserPreempted(bool preempted)
{
    if (m_userPreempted != preempted) {
        m_userPreempted = preempted;
        emit userPreemptedChanged(m_userPreempted);
    }
}

void TeleopController::setActive(bool on)
{
    if (m_active == on) return;
    m_active = on;
    setUserPreempted(on);

    if (on) {
        if (qApp) qApp->installEventFilter(this);
        resetKeyStates();
        m_timer->start();
        publishTeleopEnable(true);

        ApiService::instance().controlNavigation(QStringLiteral("pause navigation"), [](int code, const QJsonObject &resp) {
            qDebug() << "[TeleopController] 抢占控制权 pause navigation code:" << code << resp;
            if (code != 200) qWarning("抢占控制权暂停导航失败 (HTTP %d)", code);
        });
    } else {
        m_timer->stop();
        resetKeyStates();
        emergencyStop();

        if (qApp) qApp->removeEventFilter(this);
        publishTeleopEnable(false);
        emit motionChanged(0.0, 0.0);
        emit brakeChanged(false);
        emit hoistChanged(0);

        ApiService::instance().controlNavigation(QStringLiteral("continue navigation"), [](int code, const QJsonObject &resp) {
            qDebug() << "[TeleopController] 恢复导航 continue navigation code:" << code << resp;
            if (code != 200) qWarning("恢复导航失败 (HTTP %d)", code);
        });
    }

    emit activeChanged(m_active);
}

void TeleopController::emergencyStop()
{
    const QString agvId = AppConfig::instance().agvId();
    if (!agvId.isEmpty() && MqttService::instance().isConnected()) {
        const QByteArray zeroSpeedJson = QJsonDocument(QJsonObject{{"velocity", 0.0}}).toJson(QJsonDocument::Compact);
        MqttService::instance().publish(QStringLiteral("/rob/%1/linear_speed").arg(agvId), zeroSpeedJson);
        MqttService::instance().publish(QStringLiteral("/rob/%1/angular_speed").arg(agvId), zeroSpeedJson);
    }
    m_lastPublishedLinear  = 0.0;
    m_lastPublishedAngular = 0.0;
    m_hasPublishedLinear   = true;
    m_hasPublishedAngular  = true;
}

bool TeleopController::eventFilter(QObject *obj, QEvent *event)
{
    if (!m_active) return QObject::eventFilter(obj, event);
    const QEvent::Type evType = event->type();

    if (evType == QEvent::WindowDeactivate || evType == QEvent::ApplicationDeactivate) {
        TeleopInputHandler::handleWindowDeactivation(this);
        return QObject::eventFilter(obj, event);
    }

    if (evType == QEvent::ShortcutOverride) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (TeleopInputHandler::isMovementKey(ke->key())) {
            ke->accept();
            return true;
        }
    }

    if (evType == QEvent::KeyPress || evType == QEvent::KeyRelease) {
        if (TeleopInputHandler::handleFilteredKeyEvent(this, static_cast<QKeyEvent *>(event))) {
            return true;
        }
    }

    return QObject::eventFilter(obj, event);
}

void TeleopController::setVirtualKey(int key, bool pressed)
{
    TeleopInputHandler::setVirtualKey(this, key, pressed);
}

bool TeleopController::isKeyPressed(int key) const
{
    return TeleopInputHandler::isKeyPressed(this, key);
}

void TeleopController::stopAllMotion()
{
    resetKeyStates();
    emergencyStop();
    emit motionChanged(0.0, 0.0);
}

void TeleopController::onTick()
{
    if (m_ctrlHeld) {
        m_ctrlHeldTicks = qMin(m_ctrlHeldTicks + 1, kBoostMaxTicks);
    } else {
        m_ctrlHeldTicks = 0;
    }

    updateMotion();

    if (m_keyHoistUp) {
        publishHoistStatus(QStringLiteral("1"));
    } else if (m_keyHoistDown) {
        publishHoistStatus(QStringLiteral("0"));
    }
}

void TeleopController::updateMotion()
{
    const QString agvId = AppConfig::instance().agvId();
    if (agvId.isEmpty() || !MqttService::instance().isConnected()) return;

    if (m_keyBrake) {
        const QByteArray zeroJson = QJsonDocument(QJsonObject{{"velocity", 0.0}}).toJson(QJsonDocument::Compact);
        MqttService::instance().publish(QStringLiteral("/rob/%1/linear_speed").arg(agvId), zeroJson);
        MqttService::instance().publish(QStringLiteral("/rob/%1/angular_speed").arg(agvId), zeroJson);

        m_lastPublishedLinear  = 0.0;
        m_lastPublishedAngular = 0.0;
        m_hasPublishedLinear   = true;
        m_hasPublishedAngular  = true;
        emit motionChanged(0.0, 0.0);
        return;
    }

    const double mult = 1.0 + (static_cast<double>(m_ctrlHeldTicks) / kBoostMaxTicks) * (m_maxBoostMultiplier - 1.0);
    const double maxLin = MotionConfigManager::instance().maxSpeed();
    const double maxAng = MotionConfigManager::instance().maxAngularSpeedRad();

    double linear  = 0.0;
    double angular = 0.0;
    if (m_keyForward)  linear  =  qMin(m_baseLinearSpeed * mult, maxLin);
    if (m_keyBack)     linear  = -qMin(m_baseLinearSpeed * mult, maxLin);
    if (m_keyLeft)     angular =  qMin(m_baseAngularSpeed * mult, maxAng);
    if (m_keyRight)    angular = -qMin(m_baseAngularSpeed * mult, maxAng);

    const double linRounded = qRound(linear * kSpeedRoundFactor) / kSpeedRoundFactor;
    const double angRounded = qRound(angular * kSpeedRoundFactor) / kSpeedRoundFactor;

    if (!m_hasPublishedLinear || linRounded != m_lastPublishedLinear) {
        MqttService::instance().publish(
            QStringLiteral("/rob/%1/linear_speed").arg(agvId),
            QJsonDocument(QJsonObject{{"velocity", linear}}).toJson(QJsonDocument::Compact));
        m_lastPublishedLinear = linRounded;
        m_hasPublishedLinear = true;
    }
    if (!m_hasPublishedAngular || angRounded != m_lastPublishedAngular) {
        MqttService::instance().publish(
            QStringLiteral("/rob/%1/angular_speed").arg(agvId),
            QJsonDocument(QJsonObject{{"velocity", angular}}).toJson(QJsonDocument::Compact));
        m_lastPublishedAngular = angRounded;
        m_hasPublishedAngular = true;
    }

    emit motionChanged(linear, angular);
}

void TeleopController::resetKeyStates()
{
    const bool hadForward = m_keyForward;
    const bool hadBack    = m_keyBack;
    const bool hadLeft    = m_keyLeft;
    const bool hadRight   = m_keyRight;
    const bool hadBrake   = m_keyBrake;

    m_keyForward   = false;
    m_keyBack      = false;
    m_keyLeft      = false;
    m_keyRight     = false;
    m_keyBrake     = false;
    m_keyHoistUp   = false;
    m_keyHoistDown = false;
    m_wasHoisting  = false;
    m_ctrlHeld     = false;
    m_ctrlHeldTicks = 0;
    m_hasPublishedLinear   = false;
    m_hasPublishedAngular  = false;
    m_lastPublishedLinear  = 0.0;
    m_lastPublishedAngular = 0.0;

    if (hadForward) emit keyStateChanged(Qt::Key_W, false);
    if (hadBack)    emit keyStateChanged(Qt::Key_S, false);
    if (hadLeft)    emit keyStateChanged(Qt::Key_A, false);
    if (hadRight)   emit keyStateChanged(Qt::Key_D, false);
    if (hadBrake)   emit keyStateChanged(Qt::Key_Space, false);
}

void TeleopController::publishTeleopEnable(bool on)
{
    const QString agvId = AppConfig::instance().agvId();
    if (agvId.isEmpty() || !MqttService::instance().isConnected()) return;
    MqttService::instance().publish(QStringLiteral("/rob/%1/teleop_enable").arg(agvId), on ? "1" : "0");
}

void TeleopController::publishHoistStatus(const QString &status)
{
    const QString agvId = AppConfig::instance().agvId();
    if (agvId.isEmpty() || !MqttService::instance().isConnected()) return;
    MqttService::instance().publish(QStringLiteral("/rob/%1/hoisting_status").arg(agvId), status.toUtf8());
}
