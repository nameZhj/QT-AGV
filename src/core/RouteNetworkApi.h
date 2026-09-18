#pragma once

#include <QString>
#include <QPointF>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <functional>

// ============================================================================
//  路网数据模型 (严格兼容原有数据格式，全面补充工业级运动与调度参数)
// ============================================================================

/// 站点类型
enum class PointType {
    Point,          ///< 普通站点 (LM, 对应 "point")
    Charge,         ///< 充电桩 (CP, 对应 "charge")
    HighPrecision   ///< 精确定位点 (TL, 对应 "highPrecision")
};

/// 线路通行方向 (100% 兼容原有约定: 10=双向, 0=正向, 1=反向, 2=兼容双向)
enum class RouteDirection {
    Forward          = 0,  ///< 单向前进 (→)
    Backward         = 1,  ///< 单向后退 (←)
    Bidirectional    = 10, ///< 双向通行 (↔)
    BidirectionalAlt = 2   ///< 双向通行 (兼容旧版协议)
};

/// 导航与运动方式
enum class MoveStyle {
    PathNavigation     = 0, ///< 路径导航 (默认)
    FreeNavigation     = 1, ///< 自由导航 / 局部避障
    InfraredGuidance   = 3, ///< 红外引导
    MagneticStrip      = 4, ///< 磁条引导
    QRCodeNavigation   = 5, ///< 二维码导航
    RailGuided         = 6  ///< 固定轨道导航
};

/// 站点数据实体
struct RoutePoint {
    QString id;                 ///< 站点唯一标识 (如 "p_1", "c_1", "hp_1")
    QString name;               ///< 站点显示名称 (如 "P1", "LM1", "CP1")
    QString type = "point";     ///< "point" | "charge" | "highPrecision"
    double x = 0.0;             ///< 世界坐标 X (m)
    double y = 0.0;             ///< 世界坐标 Y (m)
    double yaw = 0.0;           ///< 车头停靠朝向角 (rad)
    bool rdsDockable = false;   ///< 允许停靠
    bool allowRotate = false;   ///< 允许原地自转
    bool useDownPGV  = false;   ///< 启用下视二维码/PGV标定
    QString epc;                ///< EPC/RFID 标签值
    bool isyawfix = false;      ///< 是否固定车头角度
    int taskid = 0;             ///< 绑定任务ID

    // 扩展参数 (存储于 GeoJSON properties 中)
    bool ignoreDir = false;     ///< 是否忽略车头姿态
    QString label;              ///< 业务附加标签
    double waitTimeout = 0.0;   ///< 站点等待超时时间 (s)

    // 原生 JSON 序列化与反序列化
    QJsonObject toJson() const;
    static RoutePoint fromJson(const QJsonObject &obj);
};

/// 线路 / 贝塞尔曲线数据实体
struct RouteSegment {
    QString id;                 ///< 唯一编号 (如 "l_1", "b_1")
    QString name;               ///< 线路名称 (如 "L1", "曲1")
    QString type = "line";      ///< "line" | "curve"
    QString startId;            ///< 起点站点 ID (对应 sId / startid)
    QString endId;              ///< 终点站点 ID (对应 eId / endid)
    int direct = 10;            ///< 通行方向: 10=双向(↔), 0=正向前进(→), 1=反向后退(←)

    // 几何参数 (直线: x1,y1 -> x2,y2; 曲线: controlPoints)
    QPointF startPos;           ///< 起点坐标 (m)
    QPointF endPos;             ///< 终点坐标 (m)
    QList<QPointF> controlPts;  ///< 贝塞尔控制点列表 (包含起终点及中间控制点)

    // 动力学与运动控制参数 (全面对齐工业级 AGV 参数标准)
    double maxSpeed    = 0.50;  ///< 最大线速度 (m/s)
    double minSpeed    = 0.00;  ///< 最小线速度 (m/s)
    double maxAcc      = 0.30;  ///< 最大加速度 (m/s²)
    double maxDec      = 0.30;  ///< 最大减速度 (m/s²)
    double maxRotSpeed = 60.0;  ///< 最大角速度 (°/s)
    double maxRotAcc   = 45.0;  ///< 最大角加速度 (°/s²)
    double reachDist   = 0.05;  ///< 到点距离容差 (m)
    double reachAngle  = 2.00;  ///< 到点角度容差 (°)
    double roadWidth   = 1.00;  ///< 通道宽度 (m)

