/**
 * @file    RouteFeatureSelect.cpp
 * @module  CoreTopology
 * @brief   Implementation of route feature selection enumerations and helpers.
 * @deps    RouteFeatureSelect.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "RouteFeatureSelect.h"

#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QRectF>
#include <QPen>
#include <QBrush>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsRectItem>
#include <cmath>
#include <algorithm>

namespace RouteFeatureSelect {

// ==============================================================================
// === [Section: Internal Style Constants & Shape Helpers] ===
// ==============================================================================

namespace {

constexpr double kSelectedLinePenWidth  = 2.6;
constexpr double kNormalLinePenWidth    = 1.5;
constexpr double kSelectedNodePenWidth  = 2.4;
constexpr double kNormalNodePenWidth    = 1.5;
constexpr double kSelectedPointPenWidth = 1.2;
constexpr double kNormalPointPenWidth   = 0.8;
constexpr double kSelectedRectPenWidth  = 0.8;
constexpr double kNormalRectPenWidth    = 0.5;

constexpr int kDarkenRatio  = 130;
constexpr int kLighterRatio = 115;

const QColor kSelectedPointPenColor(QStringLiteral("#2ECC71"));
const QColor kSelectedRectBrushColor(0, 230, 118, 30);
const QColor kNormalRectBrushColor(139, 92, 246, 20);

constexpr double kMinSquaredLengthTolerance = 1e-9;
constexpr double kInfiniteDistance          = 1e9;
constexpr double kMaxValidDistanceThreshold = 1e8;
constexpr double kCandidateToleranceScale   = 1.25;
constexpr double kMinAlternateSwitchDiff    = 4.0;
constexpr double kAlternateSwitchScale      = 0.4;

void setEllipseItemColor(QGraphicsEllipseItem *e, const QColor &col, bool selected)
{
    QPen pen = e->pen();
    pen.setColor(col.darker(kDarkenRatio));
    pen.setWidthF(selected ? kSelectedNodePenWidth : kNormalNodePenWidth);
    e->setPen(pen);
    e->setBrush(col);
}

void setLineItemColor(QGraphicsLineItem *l, const QColor &col, bool selected)
{
    QPen pen = l->pen();
    pen.setColor(col);
    pen.setWidthF(selected ? kSelectedLinePenWidth : kNormalLinePenWidth);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    l->setPen(pen);
    for (auto *child : l->childItems()) {
        if (auto *poly = qgraphicsitem_cast<QGraphicsPolygonItem*>(child)) {
            poly->setBrush(QBrush(col));
        }
    }
}

void setPathItemColor(QGraphicsPathItem *p, const QColor &col, bool selected)
{
    const QString type = p->data(kRoleType).toString();
    if (type == QStringLiteral("curve")) {
        p->setBrush(Qt::NoBrush);
        QPen pen = p->pen();
        pen.setColor(col);
        pen.setWidthF(selected ? kSelectedLinePenWidth : kNormalLinePenWidth);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        p->setPen(pen);
        for (auto *child : p->childItems()) {
            if (auto *poly = qgraphicsitem_cast<QGraphicsPolygonItem*>(child)) {
                poly->setBrush(QBrush(col));
            }
        }
    } else {
        const QVariant origVar = p->data(kRoleOriginalColor);
        const QColor baseCol = origVar.isValid() ? origVar.value<QColor>() : col;
        if (selected) {
            p->setPen(QPen(kSelectedPointPenColor, kSelectedPointPenWidth));
            p->setBrush(QBrush(baseCol.lighter(kLighterRatio)));
        } else {
            p->setPen(QPen(baseCol.darker(kDarkenRatio), kNormalPointPenWidth));
            p->setBrush(QBrush(baseCol));
        }
    }
}

void setRectItemColor(QGraphicsRectItem *r, const QColor &col, bool selected)
{
    QPen pen = r->pen();
    pen.setColor(col);
    pen.setWidthF(selected ? kSelectedRectPenWidth : kNormalRectPenWidth);
    r->setPen(pen);
    r->setBrush(QBrush(selected ? kSelectedRectBrushColor : kNormalRectBrushColor));
}

// 计算点 p 到线段 ab 的最短欧氏距离
double distanceToSegment(const QPointF &p, const QPointF &a, const QPointF &b)
{
    const QPointF ab = b - a;
    const double lenSq = ab.x() * ab.x() + ab.y() * ab.y();
    if (lenSq < kMinSquaredLengthTolerance) {
        return std::hypot(p.x() - a.x(), p.y() - a.y());
    }
    double t = ((p.x() - a.x()) * ab.x() + (p.y() - a.y()) * ab.y()) / lenSq;
    t = qBound(0.0, t, 1.0);
    const QPointF proj = a + t * ab;
    return std::hypot(p.x() - proj.x(), p.y() - proj.y());
}

// 计算点 p 到路网图元的最小几何距离
double distanceToItem(QGraphicsItem *item, const QPointF &scenePos)
{
    if (!item) return kInfiniteDistance;
    const QString type = item->data(kRoleType).toString();

    // 1. 直线图元 (line)
    if (auto *lineItem = qgraphicsitem_cast<QGraphicsLineItem*>(item)) {
        const QPointF a = lineItem->mapToScene(lineItem->line().p1());
        const QPointF b = lineItem->mapToScene(lineItem->line().p2());
        return distanceToSegment(scenePos, a, b);
    }

    // 2. 曲线图元 (curve)
    if (auto *pathItem = qgraphicsitem_cast<QGraphicsPathItem*>(item)) {
        if (type == QStringLiteral("curve")) {
            const QPainterPath scenePath = pathItem->mapToScene(pathItem->path());
            const QList<QPolygonF> subPolys = scenePath.toSubpathPolygons();
            double minDist = kInfiniteDistance;
            for (const auto &poly : subPolys) {
                for (int i = 0; i < poly.size() - 1; ++i) {
                    const double d = distanceToSegment(scenePos, poly[i], poly[i + 1]);
                    if (d < minDist) minDist = d;
                }
            }
            if (minDist < kMaxValidDistanceThreshold) return minDist;
        }
    }

    // 3. 站点/点位图元 (point, charge, highPrecision)
    if (item->contains(item->mapFromScene(scenePos))) {
        return 0.0;
    }
    const QRectF sbr = item->sceneBoundingRect();
    const QPointF centerPos = sbr.center();
    const double centerDist = std::hypot(scenePos.x() - centerPos.x(), scenePos.y() - centerPos.y());
    const double radius = qMin(sbr.width(), sbr.height()) / 2.0;
    return qMax(0.0, centerDist - radius);
}

} // namespace

// ==============================================================================
// === [Section: Color & Selection State Implementation] ===
// ==============================================================================

QColor itemOriginalColor(QGraphicsItem *item)
{
    if (!item) return {};
    if (auto *e = qgraphicsitem_cast<QGraphicsEllipseItem*>(item)) return e->brush().color();
    if (auto *l = qgraphicsitem_cast<QGraphicsLineItem*>(item))    return l->pen().color();
    if (auto *r = qgraphicsitem_cast<QGraphicsRectItem*>(item))    return r->pen().color();
    if (auto *p = qgraphicsitem_cast<QGraphicsPathItem*>(item)) {
        if (item->data(kRoleType).toString() == QStringLiteral("curve")) return p->pen().color();
        return p->brush().color().isValid() ? p->brush().color() : p->pen().color();
    }
    return {};
}

void setItemColor(QGraphicsItem *item, const QColor &col, bool selected)
{
    if (!item) return;
    if (auto *e = qgraphicsitem_cast<QGraphicsEllipseItem*>(item)) {
        setEllipseItemColor(e, col, selected);
    } else if (auto *l = qgraphicsitem_cast<QGraphicsLineItem*>(item)) {
        setLineItemColor(l, col, selected);
    } else if (auto *p = qgraphicsitem_cast<QGraphicsPathItem*>(item)) {
        setPathItemColor(p, col, selected);
    } else if (auto *r = qgraphicsitem_cast<QGraphicsRectItem*>(item)) {
        setRectItemColor(r, col, selected);
    }
}

void applySelectColor(QGraphicsItem *item)
{
    if (!item) return;
    if (!item->data(kRoleOriginalColor).isValid()) {
        item->setData(kRoleOriginalColor, itemOriginalColor(item));
    }
    item->setData(kRoleSelected, true);
    setItemColor(item, kSelectColor, true);
}

void clearSelectColor(QGraphicsItem *item)
{
    if (!item) return;
    item->setData(kRoleSelected, false);
    const QColor orig = item->data(kRoleOriginalColor).value<QColor>();
    if (orig.isValid()) {
        setItemColor(item, orig, false);
    }
}

// ==============================================================================
// === [Section: Scene Hit-Testing & Feature Picking Implementation] ===
// ==============================================================================

QGraphicsItem *pickItem(QGraphicsScene *scene, const QPointF &scenePos, double tolScene)
{
    if (!scene) return nullptr;

    // 1. 粗筛：获取容差相交区域内的所有 item 并追溯到顶层路网图元（去重）
    QList<QGraphicsItem*> candidates;
    const QRectF queryRect(scenePos.x() - tolScene, scenePos.y() - tolScene, tolScene * 2, tolScene * 2);
    for (auto *it : scene->items(queryRect)) {
        QGraphicsItem *target = it;
        while (target && (!target->data(kRoleType).isValid() || target->data(kRoleType).toString().isEmpty())) {
            target = target->parentItem();
        }
        if (!target) continue;
        const QString typeStr = target->data(kRoleType).toString();
        if (typeStr.isEmpty() || typeStr == QStringLiteral("highLevelArea")) continue;
        if (!candidates.contains(target)) {
            candidates.append(target);
        }
    }

    if (candidates.isEmpty()) return nullptr;

    // 2. 精筛：计算每个候选图元到点击点的最小几何距离
    struct CandInfo {
        QGraphicsItem *item = nullptr;
        double dist = kInfiniteDistance;
        bool isSelected = false;
    };
    QList<CandInfo> validCands;

    for (auto *cand : candidates) {
        const double d = distanceToItem(cand, scenePos);
        if (d <= tolScene * kCandidateToleranceScale) {
            const bool sel = cand->data(kRoleSelected).toBool();
            validCands.append({cand, d, sel});
        }
    }

    if (validCands.isEmpty()) return nullptr;

    // 按距离从小到大排序
    std::sort(validCands.begin(), validCands.end(), [](const CandInfo &a, const CandInfo &b) {
        return a.dist < b.dist;
    });

    // 3. 轮替保护：若距离最近的图元已被选中，且次近图元与最近图元距离非常接近，优先返回次近图元
    if (validCands.size() >= 2 && validCands[0].isSelected) {
        const double diff = validCands[1].dist - validCands[0].dist;
        if (diff <= qMax(kMinAlternateSwitchDiff, tolScene * kAlternateSwitchScale)) {
            return validCands[1].item;
        }
    }

    return validCands[0].item;
}

} // namespace RouteFeatureSelect

