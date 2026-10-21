/**
 * @file    RouteNetworkValidator.h
 * @module  CoreTopology
 * @brief   Topological map route network and advanced area collision/parameter validator.
 * @deps    MapCheckResultDialog.h, MotionConfigManager.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include "ui/dialogs/MapCheckResultDialog.h"

class MotionConfigManager;

namespace RouteNetworkValidator {

/**
 * @brief 提取高级区域的 AABB 包围盒范围
 */
void extractAreaBounds(const QJsonObject &obj, double &minX, double &maxX, double &minY, double &maxY);

/**
 * @brief 执行路网拓扑合规性与冲突校验
 * @param highLevelAreas 高级区域图元列表
 * @param lines 直线列表
 * @param curves 贝塞尔曲线列表
 * @param mc 底盘运动配置管理器实例引用
 * @param results 输出的校验结果项集合
 * @return 如果包含错误级别项则返回 false，否则返回 true
 */
bool validate(const QJsonArray &highLevelAreas,
              const QJsonArray &lines,
              const QJsonArray &curves,
              const MotionConfigManager &mc,
              QList<CheckResultItem> &results);

/**
 * @brief 将线路与曲线避障参数向两端普通端点同步，并在特殊工作站/充电桩强制归零
 */
void syncLineObstaclesToEndpoints(QJsonArray &points,
                                  QJsonArray &lines,
                                  QJsonArray &curves,
                                  QJsonArray &charges,
                                  QJsonArray &highPrecisionPoints);

/**
 * @brief 将内存中路网图元组装为标准 GeoJSON FeatureCollection
 */
QJsonObject buildRouteGeoJson(const QString &mapId,
                              const QJsonArray &points,
                              const QJsonArray &lines,
                              const QJsonArray &curves,
                              const QJsonArray &charges,
                              const QJsonArray &highPrecisionPoints,
                              QJsonArray &highLevelAreas);

} // namespace RouteNetworkValidator
