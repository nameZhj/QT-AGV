#include "TeleopController.h"
#include "core/AppConfig.h"
#include "core/ApiService.h"
#include "core/MqttService.h"
#include "core/MotionConfigManager.h"

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
#include <QSettings>
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

    // 读取持久化速度设置，并受全局最大速度约束
    QSettings s;
    double maxLin = MotionConfigManager::instance().maxSpeed();
    double maxAng = MotionConfigManager::instance().maxAngularSpeedRad();
    double savedLin = s.value(QStringLiteral("teleop/baseLinearSpeed"), kDefaultBaseLinear).toDouble();
    double savedAng = s.value(QStringLiteral("teleop/baseAngularSpeed"), 45.0 * 3.14159265358979323846 / 180.0).toDouble();
    m_baseLinearSpeed  = qBound(0.01, savedLin, maxLin);
    m_baseAngularSpeed = qBound(0.01, savedAng, maxAng);
}

TeleopController::~TeleopController()
{
    if (m_active) {
        setActive(false);
    }
}

void TeleopController::setBaseSpeeds(double linear, double angular)
{
    double maxLin = MotionConfigManager::instance().maxSpeed();
    double maxAng = MotionConfigManager::instance().maxAngularSpeedRad();
    m_baseLinearSpeed  = qBound(0.01, linear, maxLin);
    m_baseAngularSpeed = qBound(0.01, angular, maxAng);

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
        // 启用：安装全局事件过滤器拦截遥控键、启动定时器、发布启用状态
        if (qApp) {
            qApp->installEventFilter(this);
        }
        resetKeyStates();
        m_timer->start();
        publishTeleopEnable(true);

        // 抢占控制权：统一使用 PUT /robot/navigation/control?command=pause navigation
        ApiService::instance().controlNavigation(QStringLiteral("pause navigation"), [](int code, const QJsonObject &resp) {
            qDebug() << "[TeleopController] 抢占控制权 pause navigation code:" << code << resp;
            if (code != 200) {
                qWarning("抢占控制权暂停导航失败 (HTTP %d)", code);
            }
        });
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

        // 释放控制权恢复导航：统一使用 PUT /robot/navigation/control?command=continue navigation
        ApiService::instance().controlNavigation(QStringLiteral("continue navigation"), [](int code, const QJsonObject &resp) {
            qDebug() << "[TeleopController] 恢复导航 continue navigation code:" << code << resp;
            if (code != 200) {
                qWarning("恢复导航失败 (HTTP %d)", code);
            }
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
    const int key = event->key();
    switch (key) {
    case Qt::Key_W:
        if (m_keyForward != pressed) {
            m_keyForward = pressed;
            emit keyStateChanged(Qt::Key_W, pressed);
        }
        break;
    case Qt::Key_S:
        if (m_keyBack != pressed) {
            m_keyBack = pressed;
            emit keyStateChanged(Qt::Key_S, pressed);
        }
        break;
    case Qt::Key_A:
        if (m_keyLeft != pressed) {
            m_keyLeft = pressed;
            emit keyStateChanged(Qt::Key_A, pressed);
        }
        break;
    case Qt::Key_D:
        if (m_keyRight != pressed) {
            m_keyRight = pressed;
            emit keyStateChanged(Qt::Key_D, pressed);
        }
        break;
    case Qt::Key_Space:
        if (m_keyBrake != pressed) {
            m_keyBrake = pressed;
            emit keyStateChanged(Qt::Key_Space, pressed);
        }
        break;
    case Qt::Key_P:     m_keyHoistUp   = pressed; break;
    case Qt::Key_L:     m_keyHoistDown = pressed; break;
    default: return false;
    }

    if (key == Qt::Key_Space) {
        emit brakeChanged(m_keyBrake);
    }
    if (key == Qt::Key_P || key == Qt::Key_L) {
        int dir = 0;
        if (m_keyHoistUp)   dir = +1;
        else if (m_keyHoistDown) dir = -1;
        emit hoistChanged(dir);

        m_wasHoisting = (m_keyHoistUp || m_keyHoistDown);
    }

    updateMotion();
    return true;
}

void TeleopController::setVirtualKey(int key, bool pressed)
{
    // 未抢占控制权时忽略方向/转向虚拟键: 控制权必须由用户显式抢占 (点击"抢占控制权"/
    // "启用遥控")，禁止任何按键操作被动接管车辆控制权。
    // 例外: 刹车(空格 / 方向键盘中心方块)是安全动作，任何时候都可用，无需抢占控制权
    if (!m_active && key != Qt::Key_Space) return;

    switch (key) {
    case Qt::Key_W:
        if (m_keyForward != pressed) {
            m_keyForward = pressed;
            emit keyStateChanged(Qt::Key_W, pressed);
        }
        break;
    case Qt::Key_S:
        if (m_keyBack != pressed) {
            m_keyBack = pressed;
            emit keyStateChanged(Qt::Key_S, pressed);
        }
        break;
    case Qt::Key_A:
        if (m_keyLeft != pressed) {
            m_keyLeft = pressed;
            emit keyStateChanged(Qt::Key_A, pressed);
        }
        break;
    case Qt::Key_D:
        if (m_keyRight != pressed) {
            m_keyRight = pressed;
            emit keyStateChanged(Qt::Key_D, pressed);
        }
        break;
    case Qt::Key_Space:
        if (m_keyBrake != pressed) {
            m_keyBrake = pressed;
            emit keyStateChanged(Qt::Key_Space, pressed);
            emit brakeChanged(m_keyBrake);

            // 未抢占控制权时主定时器未运行，按住刹车期间临时启动它，
            // 使其与"空格刹车"一致: 按住期间持续下发零速，松开即恢复
            if (!m_active) {
                if (pressed) m_timer->start();
                else         m_timer->stop();
            }
        }
        break;
    default:
        return;
    }

    updateMotion();
}

bool TeleopController::isKeyPressed(int key) const
{
    switch (key) {
    case Qt::Key_W:     return m_keyForward;
    case Qt::Key_S:     return m_keyBack;
    case Qt::Key_A:     return m_keyLeft;
    case Qt::Key_D:     return m_keyRight;
    case Qt::Key_Space: return m_keyBrake;
    case Qt::Key_P:     return m_keyHoistUp;
    case Qt::Key_L:     return m_keyHoistDown;
    default: return false;
    }
}

void TeleopController::stopAllMotion()
{
    resetKeyStates();
    emergencyStop();
    emit motionChanged(0.0, 0.0);
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

    // 计算加速倍率与当前目标速度 (严格受全局最大速度限制不可超速)
    const double mult = 1.0 + (static_cast<double>(m_ctrlHeldTicks) / kBoostMaxTicks) * (m_maxBoostMultiplier - 1.0);
    const double maxLin = MotionConfigManager::instance().maxSpeed();
    const double maxAng = MotionConfigManager::instance().maxAngularSpeedRad();

    double linear  = 0.0;
    double angular = 0.0;
    if (m_keyForward)  linear  =  qMin(m_baseLinearSpeed * mult, maxLin);
    if (m_keyBack)     linear  = -qMin(m_baseLinearSpeed * mult, maxLin);
    if (m_keyLeft)     angular =  qMin(m_baseAngularSpeed * mult, maxAng);
    if (m_keyRight)    angular = -qMin(m_baseAngularSpeed * mult, maxAng);

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
