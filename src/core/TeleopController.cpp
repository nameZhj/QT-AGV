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
        bool wasHoisting = (m_wasHoisting || m_keyHoistUp || m_keyHoistDown);
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
        // if (wasHoisting) {
        //     publishHoistStatus("2");
        // }
    }

    emit activeChanged(m_active);
}

bool TeleopController::eventFilter(QObject *obj, QEvent *event)
{
    if (!m_active) return QObject::eventFilter(obj, event);

    // 遥控启用时，拦截 WASD/空格的 ShortcutOverride，防止组件通过快捷键系统消费
    if (event->type() == QEvent::ShortcutOverride) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (isMovementKey(ke->key())) {
            ke->accept();
            return true;
        }
    }

    // 拦截键盘事件
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        int key = ke->key();
        bool pressed = (event->type() == QEvent::KeyPress);

        // 显式追踪 Ctrl 键状态（不拦截，仅记录）——避免定时器回调里读到过时状态
        if (key == Qt::Key_Control) {
            m_ctrlHeld = pressed;
            // 不拦截，让系统正常处理 Ctrl
            return QObject::eventFilter(obj, event);
        }

        // 上下左右/空格：遥控启用时始终拦截，无论焦点在哪
        if (isMovementKey(key)) {
            if (ke->isAutoRepeat()) {
                return true;   // 拦截自动重复事件，不往下传递给其它组件
            }
            handleKeyEvent(ke, pressed);
            return true;   // 始终拦截，不再传递给任何组件
        }

        // P/L 升降键：文本输入时不拦截（允许正常输入字母）
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
    if (auto cb = qobject_cast<QComboBox *>(w)) {
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
        bool nowHoisting = (m_keyHoistUp || m_keyHoistDown);
        int dir = 0;
        if (m_keyHoistUp)   dir = +1;
        else if (m_keyHoistDown) dir = -1;
        emit hoistChanged(dir);

        if (nowHoisting) {
            m_wasHoisting = true;
        } else if (m_wasHoisting) {
            // 抬手不再按下控制顶升的按键：只发送一次 2（停止指令）
            m_wasHoisting = false;
            // publishHoistStatus("2");
        }
    }

    updateMotion();
    return true;
}

void TeleopController::onTick()
{
    // Ctrl 线性加速渐增速率：每 tick 向最大倍率渐一步，松开立即重置
    // 参考 Vue TeleopControls/index.vue 的 boostMultiplier 逻辑
    constexpr int kBoostMaxTicks = 10;  // 10 × 100ms = 1秒内线性到达最大倍率
    if (m_ctrlHeld) {
        m_ctrlHeldTicks = qMin(m_ctrlHeldTicks + 1, kBoostMaxTicks);
    } else {
        m_ctrlHeldTicks = 0;  // 松开立即重置
    }

    updateMotion();

    // 升降：按住期间持续发送（与速度去重逻辑无关）
    if (m_keyHoistUp)
        publishHoistStatus("1");
    else if (m_keyHoistDown)
        publishHoistStatus("0");
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

    // Ctrl 线性加速倍率：
    //   不按 Ctrl： mult = 1.0 （基础速度）
    //   按住 Ctrl： mult 从 1.0 线性增长到 2.0，在 kBoostMaxTicks × 100ms 内达到最大
    //   参考 Vue TeleopControls baseSpeed=0.5, boostMultiplier=2.0
    constexpr int    kBoostMaxTicks = 10;   // 1秒内线性到达最大倍率
    constexpr double kMaxMult       = 2.0;  // 最大加速倍率（对应 Vue boostMultiplier）
    const double mult = 1.0 + (double(m_ctrlHeldTicks) / kBoostMaxTicks) * (kMaxMult - 1.0);

    constexpr double kBase = 0.5;
    constexpr double kAngBase = 0.3;

    double linear  = 0.0;
    double angular = 0.0;
    if (m_keyForward)  linear  =  kBase * mult;
    if (m_keyBack)     linear  = -kBase * mult;
    if (m_keyLeft)     angular =  kAngBase * mult;
    if (m_keyRight)    angular = -kAngBase * mult;

    const double linRounded = qRound(linear * 1000.0) / 1000.0;  // 3位小数精度，捕捉渐变过程
    const double angRounded = qRound(angular * 1000.0) / 1000.0;

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
    m_wasHoisting = false;
    m_ctrlHeld = false;   // 停用遥控时清除 Ctrl 状态
    m_ctrlHeldTicks = 0;  // 重置线性加速计数
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

void TeleopController::publishHoistStatus(const QString &status)
{
    QString agvId = AppConfig::instance().agvId();
    if (agvId.isEmpty() || !MqttService::instance().isConnected()) return;
    MqttService::instance().publish(
        QString("/rob/%1/hoisting_status").arg(agvId),
        status.toUtf8());
}
