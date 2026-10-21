#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QJsonObject>
#include <QReadWriteLock>
#include <cmath>

/**
 * @brief 里程信息 (未收到或超时则 valid = false)
 */
struct MileageInfo {
    bool   valid            = false;
    double total_distance_m = 0.0;   ///< 累计行驶里程，米
    double today_distance_m = 0.0;   ///< 今日行驶里程，米
};

/**
 * @brief 运行时间信息 (未收到或超时则 valid = false)
 */
struct RuntimeInfo {
    bool   valid               = false;
    qint64 current_runtime_sec = 0;  ///< 本次统计节点运行时间，秒
    qint64 total_runtime_sec   = 0;  ///< 累计统计节点运行时间，秒，包含本次
};

/**
 * @brief 障碍物坐标点
 */
struct ObstaclePoint {
    double x = 0.0;                  ///< 障碍点 X 坐标，米，沿用检测节点坐标系
    double y = 0.0;                  ///< 障碍点 Y 坐标，米，沿用检测节点坐标系
};

/**
 * @brief 障碍信息 (未收到或超时则 valid = false)
 */
struct ObstacleInfo {
    bool          valid    = false;
    bool          hasPoint = false;
    ObstaclePoint point;             ///< 最近有效障碍点
    bool          blocked  = false;  ///< true：触发障碍检测；false：未触发
};

/**
 * @brief 速度反馈与指令速度 (vx, vy 线速度 m/s, wz 角速度 rad/s)
 */
struct VelocityInfo {
    bool   valid = false;
    double vx    = 0.0;              ///< X 方向速度，m/s
    double vy    = 0.0;              ///< Y 方向速度，m/s
    double wz    = 0.0;              ///< 绕 Z 轴角速度，rad/s
};

/**
 * @brief 车辆核心数据模型 (遵循“有则赋值，无则沿用原逻辑”原则)
 */
struct RobotInfo {
    // 基础标识
    int     car_id   = 0;
    QString carIdStr = "1";
    int     map_id   = 0;
    QString mapIdStr;

    // 位姿信息
    bool   hasPose  = false;
    double car_x    = 0.0;
    double car_y    = 0.0;
    double car_z    = 0.0;
    double car_yaw  = 0.0;           ///< 弧度制

    // 状态与任务
    // MQTT Topic: /rob/navigate/status -> {"status":"0"} 就绪, {"status":"1"} 导航中, {"status":"2"} 暂停导航
    // 刚开机默认初始化为 "0" (就绪)
    QString status  = "0";
    int     task_id = -1;            ///< 小车正在执行的任务id，若无则为 -1
    bool    receivedData = false;    ///< 是否从 MQTT 接收到过真实有效报文

    // 电池电量 (0.0 ~ 1.0 或百分比，例如 0.98 表示 98%)
    double battery = -1.0;

    // 机台定位与定位末端坐标 (EPC: end-effector position coordinates)
    bool   hasTheta  = false;
    double theta     = 0.0;          ///< 机台入口对应的世界坐标角度，角度值 (degrees)
    bool   hasEpc    = false;
    double epc_x     = 0.0;          ///< 机台定位末端x坐标，单位米
    double epc_y     = 0.0;          ///< 末端y坐标，单位米
    double epc_theta = 0.0;          ///< 末端角度，弧度制

    // 嵌套统计与传感器信息
    MileageInfo  mileage;
    RuntimeInfo  runtime;
    ObstacleInfo obstacle;
    VelocityInfo actual_vel;         ///< /odom 反馈速度，实际速度
    VelocityInfo cmd_vel;            ///< /cmd_vel_safe 最终指令，发送速度

    // 补充与向下兼容字段
    QString carName;
    double  covScore = -1.0;         ///< 定位置信度
    int     latency  = -1;           ///< 通讯延迟 (ms)
    QString fwVersion;
    QString remark;

    // ── 便捷访问方法 ──
    int batteryPercent() const {
        if (battery < 0.0) return -1;
        if (battery <= 1.0) return static_cast<int>(std::round(battery * 100.0));
        return static_cast<int>(std::round(battery));
    }

    bool isOnline() const {
        return receivedData && !status.isEmpty();
    }

    bool isNavigating() const {
        return isOnline() && (status == "1" || status == "2" || status == "3" || status == "5");
    }

