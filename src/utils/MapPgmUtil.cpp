/**
 * @file    MapPgmUtil.cpp
 * @layer   Utils
 * @brief   Implementation of Bezier geometry, arrow generation, and area bounds.
 * @deps    MapPgmUtil.h, MapPgmCodec.h, MapSmapParser.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapPgmUtil.h"
#include "MapSmapParser.h"

#include <cmath>
#include <QtGlobal>

namespace {

constexpr double kArrowLengthScale           = 3.5;
constexpr double kArrowWingScale             = 2.2;
constexpr double kArrowNotchScale            = 0.8;
constexpr double kMinTangentMagnitudeSquared = 1e-12;

/**
 * @brief 构建流线型燕尾内凹指示箭头多边形
 */
QPolygonF buildArrowPolygon(const QPointF &center, const QPointF &dirUnit, double scale)
{
    const QPointF norm(-dirUnit.y(), dirUnit.x());
    QPolygonF poly;
    poly << center + dirUnit * (kArrowLengthScale * scale)
         << center - dirUnit * (kArrowWingScale * scale) + norm * (kArrowWingScale * scale)
         << center - dirUnit * (kArrowNotchScale * scale)
         << center - dirUnit * (kArrowWingScale * scale) - norm * (kArrowWingScale * scale);
    return poly;
}

/**
 * @brief 判定是否为双向通行模式
 */
inline bool isBidirectional(int direct)
{
    return (direct == static_cast<int>(MapPgmUtil::Direction::Bidirectional) ||
            direct == static_cast<int>(MapPgmUtil::Direction::BidirectionalAlt));
}

} // anonymous namespace

// ==============================================================================
// === [Section: Bezier & Tangent Geometry Engine] ===
// ==============================================================================

QPointF MapPgmUtil::evaluateBezier(const QList<QPointF> &controlPoints, double t)
{
    const int n = controlPoints.size() - 1;
    if (n < 0) return QPointF();
    if (n == 0) return controlPoints[0];

    // 特化常见 3 次贝塞尔曲线，避免频繁临时堆分配
    if (n == 3) {
        const double u = 1.0 - t;
        const double tt = t * t;
        const double uu = u * u;
        const double uuu = uu * u;
        const double ttt = tt * t;

        return uuu * controlPoints[0]
             + 3.0 * uu * t * controlPoints[1]
             + 3.0 * u * tt * controlPoints[2]
             + ttt * controlPoints[3];
    }

    // 任意阶 De Casteljau 通用算法
    QList<QPointF> temp = controlPoints;
    for (int r = 1; r <= n; ++r) {
        for (int i = 0; i <= n - r; ++i) {
            temp[i] = (1.0 - t) * temp[i] + t * temp[i + 1];
        }
    }
    return temp[0];
}

QPainterPath MapPgmUtil::getBezierPath(const QList<QPointF> &controlPoints)
{
    QPainterPath path;
    if (controlPoints.isEmpty()) return path;

    path.moveTo(controlPoints.first());
    if (controlPoints.size() < 2) return path;

    constexpr int steps = kDefaultBezierSampleSteps;
    for (int i = 1; i <= steps; ++i) {
        const double t = static_cast<double>(i) / steps;
        path.lineTo(evaluateBezier(controlPoints, t));
    }
    return path;
}

QList<QPolygonF> MapPgmUtil::createDirectionArrows(const QPointF &mid,
                                                  const QPointF &dirUnit,
                                                  int direct,
                                                  double lineLen)
{
    QList<QPolygonF> result;
    const double lenSq = dirUnit.x() * dirUnit.x() + dirUnit.y() * dirUnit.y();
    if (lenSq < kMinTangentMagnitudeSquared) return result;

    const double len = std::sqrt(lenSq);
    const QPointF d = dirUnit / len;

    double scale = 1.0;
    if (lineLen < 15.0) {
        scale = 0.65;
    } else if (lineLen < 25.0) {
        scale = 0.85;
    }

    if (isBidirectional(direct)) {
        const double gap = qBound(3.5, lineLen * 0.12, 7.5);
        result.append(buildArrowPolygon(mid + d * gap, d, scale));
        result.append(buildArrowPolygon(mid - d * gap, -d, scale));
    } else if (direct == static_cast<int>(MapPgmUtil::Direction::Backward)) {
        result.append(buildArrowPolygon(mid, -d, scale));
    } else {
        result.append(buildArrowPolygon(mid, d, scale));
    }
    return result;
}

