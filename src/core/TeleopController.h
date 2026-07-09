#ifndef TELEOPCONTROLLER_H
#define TELEOPCONTROLLER_H

#include <QObject>
#include <QTimer>

// 全局遥控控制器（单例）
//
// 把 CreateMapWidget 中"键盘遥控"逻辑提取为全局组件：
//   - 通过 qApp->installEventFilter 在任意界面拦截遥控键（↑↓←→/Space/P/L）
//     返回 true 阻止事件传递到目标控件，因此遥控键优先级最高，
//     自然解决与子控件（地图编辑、表格方向键等）的键盘冲突。
//   - 100ms 定时器周期性发布速度指令到 MQTT：
//       /rob/{agvId}/linear_speed   {"velocity": X}
//       /rob/{agvId}/angular_speed  {"velocity": X}
//       /rob/{agvId}/hoisting_status "1"/"0"   (P/L 升降)
//   - 启用/停用时发布 /rob/{agvId}/teleop_enable "1"/"0"
//   - 速度按"值变化"去重发送，避免持续发 0；Space 刹车期间每 tick 强制发 0。
//   - Ctrl 加速倍率通过 event->modifiers() 检测，不拦截 Ctrl 本身，
//     不影响 Ctrl+C / Ctrl+V 等组合键。
//   - 焦点位于文本输入控件（QLineEdit/QTextEdit/QPlainTextEdit/QComboBox 可编辑）
//     时放行所有按键，避免影响用户输入。
class TeleopController : public QObject
{
    Q_OBJECT
public:
    static TeleopController &instance();

    // 当前是否启用遥控
    bool isActive() const { return m_active; }

    // 启用/停用遥控；停用时复位按键状态、停止定时器、移除事件过滤器
    void setActive(bool on);

signals:
    void activeChanged(bool active);
    // 速度变化通知（用于 HUD 显示），linear/angular 单位 m/s 与 rad/s
    void motionChanged(double linear, double angular);
    void brakeChanged(bool brake);
    void hoistChanged(int direction);   // +1 升 / -1 降 / 0 停

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    explicit TeleopController(QObject *parent = nullptr);
    ~TeleopController() override = default;
    TeleopController(const TeleopController &) = delete;
    TeleopController &operator=(const TeleopController &) = delete;

    bool handleKeyEvent(class QKeyEvent *event, bool pressed);
    bool isTeleopKey(int key) const;
    bool focusIsTextInput() const;

    void onTick();
    void updateMotion();
    void resetKeyStates();
    void publishTeleopEnable(bool on);

    QTimer *m_timer = nullptr;
    bool m_active = false;

    // 按键状态
    bool m_keyForward   = false;
    bool m_keyBack      = false;
    bool m_keyLeft      = false;
    bool m_keyRight     = false;
    bool m_keyBrake     = false;
    bool m_keyHoistUp   = false;
    bool m_keyHoistDown = false;

    // 速度去重
    double m_lastPublishedLinear  = 0.0;
    double m_lastPublishedAngular = 0.0;
    bool   m_hasPublishedLinear  = false;
    bool   m_hasPublishedAngular = false;
};

#endif // TELEOPCONTROLLER_H
