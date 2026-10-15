#ifndef MAPPGMUTIL_H
#define MAPPGMUTIL_H

#include <QString>
#include <QByteArray>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>
#include <QPolygonF>
#include <QList>
#include <QPainterPath>

/**
 * @brief ROS Map Server YAML 解析结果结构体
 */
struct MapYamlInfo
{
    double  resolution = 0.05;   ///< 地图分辨率 (米/像素)
    double  originX    = 0.0;    ///< 原点 X 坐标 (米)
    double  originY    = 0.0;    ///< 原点 Y 坐标 (米)
    double  originYaw  = 0.0;    ///< 原点偏航角 (弧度)
    bool    negate     = false;  ///< 灰度反转标志 (true 表示黑色为自由空间，白色为障碍物)
    QString imageFile;           ///< 关联的 PGM 图像文件名
    bool    isValid    = false;  ///< 解析是否成功且包含关键字段
};

/**
 * @brief PGM 图像编解码、ROS YAML 解析、贝塞尔几何算法及路网图元生成实用工具类
 * 
 * 架构规范：
 * 1. 纯静态算法工具库，显式禁用对象实例化；
 * 2. 封装 PGM(P5/P2) 二进制及文本格式的高性能像素转换；
 * 3. 几何数学引擎：任意阶 De Casteljau 贝塞尔曲线求值与切线箭头生成；
 * 4. 消除所有硬编码魔数，统一物理几何比例与容差约束；
 * 5. 全面保持 API 向下兼容。
 */
class MapPgmUtil final
{
public:
    MapPgmUtil() = delete;
    ~MapPgmUtil() = delete;
    MapPgmUtil(const MapPgmUtil &) = delete;
    MapPgmUtil &operator=(const MapPgmUtil &) = delete;

    /**
     * @brief 路网线路通行方向枚举
     */
    enum class Direction : int
    {
        Forward          = 0,  ///< 单向前进 (与线路定义顺序一致)
        Backward         = 1,  ///< 单向后退 (与线路定义顺序相反)
        Bidirectional    = 10, ///< 双向通行 (标准路网约定)
        BidirectionalAlt = 2   ///< 双向通行 (兼容旧版协议)
    };

    // ── 几何与物理比例常量 ──
    static constexpr double kDefaultResolution            = 0.05;  ///< 默认分辨率 (米/像素)
    static constexpr double kRobotPhysicalLengthMeters    = 0.50;  ///< 车辆物理长 (米)
    static constexpr double kRobotPhysicalWidthMeters     = 0.30;  ///< 车辆物理宽 (米)
    static constexpr int    kDefaultBezierSampleSteps     = 100;   ///< 贝塞尔离散采样步数
    static constexpr double kCurveMergeToleranceMeters    = 0.05;  ///< 对称曲线智能合并容差 (5cm)

    // ── 1. PGM 图像编解码 ──

    /**
     * @brief 将 PGM(P5 二进制 / P2 文本) 字节流转换为 QImage(Format_Grayscale8)
     * @param pgmData PGM 原始文件字节流
     * @param negate 是否执行灰度反转（与 ROS map_server yaml 的 negate 一致）
     * @return 转换后的灰度图像，若数据不合法则返回空 QImage
     */
    static QImage pgmToImage(const QByteArray &pgmData, bool negate = false);

    /**
     * @brief 将灰度图像编码为标准 PGM(P5 二进制) 字节流
     * @param image 待编码图像（自动转换为 8 位灰度）
     * @param negate 是否执行灰度反转
     * @return PGM 二进制数据包
     */
    static QByteArray imageToPgm(const QImage &image, bool negate = false);

    // ── 2. ROS map_server YAML 解析 ──

    /**
     * @brief 解析 ROS map_server 风格的 YAML 文本为结构体
     * @param yamlText YAML 文本内容
     * @return 结构化解析结果
     */
    static MapYamlInfo parseMapYaml(const QString &yamlText);

    /**
     * @brief 解析 ROS map_server 风格的 YAML 文本（兼容原有引用参数 API）
     */
    static bool parseMapYaml(const QString &yamlText,
                             double &resolution,
                             double &originX,
                             double &originY,
                             bool &negate,
                             QString *imageFile = nullptr);

    // ── 3. 曲线与几何计算 ──

    /**
     * @brief 评估任意阶贝塞尔曲线在参数 t 处的坐标值 (De Casteljau 算法)
     * @param controlPoints 控制点列表 (包含起始点、中间控制点、终止点)
     * @param t 插值参数 [0.0, 1.0]
     * @return 对应参数处的空间坐标
     */
    static QPointF evaluateBezier(const QList<QPointF> &controlPoints, double t);

    /**
     * @brief 生成贝塞尔曲线离散渲染路径
     * @param controlPoints 控制点列表
     * @return QPainterPath 绘制路径
     */
    static QPainterPath getBezierPath(const QList<QPointF> &controlPoints);

    /**
     * @brief 生成直线段方向指示箭头多边形列表
     * @param mid 线段中点世界坐标
     * @param dirUnit 线段单位切线方向向量
     * @param direct 通行方向 (0: 前进, 1: 后退, 10/2: 双向)
     * @param lineLen 线段几何长度 (用于箭头尺寸自适应缩放)
     * @return 箭头多边形列表
     */
    static QList<QPolygonF> createDirectionArrows(const QPointF &mid,
                                                  const QPointF &dirUnit,
                                                  int direct,
                                                  double lineLen = 100.0);

    /**
     * @brief 生成贝塞尔曲线方向指示箭头多边形列表 (严格沿曲线切线方向评估)
     * @param controlPoints 贝塞尔控制点列表
     * @param direct 通行方向 (0: 前进, 1: 后退, 10/2: 双向)
     * @param curveLen 曲线弦长或近似弧长
     * @return 贴合曲线曲率的箭头多边形列表
     */
    static QList<QPolygonF> createBezierDirectionArrows(const QList<QPointF> &controlPoints,
                                                        int direct,
                                                        double curveLen = 100.0);

    /**
     * @brief 生成路网点图元绘制路径 (长方形车体投影 + 中心定位十字)
     * @param pxPerMeterX X 方向像素/米比例
     * @param pxPerMeterY Y 方向像素/米比例
     * @param outW 可选输出长方形图元像素宽度
     * @param outH 可选输出长方形图元像素高度
     * @param outCrossPath 可选输出中心十字定位路径
     * @param minWidth 最小显示像素宽度限制
     * @return 外围长方形轮廓路径
     */
    static QPainterPath createPointPath(double pxPerMeterX,
                                        double pxPerMeterY,
                                        double *outW = nullptr,
                                        double *outH = nullptr,
                                        QPainterPath *outCrossPath = nullptr,
                                        double minWidth = 0.0);

    // ── 4. .smap 路网文件解析与转换 ──

    /**
     * @brief 解析 .smap 格式文件，提取路网点、线段、贝塞尔曲线并自动归并对称反向曲线
     */
    static bool parseSmapFile(const QString &filePath,
                              QJsonArray &outPoints,
                              QJsonArray &outLines,
                              QJsonArray &outCurves,
                              QString &outMapName,
                              QString *outError = nullptr,
                              QJsonObject *outHeader = nullptr);

    /**
     * @brief 解析 .smap JSON 数据对象
     */
    static bool parseSmapJson(const QJsonObject &smapObj,
                              QJsonArray &outPoints,
                              QJsonArray &outLines,
                              QJsonArray &outCurves,
                              QString &outMapName,
                              QString *outError = nullptr,
                              QJsonObject *outHeader = nullptr);
};

#endif // MAPPGMUTIL_H
