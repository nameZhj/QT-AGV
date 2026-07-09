#include "TeleopController.h"
#include "core/AppConfig.h"
#include "core/MqttService.h"

#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QJsonDocument>
#include <QJsonObject>

TeleopController &TeleopController::instance()
{
    static TeleopController inst;
    return inst;
}

TeleopController::TeleopController(QObject *parent)
    : QObject(parent)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(100);
    connect(m_timer, &QTimer::timeout, this, &TeleopController::onTick);
}

void TeleopController::setActive(bool on)
{
    if (m_active == on) return;
    m_active = on;

    if (on) {
        // 启用：安装全局事件过滤器拦截遥控键、启动定时器、发布启用信号
        qApp->installEventFilter(this);
        resetKeyStates();
        m_timer->start();
        publishTeleopEnable(true);
    } else {
        // 停用：复位按键、停止定时器、移除事件过滤器、发布停用信号
        m_timer->stop();
        resetKeyStates();
        // 停用瞬间强制发一次零速，确保机器人停止
        QString agvId = AppConfig::instance().agvId();
        if (!agvId.isEmpty() && MqttService::instance().isConnected()) {
            MqttService::instance().publish(
                QString("/rob/%1/linear_speed").arg(agvId),
                QJsonDocument(QJsonObject{{"velocity", 0.0}}).toJson(QJsonDocument::Compact));
            MqttService::instance().publish(
                QString("/rob/%1/angular_speed").arg(agvId),
                QJsonDocument(QJsonObject{{"velocity", 0.0}}).toJson(QJsonDocument::Compact));
        }
        qApp->removeEventFilter(this);
        publishTeleopEnable(false);
        emit motionChanged(0.0, 0.0);
        emit brakeChanged(false);
        emit hoistChanged(0);
    }

    emit activeChanged(m_active);
}

bool TeleopController::eventFilter(QObject *obj, QEvent *event)
{
    if (!m_active) return QObject::eventFilter(obj, event);

    // 只在遥控启用时拦截键盘事件
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        // 自动重复事件不处理（交给底层控件默认行为，避免重复触发）
        if (ke->isAutoRepeat()) {
            return QObject::eventFilter(obj, event);
        }
        int key = ke->key();
        if (!isTeleopKey(key)) {
            return QObject::eventFilter(obj, event);
        }
        // 焦点位于文本输入控件时放行，避免影响输入（如 QLineEdit 中按 P/L/Space）
        if (focusIsTextInput()) {
            return QObject::eventFilter(obj, event);
        }
        bool pressed = (event->type() == QEvent::KeyPress);
        if (handleKeyEvent(ke, pressed)) {
            return true;   // 拦截事件，不再传递给目标控件 → 遥控键优先级最高
        }
    }
    return QObject::eventFilter(obj, event);
}

bool TeleopController::isTeleopKey(int key) const
{
    switch (key) {
    case Qt::Key_Up:
    case Qt::Key_Down:
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Space:
    case Qt::Key_P:
    case Qt::Key_L:
        return true;
    default:
        return false;
    }
}

bool TeleopController::focusIsTextInput() const
{
    QWidget *w = qApp ? qApp->focusWidget() : nullptr;
    if (!w) return false;
    if (qobject_cast<QLineEdit *>(w))       return true;
    if (qobject_cast<QTextEdit *>(w))       return true;
    if (qobject_cast<QPlainTextEdit *>(w))  return true;
    if (auto cb = qobject_cast<QComboBox *>(w)) {
        if (cb->isEditable()) return true;
    }
    return false;
}

bool TeleopController::handleKeyEvent(QKeyEvent *event, bool pressed)
{
    switch (event->key()) {
    case Qt::Key_Up:    m_keyForward   = pressed; break;
    case Qt::Key_Down:  m_keyBack      = pressed; break;
    case Qt::Key_Left:  m_keyLeft      = pressed; break;
    case Qt::Key_Right: m_keyRight     = pressed; break;
    case Qt::Key_Space: m_keyBrake     = pressed; break;
    case Qt::Key_P:     m_keyHoistUp   = pressed; break;
    case Qt::Key_L:     m_keyHoistDown = pressed; break;
    default: return false;
    }

    if (event->key() == Qt::Key_Space) {
        emit brakeChanged(m_keyBrake);
    }
    if (event->key() == Qt::Key_P || event->key() == Qt::Key_L) {
        int dir = 0;
        if (m_keyHoistUp)   dir = +1;
        else if (m_keyHoistDown) dir = -1;
        emit hoistChanged(dir);
    }

    updateMotion();
    return true;
}