    // 高级调度与导航扩展参数 (存储于 GeoJSON properties 中)
    int moveStyle      = 0;     ///< 导航方式 (0: 路径导航, 1: 自由导航, ...)
    bool locked        = false; ///< 是否临时闭塞/禁行
    double weight      = 1.00;  ///< 拓扑规划权重代价系数
    int priority       = 0;     ///< 调度通行优先级
    bool spinTurn      = true;  ///< 是否允许在端点原地自转
    bool avoidObstacle = true;  ///< 是否允许局部避障

    // 原生 JSON 序列化与反序列化
    QJsonObject toJson() const;
    static RouteSegment fromJson(const QJsonObject &obj);

    /// 便捷方向判定
    bool isBidirectional() const { return direct == 10 || direct == 2; }
    bool hasForward() const      { return direct == 0 || isBidirectional(); }
    bool hasBackward() const     { return direct == 1 || isBidirectional(); }
};

/// 展开后的拓扑有向边 (供 A* / Dijkstra 寻路以及调度引擎直接使用)
struct DirectedEdge {
    QString edgeId;             ///< 边标识 (如 "l_1_fwd", "l_1_rev")
    QString sourcePointId;      ///< 起点 ID
    QString targetPointId;      ///< 终点 ID
    double lengthMeters = 0.0;  ///< 物理弧长或线长 (m)
    double maxSpeed     = 0.5;  ///< 限速 (m/s)
    double weight       = 1.0;  ///< 规划权重
    bool isReverse      = false;///< 是否是由双向路线逆向衍生出的有向边
    QList<QPointF> pathPoints;  ///< 路径插值点点串
};

/// 完整路网数据容器 (内存标准模型)
struct RouteNetwork {
    QString mapId;
    QString mapName;
    QList<RoutePoint>   points;              ///< 普通站点 (LM)
    QList<RoutePoint>   charges;             ///< 充电站点 (CP)
    QList<RoutePoint>   highPrecisionPoints; ///< 精确标定点 (TL)
    QList<RouteSegment> lines;               ///< 直线段
    QList<RouteSegment> curves;              ///< 贝塞尔曲线段

    void clear() {
        points.clear();
        charges.clear();
        highPrecisionPoints.clear();
        lines.clear();
        curves.clear();
    }

    bool isEmpty() const {
        return points.isEmpty() && charges.isEmpty() && highPrecisionPoints.isEmpty() && lines.isEmpty() && curves.isEmpty();
    }
};

// ============================================================================
//  路网 API 服务类 (纯静态无状态算法工具库)
// ============================================================================
class RouteNetworkApi final
{
public:
    RouteNetworkApi() = delete;
    ~RouteNetworkApi() = delete;
    RouteNetworkApi(const RouteNetworkApi &) = delete;
    RouteNetworkApi &operator=(const RouteNetworkApi &) = delete;

    // 1. 原生 GeoJSON 数据格式互转 (100% 保持与原有格式一致)
    static RouteNetwork fromGeoJson(const QJsonObject &root);
    static QJsonObject toGeoJson(const RouteNetwork &network);

    // 2. 拓扑有向图展开算法
    static QList<DirectedEdge> toDirectedGraph(const RouteNetwork &network);

    // 3. 工业级 .smap 地图格式导入与导出
    static bool parseSmap(const QJsonObject &smapObj, RouteNetwork &outNetwork);
    static QJsonObject exportSmap(const RouteNetwork &network);

    // 4. 路网拓扑与参数完整性校验
    static bool validate(const RouteNetwork &network, QStringList &warnings, QStringList &errors);

    // 5. 异步网络服务 API (基于底层 ApiService 封装)
    using LoadCallback = std::function<void(bool success, const RouteNetwork &network, const QString &errorMsg)>;
    using SaveCallback = std::function<void(bool success, const QString &message)>;

    static void loadNetwork(const QString &mapId, LoadCallback cb);
    static void saveNetwork(const QString &mapId, const QString &mapName, const RouteNetwork &network, SaveCallback cb);
};
