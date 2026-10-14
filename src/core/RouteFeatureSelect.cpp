#include "RouteFeatureSelect.h"

#include <QGraphicsScene>
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

// 记录在 data(2) 里的原始颜色（用 setItemColor 改色前先取出保存，取消选中时还原）
QColor itemOriginalColor(QGraphicsItem *item)
{
    if (auto *e = qgraphicsitem_cast<QGraphicsEllipseItem*>(item)) return e->brush().color();
    if (auto *l = qgraphicsitem_cast<QGraphicsLineItem*>(item))    return l->pen().color();
    if (auto *r = qgraphicsitem_cast<QGraphicsRectItem*>(item))    return r->pen().color();
    if (auto *p = qgraphicsitem_cast<QGraphicsPathItem*>(item)) {
        if (item->data(0).toString() == "curve") return p->pen().color();
        return p->brush().color().isValid() ? p->brush().color() : p->pen().color();
    }
    return {};
}

void setItemColor(QGraphicsItem *item, const QColor &col, bool selected)
{
    if (auto *e = qgraphicsitem_cast<QGraphicsEllipseItem*>(item)) {
        QPen pen = e->pen(); pen.setColor(col.darker(130)); pen.setWidthF(selected ? 2.4 : 1.5);
        e->setPen(pen);
        e->setBrush(col);
    } else if (auto *l = qgraphicsitem_cast<QGraphicsLineItem*>(item)) {
        QPen pen = l->pen(); pen.setColor(col); pen.setWidthF(selected ? 2.6 : 1.5);
        pen.setCapStyle(Qt::RoundCap); pen.setJoinStyle(Qt::RoundJoin);
        l->setPen(pen);
        for (auto *child : item->childItems()) {
            if (auto *poly = qgraphicsitem_cast<QGraphicsPolygonItem*>(child)) {
                poly->setBrush(QBrush(col));
            }
        }
    } else if (auto *p = qgraphicsitem_cast<QGraphicsPathItem*>(item)) {
        QString type = item->data(0).toString();
        if (type == "curve") {
            // 贝塞尔曲线：保持无画刷填充 (NoBrush)，严禁填充闭合区域，仅加粗线条画笔并变色高亮
            p->setBrush(Qt::NoBrush);
            QPen pen = p->pen();
            pen.setColor(col);
            pen.setWidthF(selected ? 2.6 : 1.5);
            pen.setCapStyle(Qt::RoundCap); pen.setJoinStyle(Qt::RoundJoin);
            p->setPen(pen);
            for (auto *child : item->childItems()) {
                if (auto *poly = qgraphicsitem_cast<QGraphicsPolygonItem*>(child)) {
                    poly->setBrush(QBrush(col));
                }
            }
        } else {
            // 点图元(QGraphicsPathItem): 保持节点原本类型色彩，选中时加深边框并高亮
            QColor baseCol = item->data(2).isValid() ? item->data(2).value<QColor>() : col;
            if (selected) {
                QPen pen(QColor("#2ECC71"), 1.2);
                p->setPen(pen);
                QColor fillCol = baseCol.lighter(115);
                p->setBrush(QBrush(fillCol));
            } else {
                QPen pen(baseCol.darker(130), 0.8);
                p->setPen(pen);
                p->setBrush(QBrush(baseCol));
            }
        }
    } else if (auto *r = qgraphicsitem_cast<QGraphicsRectItem*>(item)) {
        QPen pen = r->pen();
        pen.setColor(col);
        pen.setWidthF(selected ? 0.8 : 0.5);
        r->setPen(pen);
        r->setBrush(QBrush(selected ? QColor(0, 230, 118, 30) : QColor(139, 92, 246, 20)));
    }
}

