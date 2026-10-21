/**
 * @file    MapPgmUtil.h
 * @layer   Utils
 * @brief   Binary PGM/YAML map loader, Bezier geometry, and SMAP route utilities.
 * @deps    MapPgmCodec.h, MapSmapParser.h, QImage, QPointF, QPolygonF, QPainterPath
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include "MapPgmCodec.h"
#include "MapTopologyUtil.h"

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
 * @brief PGM 图像编解码、ROS YAML 解析、贝塞尔几何算法及路网图元生成实用工具类
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

    // ==============================================================================
    // === [Section: Geometric & Physical Constants] ===
    // ==============================================================================
    static constexpr double kDefaultResolution            = 0.05;  ///< 默认分辨率 (米/像素)
    static constexpr double kRobotPhysicalLengthMeters    = 0.50;  ///< 车辆物理长 (米)
    static constexpr double kRobotPhysicalWidthMeters     = 0.30;  ///< 车辆物理宽 (米)
    static constexpr int    kDefaultBezierSampleSteps     = 100;   ///< 贝塞尔离散采样步数
    static constexpr double kCurveMergeToleranceMeters    = 0.05;  ///< 对称曲线智能合并容差 (5cm)

    // 默认拓扑曲线动力学与调度参数
    static constexpr double kDefaultCurveMaxSpeed         = 8.0;   ///< 默认曲线最大线速度 (m/s)
    static constexpr double kDefaultCurveMinSpeed         = 0.0;   ///< 默认曲线最小线速度 (m/s)
    static constexpr double kDefaultCurveMaxAcc           = 0.3;   ///< 默认曲线最大加速度 (m/s²)
    static constexpr double kDefaultCurveMaxRotSpeed      = 60.0;  ///< 默认曲线最大角速度 (°/s)
    static constexpr double kDefaultCurveMaxRotAcc        = 45.0;  ///< 默认曲线最大角加速度 (°/s²)
    static constexpr double kDefaultCurveSlowDistance     = 2.0;   ///< 默认曲线减速距离 (m)

    // ==============================================================================
    // === [Section: PGM Binary/Text Image Codec] ===
    // ==============================================================================

    static QImage pgmToImage(const QByteArray &pgmData, bool negate = false) {
        return MapPgmCodec::pgmToImage(pgmData, negate);
    }

    static QByteArray imageToPgm(const QImage &image, bool negate = false) {
        return MapPgmCodec::imageToPgm(image, negate);
    }

    // ==============================================================================
    // === [Section: ROS map_server YAML Parser] ===
    // ==============================================================================

    static MapYamlInfo parseMapYaml(const QString &yamlText) {
        return MapPgmCodec::parseMapYaml(yamlText);
    }

    static bool parseMapYaml(const QString &yamlText,
                             double &resolution,
                             double &originX,
                             double &originY,
                             bool &negate,
                             QString *imageFile = nullptr) {
        return MapPgmCodec::parseMapYaml(yamlText, resolution, originX, originY, negate, imageFile);
    }

    // ==============================================================================
    // === [Section: Bezier & Tangent Geometry Engine] ===
    // ==============================================================================

    /**
     * @brief 评估任意阶贝塞尔曲线在参数 t 处的坐标值 (De Casteljau 算法)
     */
    static QPointF evaluateBezier(const QList<QPointF> &controlPoints, double t);

    /**
     * @brief 生成贝塞尔曲线离散渲染路径
     */
    static QPainterPath getBezierPath(const QList<QPointF> &controlPoints);

    /**
     * @brief 生成直线段方向指示箭头多边形列表
     */
    static QList<QPolygonF> createDirectionArrows(const QPointF &mid,
                                                  const QPointF &dirUnit,
                                                  int direct,
                                                  double lineLen = 100.0);

    /**
     * @brief 生成贝塞尔曲线方向指示箭头多边形列表 (严格沿曲线切线方向评估)
     */
    static QList<QPolygonF> createBezierDirectionArrows(const QList<QPointF> &controlPoints,
                                                        int direct,
                                                        double curveLen = 100.0);

    /**
     * @brief 生成路网点图元绘制路径 (长方形车体投影 + 中心定位十字)
     */
    static QPainterPath createPointPath(double pxPerMeterX,
                                        double pxPerMeterY,
                                        double *outW = nullptr,
                                        double *outH = nullptr,
                                        QPainterPath *outCrossPath = nullptr,
                                        double minWidth = 0.0);

    // ==============================================================================
    // === [Section: SMAP Map Import & Curve Merging] ===
    // ==============================================================================

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

    // ==============================================================================
    // === [Section: Topology & Area Bound Extraction] ===
    // ==============================================================================

    static void extractAreaBounds(const QJsonObject &obj, double &minX, double &maxX, double &minY, double &maxY) {
        MapTopologyUtil::extractAreaBounds(obj, minX, maxX, minY, maxY);
    }

    static bool isSpecialWorkstationPoint(const QJsonObject &obj) {
        return MapTopologyUtil::isSpecialWorkstationPoint(obj);
    }

    static void forceZeroObstacleParams(QJsonObject &obj) {
        MapTopologyUtil::forceZeroObstacleParams(obj);
    }
};