QList<QPolygonF> MapPgmUtil::createBezierDirectionArrows(const QList<QPointF> &controlPoints,
                                                        int direct,
                                                        double curveLen)
{
    QList<QPolygonF> result;
    if (controlPoints.size() < 2) return result;

    double scale = 1.0;
    if (curveLen < 15.0) {
        scale = 0.65;
    } else if (curveLen < 25.0) {
        scale = 0.85;
    }

    auto safeMakeArrow = [scale](const QPointF &center, const QPointF &forwardDir) -> QPolygonF {
        const double lenSq = forwardDir.x() * forwardDir.x() + forwardDir.y() * forwardDir.y();
        if (lenSq < kMinTangentMagnitudeSquared) return QPolygonF();
        const QPointF d = forwardDir / std::sqrt(lenSq);
        return buildArrowPolygon(center, d, scale);
    };

    if (isBidirectional(direct)) {
        const double dt = (curveLen < 25.0) ? 0.03 : 0.05;
        const double t1 = 0.50 + dt;
        const double t2 = 0.50 - dt;

        const QPointF p1 = evaluateBezier(controlPoints, t1);
        const QPointF dir1 = evaluateBezier(controlPoints, t1 + 0.02) - evaluateBezier(controlPoints, t1 - 0.02);
        const QPolygonF arrow1 = safeMakeArrow(p1, dir1);
        if (!arrow1.isEmpty()) result.append(arrow1);

        const QPointF p2 = evaluateBezier(controlPoints, t2);
        const QPointF dir2 = evaluateBezier(controlPoints, t2 + 0.02) - evaluateBezier(controlPoints, t2 - 0.02);
        const QPolygonF arrow2 = safeMakeArrow(p2, -dir2);
        if (!arrow2.isEmpty()) result.append(arrow2);
    } else if (direct == static_cast<int>(MapPgmUtil::Direction::Backward)) {
        const QPointF mid = evaluateBezier(controlPoints, 0.50);
        const QPointF dir = evaluateBezier(controlPoints, 0.52) - evaluateBezier(controlPoints, 0.48);
        const QPolygonF arrow = safeMakeArrow(mid, -dir);
        if (!arrow.isEmpty()) result.append(arrow);
    } else {
        const QPointF mid = evaluateBezier(controlPoints, 0.50);
        const QPointF dir = evaluateBezier(controlPoints, 0.52) - evaluateBezier(controlPoints, 0.48);
        const QPolygonF arrow = safeMakeArrow(mid, dir);
        if (!arrow.isEmpty()) result.append(arrow);
    }
    return result;
}

QPainterPath MapPgmUtil::createPointPath(double pxPerMeterX,
                                        double pxPerMeterY,
                                        double *outW,
                                        double *outH,
                                        QPainterPath *outCrossPath,
                                        double minWidth)
{
    const double scaleX = (pxPerMeterX > 0.0) ? pxPerMeterX : 20.0;
    const double scaleY = (pxPerMeterY > 0.0) ? pxPerMeterY : 20.0;

    double rectW = kRobotPhysicalLengthMeters * scaleX;
    const double rectH = kRobotPhysicalWidthMeters * scaleY;
    if (minWidth > 0.0 && rectW < minWidth && minWidth <= rectW * 1.25) {
        rectW = minWidth;
    }

    if (outW) *outW = rectW;
    if (outH) *outH = rectH;

    QPainterPath rectPath;
    rectPath.addRect(QRectF(-rectW / 2.0, -rectH / 2.0, rectW, rectH));

    if (outCrossPath) {
        QPainterPath crossPath;
        const double cw = qBound(1.0, 0.06 * scaleX, rectW * 0.22);
        const double ch = qBound(1.0, 0.06 * scaleY, rectH * 0.22);
        crossPath.moveTo(-cw, 0.0); crossPath.lineTo(cw, 0.0);
        crossPath.moveTo(0.0, -ch); crossPath.lineTo(0.0, ch);
        *outCrossPath = crossPath;
    }

    return rectPath;
}

// ==============================================================================
// === [Section: SMAP Map Import & Curve Merging] ===
// ==============================================================================

bool MapPgmUtil::parseSmapFile(const QString &filePath,
                              QJsonArray &outPoints,
                              QJsonArray &outLines,
                              QJsonArray &outCurves,
                              QString &outMapName,
                              QString *outError,
                              QJsonObject *outHeader)
{
    return MapSmapParser::parseSmapFile(filePath, outPoints, outLines, outCurves, outMapName, outError, outHeader);
}

bool MapPgmUtil::parseSmapJson(const QJsonObject &smapObj,
                              QJsonArray &outPoints,
                              QJsonArray &outLines,
                              QJsonArray &outCurves,
                              QString &outMapName,
                              QString *outError,
                              QJsonObject *outHeader)
{
    return MapSmapParser::parseSmapJson(smapObj, outPoints, outLines, outCurves, outMapName, outError, outHeader);
}