void TeleopController::onTick()
{
    updateMotion();

    // 升降：按住期间持续发送（与速度去重逻辑无关）
    QString agvId = AppConfig::instance().agvId();
    if (agvId.isEmpty() || !MqttService::instance().isConnected()) return;

    if (m_keyHoistUp)
        MqttService::instance().publish(QString("/rob/%1/hoisting_status").arg(agvId), "1");
    else if (m_keyHoistDown)
        MqttService::instance().publish(QString("/rob/%1/hoisting_status").arg(agvId), "0");
}

void TeleopController::updateMotion()
{
    QString agvId = AppConfig::instance().agvId();
    if (agvId.isEmpty() || !MqttService::instance().isConnected()) return;

    if (m_keyBrake) {
        // 刹车：每 tick 强制发 0
        MqttService::instance().publish(
            QString("/rob/%1/linear_speed").arg(agvId),
            QJsonDocument(QJsonObject{{"velocity", 0.0}}).toJson(QJsonDocument::Compact));
        MqttService::instance().publish(
            QString("/rob/%1/angular_speed").arg(agvId),
            QJsonDocument(QJsonObject{{"velocity", 0.0}}).toJson(QJsonDocument::Compact));
        m_lastPublishedLinear = 0.0;
        m_lastPublishedAngular = 0.0;
        m_hasPublishedLinear = m_hasPublishedAngular = true;
        emit motionChanged(0.0, 0.0);
        return;
    }

    // 加速倍率：通过当前应用级键盘修饰状态检测
    // 注意：Ctrl 本身不被 TeleopController 拦截，因此 Ctrl+方向键 触发加速
    Qt::KeyboardModifiers mods = qApp->keyboardModifiers();
    const double mult = (mods & Qt::ControlModifier) ? 2.0 : 1.0;

    constexpr double kBase = 0.5;
    constexpr double kAngBase = 0.3;

    double linear  = 0.0;
    double angular = 0.0;
    if (m_keyForward)  linear  =  kBase * mult;
    if (m_keyBack)     linear  = -kBase * mult;
    if (m_keyLeft)     angular =  kAngBase * mult;
    if (m_keyRight)    angular = -kAngBase * mult;

    const double linRounded = qRound(linear * 100.0) / 100.0;
    const double angRounded = qRound(angular * 100.0) / 100.0;

    if (!m_hasPublishedLinear || linRounded != m_lastPublishedLinear) {
        MqttService::instance().publish(
            QString("/rob/%1/linear_speed").arg(agvId),
            QJsonDocument(QJsonObject{{"velocity", linear}}).toJson(QJsonDocument::Compact));
        m_lastPublishedLinear = linRounded;
        m_hasPublishedLinear = true;
    }
    if (!m_hasPublishedAngular || angRounded != m_lastPublishedAngular) {
        MqttService::instance().publish(
            QString("/rob/%1/angular_speed").arg(agvId),
            QJsonDocument(QJsonObject{{"velocity", angular}}).toJson(QJsonDocument::Compact));
        m_lastPublishedAngular = angRounded;
        m_hasPublishedAngular = true;
    }

    emit motionChanged(linear, angular);
}

void TeleopController::resetKeyStates()
{
    m_keyForward = m_keyBack = m_keyLeft = m_keyRight = false;
    m_keyBrake = m_keyHoistUp = m_keyHoistDown = false;
    m_hasPublishedLinear = m_hasPublishedAngular = false;
    m_lastPublishedLinear = 0.0;
    m_lastPublishedAngular = 0.0;
}

void TeleopController::publishTeleopEnable(bool on)
{
    QString agvId = AppConfig::instance().agvId();
    if (agvId.isEmpty() || !MqttService::instance().isConnected()) return;
    MqttService::instance().publish(
        QString("/rob/%1/teleop_enable").arg(agvId),
        on ? "1" : "0");
}
