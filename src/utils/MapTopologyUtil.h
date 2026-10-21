/**
 * @file    MapTopologyUtil.h
 * @layer   Utils
 * @brief   AABB boundary extraction and workstation point classification.
 * @deps    QJsonObject, QJsonArray
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QJsonObject>
#include <QJsonArray>

/**
 * @brief 拓扑区域包围盒提取与工作站属性校验实用类
 */
class MapTopologyUtil final
{
public:
    MapTopologyUtil() = delete;
    ~MapTopologyUtil() = delete;
    MapTopologyUtil(const MapTopologyUtil &) = delete;
    MapTopologyUtil &operator=(const MapTopologyUtil &) = delete;

    /**
     * @brief 提取高级区域的 AABB 轴对齐包围盒边界
     */
    static void extractAreaBounds(const QJsonObject &obj, double &minX, double &maxX, double &minY, double &maxY);

    /**
     * @brief 严格判断站点是否为特殊工作站点 (高精度工作站或充电桩)
     */
    static bool isSpecialWorkstationPoint(const QJsonObject &obj);

    /**
     * @brief 强制将特殊工作站/工作站的四个避障参数全部置 0 (平铺字段及 actions 数组)
     */
    static void forceZeroObstacleParams(QJsonObject &obj);
};
