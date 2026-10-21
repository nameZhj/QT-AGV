/**
 * @file    RouteNetworkValidator.cpp
 * @module  CoreTopology
 * @brief   Implementation of map route topology and area constraint validator.
 * @deps    RouteNetworkValidator.h, RouteObstacleSync.h, RouteGeoJsonBuilder.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "RouteNetworkValidator.h"
#include "RouteObstacleSync.h"
#include "RouteGeoJsonBuilder.h"
#include "core/MotionConfigManager.h"
#include "utils/MapPgmUtil.h"
#include <cmath>
#include <QPointF>

namespace RouteNetworkValidator {

namespace {
constexpr double kTolerance = 1e-4;
constexpr int    kCurveSamples = 10;
}

// === [Section: Geometry & Bound Extraction] ===

void extractAreaBounds(const QJsonObject &obj, double &minX, double &maxX, double &minY, double &maxY)
{
    MapPgmUtil::extractAreaBounds(obj, minX, maxX, minY, maxY);
}

// === [Section: Parameter Schema Definitions] ===

struct ParamDef {
    QString propKey;
    QString jsonKey1;
    QString jsonKey2;
    QString displayName;
};

static const ParamDef kDefs[] = {
    {QStringLiteral("maxspeed"),                     QStringLiteral("maxspeed"),                     QStringLiteral("max_speed"),    QStringLiteral("maxspeed")},
    {QStringLiteral("maxacc"),                       QStringLiteral("max_acceleration"),             QStringLiteral("maxacc"),       QStringLiteral("maxacc")},
    {QStringLiteral("maxrot"),                       QStringLiteral("max_angular_speed"),            QStringLiteral("maxrot"),       QStringLiteral("maxrot")},
    {QStringLiteral("maxrotacc"),                    QStringLiteral("max_angular_acceleration"),     QStringLiteral("maxrotacc"),    QStringLiteral("maxrotacc")},
    {QStringLiteral("reachdist"),                    QStringLiteral("arrival_distance_tolerance"),   QStringLiteral("reachdist"),    QStringLiteral("reachdist")},
    {QStringLiteral("reachangle"),                   QStringLiteral("arrival_angle_tolerance"),      QStringLiteral("reachangle"),   QStringLiteral("reachangle")},
    {QStringLiteral("width"),                        QStringLiteral("route_width"),                  QStringLiteral("width"),        QStringLiteral("width")},
    {QStringLiteral("intrusion_stop_length"),        QStringLiteral("intrusion_stop_length"),        QString(),                      QStringLiteral("intrusion_stop_length")},
    {QStringLiteral("intrusion_stop_width"),         QStringLiteral("intrusion_stop_width"),         QString(),                      QStringLiteral("intrusion_stop_width")},
    {QStringLiteral("intrusion_deceleration_length"),QStringLiteral("intrusion_deceleration_length"),QString(),                      QStringLiteral("intrusion_deceleration_length")},
    {QStringLiteral("intrusion_deceleration_width"), QStringLiteral("intrusion_deceleration_width"), QString(),                      QStringLiteral("intrusion_deceleration_width")}
};

static bool isParamEnabled(const QJsonObject &obj, const QString &propKey, const QString &k1, const QString &k2)
{
    if (obj.contains(QStringLiteral("checked_props"))) {
        const QJsonArray cp = obj.value(QStringLiteral("checked_props")).toArray();
        for (const auto &v : cp) {
            if (v.toString() == propKey) return true;
        }
        return false;
    }
    return obj.contains(k1) || (!k2.isEmpty() && obj.contains(k2));
}

static double getParamValue(const QJsonObject &obj, const QString &k1, const QString &k2)
{
    if (obj.contains(k1)) return obj.value(k1).toDouble();
    if (!k2.isEmpty() && obj.contains(k2)) return obj.value(k2).toDouble();
    return 0.0;
}

// === [Section: Intersection Calculations] ===

static bool lineIntersectsAABB(double x1, double y1, double x2, double y2,
                              double minX, double maxX, double minY, double maxY)
{
    if (x1 >= minX && x1 <= maxX && y1 >= minY && y1 <= maxY) return true;
    if (x2 >= minX && x2 <= maxX && y2 >= minY && y2 <= maxY) return true;

    auto ccw = [](double ax, double ay, double bx, double by, double cx, double cy) {
        return (cy - ay) * (bx - ax) > (by - ay) * (cx - ax);
    };
    auto segIntersect = [&](double ax, double ay, double bx, double by, double cx, double cy, double dx, double dy) {
        return (ccw(ax, ay, cx, cy, dx, dy) != ccw(ax, ay, bx, by, cx, cy)) &&
               (ccw(ax, ay, bx, by, cx, cy) != ccw(ax, ay, bx, by, dx, dy));
    };

    return segIntersect(x1, y1, x2, y2, minX, minY, maxX, minY) ||
           segIntersect(x1, y1, x2, y2, maxX, minY, maxX, maxY) ||
           segIntersect(x1, y1, x2, y2, maxX, maxY, minX, maxY) ||
           segIntersect(x1, y1, x2, y2, minX, maxY, minX, minY);
}

static bool curveIntersectsAABB(const QJsonObject &cv, double minX, double maxX, double minY, double maxY)
{
    QList<QPointF> cPts;
    const QJsonArray ctrlArr = cv.value(QStringLiteral("controlPoints")).toArray();
    if (!ctrlArr.isEmpty()) {
        for (const auto &pv : ctrlArr) {
            const QJsonObject pt = pv.toObject();
            cPts.append(QPointF(pt.value(QStringLiteral("x")).toDouble(), pt.value(QStringLiteral("y")).toDouble()));
        }
    } else {
        cPts.append(QPointF(cv.value(QStringLiteral("x1")).toDouble(), cv.value(QStringLiteral("y1")).toDouble()));
        cPts.append(QPointF(cv.value(QStringLiteral("cx")).toDouble(), cv.value(QStringLiteral("cy")).toDouble()));
        cPts.append(QPointF(cv.value(QStringLiteral("x2")).toDouble(), cv.value(QStringLiteral("y2")).toDouble()));
    }
    if (cPts.size() < 2) return false;

    QPointF prevPt = cPts.first();
    for (int s = 1; s <= kCurveSamples; ++s) {
        const double t = static_cast<double>(s) / kCurveSamples;
        const QPointF curPt = (cPts.size() == 3)
            ? ((1.0 - t) * (1.0 - t) * cPts[0] + 2.0 * (1.0 - t) * t * cPts[1] + t * t * cPts[2])
            : ((1.0 - t) * cPts.first() + t * cPts.last());

        if (lineIntersectsAABB(prevPt.x(), prevPt.y(), curPt.x(), curPt.y(), minX, maxX, minY, maxY)) {
            return true;
        }
        prevPt = curPt;
    }
    return false;
}

static void appendSpeedMismatchWarning(const QString &areaName, const QJsonObject &edgeObj,
                                       const QString &targetType, QList<CheckResultItem> &results)
{
    QString sId = edgeObj.value(QStringLiteral("startid")).toString();
    QString eId = edgeObj.value(QStringLiteral("endid")).toString();
    if (sId.isEmpty()) sId = edgeObj.value(QStringLiteral("id")).toString();
    if (eId.isEmpty()) eId = QStringLiteral("?");

    CheckResultItem warn;
    warn.level = CheckResultItem::Warning;
    warn.message = QStringLiteral("%1与%2→%3有重叠且 maxspeed值不同").arg(areaName, sId, eId);
    warn.targetType = targetType;
    warn.targetId = edgeObj.value(QStringLiteral("id")).toString();
    results.append(warn);
}

// === [Section: Topology Conflict Checks] ===

static void checkAreaToAreaOverlap(const QJsonArray &highLevelAreas, QList<CheckResultItem> &results)
{
    for (int i = 0; i < highLevelAreas.size(); ++i) {
        const QJsonObject areaA = highLevelAreas[i].toObject();
        double minXa = 0, maxXa = 0, minYa = 0, maxYa = 0;
        extractAreaBounds(areaA, minXa, maxXa, minYa, maxYa);
        const QString nameA = areaA.value(QStringLiteral("name")).toString(QStringLiteral("AdvancedArea%1").arg(i + 1));
        const QString idA = areaA.value(QStringLiteral("id")).toString();

        for (int j = i + 1; j < highLevelAreas.size(); ++j) {
            const QJsonObject areaB = highLevelAreas[j].toObject();
            double minXb = 0, maxXb = 0, minYb = 0, maxYb = 0;
            extractAreaBounds(areaB, minXb, maxXb, minYb, maxYb);
            const QString nameB = areaB.value(QStringLiteral("name")).toString(QStringLiteral("AdvancedArea%1").arg(j + 1));

            const double ox1 = qMax(minXa, minXb), ox2 = qMin(maxXa, maxXb);
            const double oy1 = qMax(minYa, minYb), oy2 = qMin(maxYa, maxYb);
            if (ox1 >= ox2 - kTolerance || oy1 >= oy2 - kTolerance) continue;

            for (const auto &p : kDefs) {
                const bool enA = isParamEnabled(areaA, p.propKey, p.jsonKey1, p.jsonKey2);
                const bool enB = isParamEnabled(areaB, p.propKey, p.jsonKey1, p.jsonKey2);
                if (!enA && !enB) continue;
                if (enA != enB) continue;

                const double valA = getParamValue(areaA, p.jsonKey1, p.jsonKey2);
                const double valB = getParamValue(areaB, p.jsonKey1, p.jsonKey2);
                if (std::abs(valA - valB) <= kTolerance) continue;

                CheckResultItem err;
                err.level = CheckResultItem::Error;
                err.message = QStringLiteral("%1 与 %2 有重叠且 %3 不同").arg(nameA, nameB, p.displayName);
                err.targetType = QStringLiteral("highLevelArea");
                err.targetId = idA;
                results.append(err);
            }
        }
    }
}

static void checkAreaToLinesAndCurves(const QJsonArray &highLevelAreas, const QJsonArray &lines,
                                      const QJsonArray &curves, const MotionConfigManager &mc,
                                      QList<CheckResultItem> &results)
{
    for (int i = 0; i < highLevelAreas.size(); ++i) {
        const QJsonObject area = highLevelAreas[i].toObject();
        if (!isParamEnabled(area, QStringLiteral("maxspeed"), QStringLiteral("maxspeed"), QStringLiteral("max_speed"))) {
            continue;
        }

        const QString nameA = area.value(QStringLiteral("name")).toString(QStringLiteral("AdvancedArea%1").arg(i + 1));
        double minX = 0, maxX = 0, minY = 0, maxY = 0;
        extractAreaBounds(area, minX, maxX, minY, maxY);
        const double areaSpd = getParamValue(area, QStringLiteral("maxspeed"), QStringLiteral("max_speed"));

        // 直线检查
        for (const auto &v : lines) {
            const QJsonObject ln = v.toObject();
            const double x1 = ln.value(QStringLiteral("x1")).toDouble(), y1 = ln.value(QStringLiteral("y1")).toDouble();
            const double x2 = ln.value(QStringLiteral("x2")).toDouble(), y2 = ln.value(QStringLiteral("y2")).toDouble();
            if (!lineIntersectsAABB(x1, y1, x2, y2, minX, maxX, minY, maxY)) continue;

            const double lineSpd = ln.contains(QStringLiteral("maxspeed")) ? ln.value(QStringLiteral("maxspeed")).toDouble() : mc.maxSpeed();
            if (std::abs(lineSpd - areaSpd) > kTolerance) {
                appendSpeedMismatchWarning(nameA, ln, QStringLiteral("line"), results);
            }
        }

        // 贝塞尔曲线检查
        for (const auto &v : curves) {
            const QJsonObject cv = v.toObject();
            if (!curveIntersectsAABB(cv, minX, maxX, minY, maxY)) continue;

            const double curveSpd = cv.contains(QStringLiteral("maxspeed")) ? cv.value(QStringLiteral("maxspeed")).toDouble() : mc.maxSpeed();
            if (std::abs(curveSpd - areaSpd) > kTolerance) {
                appendSpeedMismatchWarning(nameA, cv, QStringLiteral("curve"), results);
            }
        }
    }
}

// === [Section: Public Validation Entrypoint] ===

bool validate(const QJsonArray &highLevelAreas, const QJsonArray &lines,
              const QJsonArray &curves, const MotionConfigManager &mc,
              QList<CheckResultItem> &results)
{
    results.clear();
    checkAreaToAreaOverlap(highLevelAreas, results);
    checkAreaToLinesAndCurves(highLevelAreas, lines, curves, mc, results);

    for (const auto &it : results) {
        if (it.level == CheckResultItem::Error) return false;
    }
    return true;
}

// === [Section: Obstacle Parameter Endpoint Synchronization] ===

void syncLineObstaclesToEndpoints(QJsonArray &points, QJsonArray &lines,
                                  QJsonArray &curves, QJsonArray &charges,
                                  QJsonArray &highPrecisionPoints)
{
    RouteObstacleSync::syncLineObstaclesToEndpoints(points, lines, curves, charges, highPrecisionPoints);
}

// === [Section: GeoJSON FeatureCollection Serialization] ===

QJsonObject buildRouteGeoJson(const QString &mapId, const QJsonArray &points,
                              const QJsonArray &lines, const QJsonArray &curves,
                              const QJsonArray &charges, const QJsonArray &highPrecisionPoints,
                              QJsonArray &highLevelAreas)
{
    return RouteGeoJsonBuilder::buildRouteGeoJson(mapId, points, lines, curves, charges, highPrecisionPoints, highLevelAreas);
}

} // namespace RouteNetworkValidator