    QString statusText() const {
        if (!isOnline()) return "未连接";
        if (status == "0") return "就绪";
        if (status == "1") return "导航中";
        if (status == "2") return "暂停导航";
        if (status == "3") return "导航中";
        if (status == "4") return "停止导航，作业中";
        if (status == "5") return "暂停导航";
        if (status == "6") return "故障";
        return status.isEmpty() ? "就绪" : QString("状态[%1]").arg(status);
    }

    QString navStatusText() const {
        if (status == "1" || status == "3") return "导航中";
        if (status == "2" || status == "5") return "暂停导航";
        if (status == "4") return "停止导航，作业中";
        if (status == "0") return "就绪";
        if (status == "6") return "故障";
        return "就绪";
    }

    QString statusColor() const {
        if (status == "0") return "#22c55e"; // 绿色 (就绪)
        if (status == "1" || status == "3") return "#3b82f6"; // 蓝色 (导航中)
        if (status == "2" || status == "5") return "#f59e0b"; // 黄色 (暂停导航)
        if (status == "4") return "#06b6d4"; // 青色
        if (status == "6") return "#ef4444"; // 红色 (故障)
        return "#22c55e";
    }

    double actualLinearSpeed() const {
        if (!actual_vel.valid) return 0.0;
        return std::hypot(actual_vel.vx, actual_vel.vy);
    }

    double cmdLinearSpeed() const {
        if (!cmd_vel.valid) return 0.0;
        return std::hypot(cmd_vel.vx, cmd_vel.vy);
    }
};

/**
 * @brief 全局车辆信息总线管理器 (Thread-Safe Reactive Singleton)
 * 
 * 职责：
 * 1. 自动常驻监听全局 MQTT 中的标准 /info、/rob/+/info、/rob/info 话题；
 * 2. 集中解析最新 info 协议结构，对缺失或 null 字段采取“增量更新、保留原值”策略；
 * 3. 维护多车及当前主控车辆的数据仓库，提供响应式信号与线程安全读写接口；
 * 4. 提供通用解析与时间、状态格式化静态工具函数。
 */
class RobotInfoManager final : public QObject
{
    Q_OBJECT

public:
    static RobotInfoManager &instance();

    // 初始化常驻 MQTT 订阅与信号绑定
    void init();
    // 切换并重置单车连接 (清理旧车数据并根据 IP/CarID 同步修改全局订阅)
    void resetForCar(const QString &newCarId);
    // 全局常驻核心 MQTT 订阅生效与同步
    void subscribeGlobalTopics();

    // 数据查询 (线程安全)
    RobotInfo currentInfo() const;
    RobotInfo infoForCar(const QString &carId) const;
    QMap<QString, RobotInfo> allRobotInfos() const;
    bool hasInfoForCar(const QString &carId) const;

    // 核心解析与分发 (外部亦可直接喂入特定设备 MQTT 收到的 payload)
    bool updateFromPayload(const QString &topic, const QByteArray &payload, QString *outCarId = nullptr);
    // 处理 /rob/navigate/status MQTT 状态消息 {"status":"0"就绪, "status":"1"导航中}
    bool updateNavigateStatus(const QByteArray &payload);

    // 静态工具：复用解析单个 JSON 对象到目标 RobotInfo 中
    static bool parseInfoJson(const QJsonObject &d, RobotInfo *outInfo);

    // 静态工具：将秒数格式化为 "XX天/XX时/XX分/XX秒"
    static QString formatDuration(qint64 totalSeconds);

signals:
    // 任意车辆更新信号
    void robotInfoUpdated(const QString &carId, const RobotInfo &info);
    // 当前主控设备更新信号
    void currentRobotInfoUpdated(const RobotInfo &info);

private slots:
    void onMqttConnected();
    void onMqttDisconnected();

private:
    explicit RobotInfoManager(QObject *parent = nullptr);
    ~RobotInfoManager() override = default;
    RobotInfoManager(const RobotInfoManager &) = delete;
    RobotInfoManager &operator=(const RobotInfoManager &) = delete;

    mutable QReadWriteLock   m_lock;
    QMap<QString, RobotInfo> m_robots;
    bool                     m_initialized = false;
};
