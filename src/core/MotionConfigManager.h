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

    // --- 读取接口 (返回当前“参数”中配置的全局值；图元未修改时以本参数值为准) ---
    double maxSpeed() const { return m_maxSpeed; }
    double maxAcceleration() const { return m_maxAcceleration; }
    double maxDeceleration() const { return m_maxDeceleration; }
    double maxAngularSpeedDeg() const { return m_maxAngularSpeedDeg; }
    double maxAngularSpeedRad() const { return m_maxAngularSpeedDeg * M_PI / 180.0; }
    double maxAngularAccelerationDeg() const { return m_maxAngularAccelerationDeg; }
    double maxAngularAccelerationRad() const { return m_maxAngularAccelerationDeg * M_PI / 180.0; }
    double arrivalDistanceTolerance() const { return m_arrivalDistanceTolerance; }
    double arrivalAngleToleranceDeg() const { return m_arrivalAngleToleranceDeg; }
    double arrivalAngleToleranceRad() const { return m_arrivalAngleToleranceDeg * M_PI / 180.0; }
    double routeWidth() const { return m_routeWidth; }

    // --- 默认避障参数读取接口 (来自“配置参数”中的“障碍物配置”) ---
    double defaultObsStopLength() const { return m_defaultObsStopLength; }
    double defaultObsStopWidth() const { return m_defaultObsStopWidth; }
    double defaultObsDecelLength() const { return m_defaultObsDecelLength; }
    double defaultObsDecelWidth() const { return m_defaultObsDecelWidth; }

    // --- 元数据与参数范围支持 (来自 /rbrainrobot/get_config_metadata 原有接口) ---
    struct MotionParamMeta {
        double defaultValue = 0.0;
        double minVal = 0.0;
        double maxVal = 0.0;
        bool hasMin = false;
        bool hasMax = false;
        bool hasDefault = false;
    };

    void updateFromMetadata(const QByteArray &rawJson);
    void updateFromMetadataArray(const QJsonArray &metaList);
    void updateFromConfigsArray(const QJsonArray &configsList);

    bool hasParamMeta(const QString &paramName) const;
    MotionParamMeta paramMeta(const QString &paramName) const;
    double paramMin(const QString &paramName, double fallback = 0.0) const;
    double paramMax(const QString &paramName, double fallback = 0.0) const;
    double paramDefault(const QString &paramName, double fallback = 0.0) const;
    double clampByParamMeta(const QString &paramName, double val) const;
    const QMap<QString, MotionParamMeta> &allParamMetas() const { return m_paramMetas; }

    // --- 写入与同步接口 (自动持久化到本地 QSettings 并发射信号) ---
    void setMaxSpeed(double val);
    void setMaxAcceleration(double val);
    void setMaxDeceleration(double val);
    void setMaxAngularSpeedDeg(double val);
    void setMaxAngularAccelerationDeg(double val);
    void setArrivalDistanceTolerance(double val);
    void setArrivalAngleToleranceDeg(double val);
    void setRouteWidth(double val);
    void setDefaultObsStopRange(double dx, double dy);
    void setDefaultObsDecelRange(double dx, double dy);

    // 批量从 QJsonObject (如配置参数中的 params) 更新
    void updateFromJsonObject(const QJsonObject &params);
    void updateObstacleConfig(const QJsonObject &params);
    QJsonObject toJsonObject() const;

    // --- 安全限速约束辅助方法 (Clamp) ---
    // 线速度正向限幅 [minVal, max_speed]
    double clampLinearSpeed(double spd, double minVal = 0.0) const;
    // 线速度双向限幅 [-max_speed, max_speed]
    double clampSignedLinearSpeed(double spd) const;
    // 角度制角速度正向限幅 [minVal, max_angular_speed]
    double clampAngularSpeedDeg(double deg, double minVal = 0.0) const;
    // 弧度制角速度正向限幅 [minVal, max_angular_speed_rad]
    double clampAngularSpeedRad(double rad, double minVal = 0.0) const;
    // 角度制角速度双向限幅 [-max_angular_speed, max_angular_speed]
    double clampSignedAngularSpeedDeg(double deg) const;
    // 弧度制角速度双向限幅 [-max_angular_speed_rad, max_angular_speed_rad]
    double clampSignedAngularSpeedRad(double rad) const;
    // 加速度限幅 [minVal, max_acceleration]
    double clampAcceleration(double acc) const;
    // 减速度限幅 [minVal, max_deceleration]
    double clampDeceleration(double dec) const;
    // 角加速度限幅 [minVal, max_angular_acceleration]
    double clampAngularAccelerationDeg(double acc) const;

signals:
    void configChanged();

private:
    explicit MotionConfigManager(QObject *parent = nullptr);
    ~MotionConfigManager() override = default;
    MotionConfigManager(const MotionConfigManager &) = delete;
    MotionConfigManager &operator=(const MotionConfigManager &) = delete;

    void loadFromSettings();
    void saveToSettings();

    // 全局运动控制参数 (当前配置值，受 metadata min/max 限制，图元未修改时以这些值为默认值)
    double m_maxSpeed                  = 0.5;   // m/s
    double m_maxAcceleration           = 0.4;   // m/s²
    double m_maxDeceleration           = 0.4;   // m/s²
    double m_maxAngularSpeedDeg        = 50.0;  // °/s
    double m_maxAngularAccelerationDeg = 20.0;  // °/s²
    double m_arrivalDistanceTolerance  = 0.1;   // m
    double m_arrivalAngleToleranceDeg  = 0.5;   // °
    double m_routeWidth                = 4.0;   // m

    // 默认避障参数 (来自“配置参数”中“障碍物配置”的 id_stop_range_threshold 与 id_deceleration_range_threshold)
    double m_defaultObsStopLength      = 0.1;   // m (纵向停车扩展量 dx)
    double m_defaultObsStopWidth       = 0.1;   // m (横向停车扩展量 dy)
    double m_defaultObsDecelLength     = 0.5;   // m (纵向减速扩展量 dx)
    double m_defaultObsDecelWidth      = 0.3;   // m (横向减速扩展量 dy)

    QMap<QString, MotionParamMeta> m_paramMetas;
};
