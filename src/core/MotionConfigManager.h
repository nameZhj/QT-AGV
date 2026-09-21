#pragma once

#include <QObject>
#include <QJsonObject>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * @brief 全局运动控制配置管理器 (单例)
 * 
 * 核心功能:
 * 1. 集中管理与持久化本地缓存运动控制参数:
 *    - max_speed: 最大线速度配置值 (m/s, 默认 0.5)
 *    - max_acceleration: 最大线加速度配置值 (m/s², 默认 0.3)
 *    - max_deceleration: 最大线减速度配置值 (m/s², 默认 0.3)
 *    - max_angular_speed: 最大角速度配置值 (°/s, 默认 60.0)
 *    - arrival_distance_tolerance: 到达目标允许的距离误差 (m, 默认 0.1)
 *    - arrival_angle_tolerance: 到达目标允许的角度误差 (°, 默认 0.5)
 *    - route_width: 路线宽度配置值 (m, 默认 4.0)
 * 2. 对“导航”、“启用编辑”、“移动”、“设置速度”等功能施加全局最大速度强约束，防止超速。
 * 3. 支持 QSettings 启动自恢复与保存自动持久化。
 */
class MotionConfigManager : public QObject
{
    Q_OBJECT

public:
    static MotionConfigManager &instance();

    // --- 读取接口 ---
    double maxSpeed() const { return m_maxSpeed; }
    double maxAcceleration() const { return m_maxAcceleration; }
    double maxDeceleration() const { return m_maxDeceleration; }
    double maxAngularSpeedDeg() const { return m_maxAngularSpeedDeg; }
    double maxAngularSpeedRad() const { return m_maxAngularSpeedDeg * M_PI / 180.0; }
    double arrivalDistanceTolerance() const { return m_arrivalDistanceTolerance; }
    double arrivalAngleToleranceDeg() const { return m_arrivalAngleToleranceDeg; }
    double arrivalAngleToleranceRad() const { return m_arrivalAngleToleranceDeg * M_PI / 180.0; }
    double routeWidth() const { return m_routeWidth; }

    // --- 写入与同步接口 (自动持久化到本地 QSettings 并发射信号) ---
    void setMaxSpeed(double val);
    void setMaxAcceleration(double val);
    void setMaxDeceleration(double val);
    void setMaxAngularSpeedDeg(double val);
    void setArrivalDistanceTolerance(double val);
    void setArrivalAngleToleranceDeg(double val);
    void setRouteWidth(double val);

    // 批量从 QJsonObject (如配置参数中的 params) 更新
    void updateFromJsonObject(const QJsonObject &params);
    QJsonObject toJsonObject() const;

    // --- 安全限速约束辅助方法 (Clamp) ---
    // 线速度正向限幅 [minVal, max_speed]
    double clampLinearSpeed(double spd, double minVal = 0.01) const;
    // 线速度双向限幅 [-max_speed, max_speed]
    double clampSignedLinearSpeed(double spd) const;
    // 角度制角速度正向限幅 [minVal, max_angular_speed]
    double clampAngularSpeedDeg(double deg, double minVal = 0.01) const;
    // 弧度制角速度正向限幅 [minVal, max_angular_speed_rad]
    double clampAngularSpeedRad(double rad, double minVal = 0.01) const;
    // 角度制角速度双向限幅 [-max_angular_speed, max_angular_speed]
    double clampSignedAngularSpeedDeg(double deg) const;
    // 弧度制角速度双向限幅 [-max_angular_speed_rad, max_angular_speed_rad]
    double clampSignedAngularSpeedRad(double rad) const;
    // 加速度限幅 [0.01, max_acceleration]
    double clampAcceleration(double acc) const;
    // 减速度限幅 [0.01, max_deceleration]
    double clampDeceleration(double dec) const;

signals:
    void configChanged();

private:
    explicit MotionConfigManager(QObject *parent = nullptr);
    ~MotionConfigManager() override = default;
    MotionConfigManager(const MotionConfigManager &) = delete;
    MotionConfigManager &operator=(const MotionConfigManager &) = delete;

    void loadFromSettings();
    void saveToSettings();

    double m_maxSpeed                  = 0.5;   // m/s
    double m_maxAcceleration           = 0.3;   // m/s²
    double m_maxDeceleration           = 0.3;   // m/s²
    double m_maxAngularSpeedDeg        = 60.0;  // °/s
    double m_arrivalDistanceTolerance  = 0.1;   // m
    double m_arrivalAngleToleranceDeg  = 0.5;   // °
    double m_routeWidth                = 4.0;   // m
};
