#pragma once

#include <QObject>

class QTimer;
class QKeyEvent;
class QEvent;

/**
 * @brief AGV 全局键盘与遥控运动控制器 (Thread-Safe Reactive Singleton)
 * 
 * 核心架构与安全规范：
 * 1. 工业级安全看门狗：捕获 WindowDeactivate / FocusOut 事件，彻底解决窗口失焦导致按键粘连 (Sticky Key) 的失控隐患；
 * 2. 停用与析构零速急停：无论主动停用、窗口关闭还是程序退出，安全下发 0 速度指令；
 * 3. 动态物理参数管理：消除硬编码魔数，支持外部动态调整基础线速度、角速度与加速倍率；
 * 4. 严格依赖控制：头文件完全采用前置声明，阻断大型头文件包含；
 * 5. 100% 保持既有信号与外部调用签名兼容。
 */
class TeleopController : public QObject
{
    Q_OBJECT

public:
    static TeleopController &instance();

    // ── 物理运动参数常量 (单位: 国际标准单位) ──
    static constexpr int    kDefaultTickIntervalMs = 100;  ///< 控制器发送周期 (100ms / 10Hz)
    static constexpr int    kBoostMaxTicks         = 10;   ///< Ctrl 键 1秒内线性达最大倍率 (10*100ms)
    static constexpr double kDefaultBaseLinear     = 0.50; ///< 默认基础线速度 (0.50 m/s)
    static constexpr double kDefaultBaseAngular    = 0.30; ///< 默认基础角速度 (0.30 rad/s)
    static constexpr double kDefaultMaxMultiplier  = 2.00; ///< Ctrl 键最大加速倍率 (2.0x)

    bool isActive() const { return m_active; }
    void setActive(bool on);

    // 用户标记：是否为用户主动点击抢占控制权
    bool isUserPreempted() const { return m_userPreempted; }
    void setUserPreempted(bool preempted);

    // ── 运动学参数动态配置 ──
    double baseLinearSpeed() const { return m_baseLinearSpeed; }
    double baseAngularSpeed() const { return m_baseAngularSpeed; }
    void setBaseSpeeds(double linear, double angular);

    double maxBoostMultiplier() const { return m_maxBoostMultiplier; }
    void setMaxBoostMultiplier(double mult);

    // ── 虚拟按键接口 (供 UI 控制盘调用) ──
    void setVirtualKey(int key, bool pressed);
    bool isKeyPressed(int key) const;
    void stopAllMotion();

signals:
    void activeChanged(bool active);
    void userPreemptedChanged(bool preempted);
    void motionChanged(double linear, double angular);
    void brakeChanged(bool brake);
    void hoistChanged(int direction);   // +1 升 / -1 降 / 0 停
    void keyStateChanged(int key, bool pressed); ///< WASD/Space 状态变化 (用于 UI 联动)

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    explicit TeleopController(QObject *parent = nullptr);
    ~TeleopController() override;
    TeleopController(const TeleopController &) = delete;
    TeleopController &operator=(const TeleopController &) = delete;

    bool handleKeyEvent(QKeyEvent *event, bool pressed);
    bool isTeleopKey(int key) const;
    bool isMovementKey(int key) const;
    bool focusIsTextInput() const;

    void onTick();
    void updateMotion();
    void resetKeyStates();
    void publishTeleopEnable(bool on);
    void publishHoistStatus(const QString &status);
    void emergencyStop();

private:
    QTimer *m_timer = nullptr;
    bool    m_active = false;
    bool    m_userPreempted = false;
    bool    m_wasHoisting = false;

    // 基础运动配置
    double  m_baseLinearSpeed    = kDefaultBaseLinear;
    double  m_baseAngularSpeed   = kDefaultBaseAngular;
    double  m_maxBoostMultiplier = kDefaultMaxMultiplier;

    // 按键状态
    bool m_keyForward   = false;
    bool m_keyBack      = false;
    bool m_keyLeft      = false;
    bool m_keyRight     = false;
    bool m_keyBrake     = false;
    bool m_keyHoistUp   = false;
    bool m_keyHoistDown = false;
    bool m_ctrlHeld     = false;
    int  m_ctrlHeldTicks = 0;

    // 速度去重
    double m_lastPublishedLinear  = 0.0;
    double m_lastPublishedAngular = 0.0;
    bool   m_hasPublishedLinear   = false;
    bool   m_hasPublishedAngular  = false;
};
