#include "TeleopController.h"
#include "core/AppConfig.h"
#include "core/MqttService.h"

#include <QApplication>
#include <QTimer>
#include <QKeyEvent>
#include <QEvent>
#include <QLineEdit>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

TeleopController &TeleopController::instance()
{
    static TeleopController inst;
    return inst;
}

TeleopController::TeleopController(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    m_timer->setInterval(kDefaultTickIntervalMs);
    connect(m_timer, &QTimer::timeout, this, &TeleopController::onTick);
}

TeleopController::~TeleopController()
{
    if (m_active) {
        setActive(false);
    }
}

void TeleopController::setBaseSpeeds(double linear, double angular)
{
    m_baseLinearSpeed  = qMax(0.05, linear);
    m_baseAngularSpeed = qMax(0.05, angular);
}

void TeleopController::setMaxBoostMultiplier(double mult)
{
    m_maxBoostMultiplier = qMax(1.0, mult);
}

void TeleopController::setActive(bool on)
{
    if (m_active == on) return;
    m_active = on;

    if (on) {
        // 启用：安装全局事件过滤器拦截遥控键、启动定时器、发布启用状态
        if (qApp) {
            qApp->installEventFilter(this);
        }
        resetKeyStates();
        m_timer->start();
        publishTeleopEnable(true);
    } else {
        // 停用：安全停止定时器、复位按键、下发零速急停指令、移除事件过滤器
        m_timer->stop();
        resetKeyStates();
        emergencyStop();

        if (qApp) {
            qApp->removeEventFilter(this);
        }
        publishTeleopEnable(false);
        emit motionChanged(0.0, 0.0);
        emit brakeChanged(false);
        emit hoistChanged(0);
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

    // 工业级安全看门狗：窗口失焦或系统切换 (Alt+Tab/切屏) 时，自动复位按键并下发急停，杜绝按键粘连
    if (event->type() == QEvent::WindowDeactivate || event->type() == QEvent::ApplicationDeactivate) {
        if (m_keyForward || m_keyBack || m_keyLeft || m_keyRight || m_keyHoistUp || m_keyHoistDown) {
            qWarning().noquote() << QStringLiteral("[TeleopController Safety] 窗口失焦，触发安全急停与按键复位");
            resetKeyStates();
            emergencyStop();
            emit motionChanged(0.0, 0.0);
            emit hoistChanged(0);
        }
    }

    // 遥控启用时，拦截 WASD/空格的 ShortcutOverride，防止组件通过快捷键系统消费
    if (event->type() == QEvent::ShortcutOverride) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (isMovementKey(ke->key())) {
            ke->accept();
            return true;
        }
    }

    // 拦截键盘事件
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        auto *ke = static_cast<QKeyEvent *>(event);
        const int key = ke->key();
        const bool pressed = (event->type() == QEvent::KeyPress);

        // 显式追踪 Ctrl 键状态（不拦截，仅记录）
        if (key == Qt::Key_Control) {
            m_ctrlHeld = pressed;
            return QObject::eventFilter(obj, event);
        }

        // 上下左右/空格：遥控启用时始终拦截，无论焦点在哪
        if (isMovementKey(key)) {
            if (ke->isAutoRepeat()) {
                return true;   // 拦截自动重复事件，不往下传递给其它组件
            }
            handleKeyEvent(ke, pressed);
            return true;
        }

        // P/L 升降键：文本输入时放行，允许正常输入英文字母
        if (isTeleopKey(key) && !focusIsTextInput()) {
            if (ke->isAutoRepeat()) {
                return true;
            }
            if (handleKeyEvent(ke, pressed)) {
                return true;
            }
        }
    }

    return QObject::eventFilter(obj, event);
}

bool TeleopController::isMovementKey(int key) const
{
    switch (key) {
    case Qt::Key_W:
    case Qt::Key_S:
    case Qt::Key_A:
    case Qt::Key_D:
    case Qt::Key_Space:
        return true;
    default:
        return false;
    }
}

bool TeleopController::isTeleopKey(int key) const
{
    switch (key) {
    case Qt::Key_W:
    case Qt::Key_S:
    case Qt::Key_A:
    case Qt::Key_D:
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
    if (auto *cb = qobject_cast<QComboBox *>(w)) {
        if (cb->isEditable()) return true;
    }
    return false;
}

bool TeleopController::handleKeyEvent(QKeyEvent *event, bool pressed)
{
    switch (event->key()) {
    case Qt::Key_W:     m_keyForward   = pressed; break;
    case Qt::Key_S:     m_keyBack      = pressed; break;
    case Qt::Key_A:     m_keyLeft      = pressed; break;
    case Qt::Key_D:     m_keyRight     = pressed; break;
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

        m_wasHoisting = (m_keyHoistUp || m_keyHoistDown);
    }

    updateMotion();
    return true;
}

void TeleopController::onTick()
{
    // Ctrl 线性加速渐增：每 tick 递增一步，松开立即恢复 1.0 倍率
    if (m_ctrlHeld) {
        m_ctrlHeldTicks = qMin(m_ctrlHeldTicks + 1, kBoostMaxTicks);
    } else {
        m_ctrlHeldTicks = 0;
    }

    updateMotion();

    // 升降：按住期间持续发送状态
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
        // 刹车状态：每 tick 强制发送零速
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

    // 计算加速倍率与当前目标速度
    const double mult = 1.0 + (static_cast<double>(m_ctrlHeldTicks) / kBoostMaxTicks) * (m_maxBoostMultiplier - 1.0);

    double linear  = 0.0;
    double angular = 0.0;
    if (m_keyForward)  linear  =  m_baseLinearSpeed * mult;
    if (m_keyBack)     linear  = -m_baseLinearSpeed * mult;
    if (m_keyLeft)     angular =  m_baseAngularSpeed * mult;
    if (m_keyRight)    angular = -m_baseAngularSpeed * mult;

    const double linRounded = qRound(linear * 1000.0) / 1000.0;
    const double angRounded = qRound(angular * 1000.0) / 1000.0;

    // 速度值变化去重下发
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
}

void TeleopController::publishTeleopEnable(bool on)
{
    const QString agvId = AppConfig::instance().agvId();
    if (agvId.isEmpty() || !MqttService::instance().isConnected()) return;
    MqttService::instance().publish(
        QStringLiteral("/rob/%1/teleop_enable").arg(agvId),
        on ? "1" : "0");
}

void TeleopController::publishHoistStatus(const QString &status)
{
    const QString agvId = AppConfig::instance().agvId();
    if (agvId.isEmpty() || !MqttService::instance().isConnected()) return;
    MqttService::instance().publish(
        QStringLiteral("/rob/%1/hoisting_status").arg(agvId),
        status.toUtf8());
}