namespace {

// 计算点 p 到线段 ab 的最短欧氏距离
double distanceToSegment(const QPointF &p, const QPointF &a, const QPointF &b)
{
    QPointF ab = b - a;
    double lenSq = ab.x() * ab.x() + ab.y() * ab.y();
    if (lenSq < 1e-9) {
        return std::hypot(p.x() - a.x(), p.y() - a.y());
    }
    double t = ((p.x() - a.x()) * ab.x() + (p.y() - a.y()) * ab.y()) / lenSq;
    t = qBound(0.0, t, 1.0);
    QPointF proj = a + t * ab;
    return std::hypot(p.x() - proj.x(), p.y() - proj.y());
}

// 计算点 p 到路网图元的最小几何距离
double distanceToItem(QGraphicsItem *item, const QPointF &scenePos)
{
    if (!item) return 1e9;
    QString type = item->data(0).toString();

    // 1. 直线图元 (line)
    if (auto *lineItem = qgraphicsitem_cast<QGraphicsLineItem*>(item)) {
        QPointF a = lineItem->mapToScene(lineItem->line().p1());
        QPointF b = lineItem->mapToScene(lineItem->line().p2());
        return distanceToSegment(scenePos, a, b);
    }

    // 2. 曲线图元 (curve)
    if (auto *pathItem = qgraphicsitem_cast<QGraphicsPathItem*>(item)) {
        if (type == "curve") {
            QPainterPath scenePath = pathItem->mapToScene(pathItem->path());
            QList<QPolygonF> subPolys = scenePath.toSubpathPolygons();
            double minDist = 1e9;
            for (const auto &poly : subPolys) {
                for (int i = 0; i < poly.size() - 1; ++i) {
                    double d = distanceToSegment(scenePos, poly[i], poly[i + 1]);
                    if (d < minDist) minDist = d;
                }
            }
            if (minDist < 1e8) return minDist;
        }
    }

    // 3. 站点/点位图元 (point, charge, highPrecision)
    if (item->contains(item->mapFromScene(scenePos))) {
        return 0.0;
    }
    QRectF sbr = item->sceneBoundingRect();
    QPointF c = sbr.center();
    double centerDist = std::hypot(scenePos.x() - c.x(), scenePos.y() - c.y());
    double radius = qMin(sbr.width(), sbr.height()) / 2.0;
    return qMax(0.0, centerDist - radius);
}

} // namespace

// 选中变色：先把当前(原始)颜色缓存到 data(2)，再统一改为高亮绿；取消选中时按
// data(2) 还原，恢复点/线/曲线各自原本的配色（蓝/橙/红/绿/紫等）。
void applySelectColor(QGraphicsItem *item)
{
    if (!item) return;
    if (!item->data(2).isValid()) item->setData(2, itemOriginalColor(item));
    item->setData(10, true);
    setItemColor(item, kSelectColor, true);
}

void clearSelectColor(QGraphicsItem *item)
{
    if (!item) return;
    item->setData(10, false);
    QColor orig = item->data(2).value<QColor>();
    if (orig.isValid()) setItemColor(item, orig, false);
}

QGraphicsItem *pickItem(QGraphicsScene *scene, const QPointF &scenePos, double tolScene)
{
    if (!scene) return nullptr;

    // 1. 粗筛：获取容差相交区域内的所有 item 并追溯到顶层路网图元（去重）
    QList<QGraphicsItem*> candidates;
    for (auto *it : scene->items(QRectF(scenePos.x() - tolScene, scenePos.y() - tolScene,
                                        tolScene * 2, tolScene * 2))) {
        QGraphicsItem *target = it;
        while (target && (!target->data(0).isValid() || target->data(0).toString().isEmpty())) {
            target = target->parentItem();
        }
        if (!target || target->data(0).toString().isEmpty()) continue;
        if (target->data(0).toString() == "highLevelArea") continue; // 高级区域不参与点线几何拾取，由外部专属优先级判定
        if (!candidates.contains(target)) {
            candidates.append(target);
        }
    }

    if (candidates.isEmpty()) return nullptr;

    // 2. 精筛：计算每个候选图元到点击点的最小几何距离
    struct CandInfo {
        QGraphicsItem *item = nullptr;
        double dist = 1e9;
        bool isSelected = false;
    };
    QList<CandInfo> validCands;

    for (auto *cand : candidates) {
        double d = distanceToItem(cand, scenePos);
        if (d <= tolScene * 1.25) {
            bool sel = cand->data(10).toBool();
            validCands.append({cand, d, sel});
        }
    }

    if (validCands.isEmpty()) return nullptr;

    // 按距离从小到大排序
    std::sort(validCands.begin(), validCands.end(), [](const CandInfo &a, const CandInfo &b) {
        return a.dist < b.dist;
    });

    // 3. 轮替保护：若距离最近的图元已被选中，且次近图元与最近图元距离非常接近（距离差在容差阈值内），
    // 优先返回次近图元，支持在极度临近/重合线段之间点击轮替切换
    if (validCands.size() >= 2 && validCands[0].isSelected) {
        double diff = validCands[1].dist - validCands[0].dist;
        if (diff <= qMax(4.0, tolScene * 0.4)) {
            return validCands[1].item;
        }
    }

    return validCands[0].item;
}

} // namespace RouteFeatureSelect