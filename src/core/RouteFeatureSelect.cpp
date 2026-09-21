#include "RouteFeatureSelect.h"

#include <QGraphicsScene>
#include <QRectF>
#include <QPen>
#include <QBrush>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QGraphicsPolygonItem>

namespace RouteFeatureSelect {

// 记录在 data(2) 里的原始颜色（用 setItemColor 改色前先取出保存，取消选中时还原）
QColor itemOriginalColor(QGraphicsItem *item)
{
    if (auto *e = qgraphicsitem_cast<QGraphicsEllipseItem*>(item)) return e->brush().color();
    if (auto *l = qgraphicsitem_cast<QGraphicsLineItem*>(item))    return l->pen().color();
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
    }
}

// 选中变色：先把当前(原始)颜色缓存到 data(2)，再统一改为高亮绿；取消选中时按
// data(2) 还原，恢复点/线/曲线各自原本的配色（蓝/橙/红/绿/紫等）。
void applySelectColor(QGraphicsItem *item)
{
    if (!item) return;
    if (!item->data(2).isValid()) item->setData(2, itemOriginalColor(item));
    setItemColor(item, kSelectColor, true);
}

void clearSelectColor(QGraphicsItem *item)
{
    if (!item) return;
    QColor orig = item->data(2).value<QColor>();
    if (orig.isValid()) setItemColor(item, orig, false);
}

QGraphicsItem *pickItem(QGraphicsScene *scene, const QPointF &scenePos, double tolScene)
{
    if (!scene) return nullptr;
    for (auto *it : scene->items(QRectF(scenePos.x() - tolScene, scenePos.y() - tolScene,
                                        tolScene * 2, tolScene * 2))) {
        QGraphicsItem *target = it;
        if (!target->data(0).isValid() && target->parentItem()) {
            target = target->parentItem();
        }
        if (!target->data(0).toString().isEmpty()) return target;
    }
    return nullptr;
}

} // namespace RouteFeatureSelect