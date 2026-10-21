/**
 * @file    MapImageEditIcons.cpp
 * @layer   GUILayer
 * @brief   Implementation of vector icon drawing for raster map editor.
 * @deps    MapImageEditIcons.h, QPainter, QPixmap
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapImageEditIcons.h"
#include <QPainter>
#include <QPixmap>
#include <QPolygonF>

namespace MapImageEditIcons {

QIcon drawBrushToolIcon(bool eraser)
{
    int sz = 22;
    QPixmap pix(sz, sz);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(220, 220, 220), 1.2));
    if (!eraser) {
        // 画笔：实心圆点，代表涂黑(标记障碍)
        p.setBrush(QColor(40, 40, 40));
        p.drawEllipse(QRectF(2, 2, 18, 18));
        p.setBrush(QColor(10, 10, 10));
        p.drawEllipse(QRectF(6, 6, 10, 10));
    } else {
        // 橡皮：白色方块，代表擦除为可通行区域
        p.setBrush(QColor(245, 245, 245));
        p.drawRect(QRectF(3, 3, 16, 16));
    }
    p.end();
    return QIcon(pix);
}

QIcon drawMoveToolIcon()
{
    int sz = 22;
    QPixmap pix(sz, sz);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    // 移动：四向箭头，代表取消画笔/橡皮，仅用于平移浏览
    QColor col(220, 220, 220);
    double c = sz * 0.5;
    p.setPen(QPen(col, 1.6, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(c, sz * 0.12), QPointF(c, sz * 0.88));
    p.drawLine(QPointF(sz * 0.12, c), QPointF(sz * 0.88, c));
    p.setPen(Qt::NoPen);
    p.setBrush(col);
    QPolygonF up;
    up << QPointF(c, sz * 0.04) << QPointF(c - sz * 0.10, sz * 0.18) << QPointF(c + sz * 0.10, sz * 0.18);
    QPolygonF down;
    down << QPointF(c, sz * 0.96) << QPointF(c - sz * 0.10, sz * 0.82) << QPointF(c + sz * 0.10, sz * 0.82);
    QPolygonF left;
    left << QPointF(sz * 0.04, c) << QPointF(sz * 0.18, c - sz * 0.10) << QPointF(sz * 0.18, c + sz * 0.10);
    QPolygonF right;
    right << QPointF(sz * 0.96, c) << QPointF(sz * 0.82, c - sz * 0.10) << QPointF(sz * 0.82, c + sz * 0.10);
    p.drawPolygon(up);
    p.drawPolygon(down);
    p.drawPolygon(left);
    p.drawPolygon(right);
    p.end();
    return QIcon(pix);
}

} // namespace MapImageEditIcons
