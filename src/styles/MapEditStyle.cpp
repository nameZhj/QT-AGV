/**
 * @file    MapEditStyle.cpp
 * @layer   GUILayer
 * @brief   Implementation of vector icon painting for map editing tools.
 * @deps    MapEditStyle.h, QPainter, QPainterPath
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapEditStyle.h"

namespace MapEditStyle {

QPixmap drawToolIcon(const QString &name, const QColor &col, int sz)
{
    QPixmap pm(sz, sz);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const double c = sz * 0.5;

    if (name == "pan") {
        QPainterPath ptr;
        ptr.moveTo(sz * 0.22, sz * 0.14);
        ptr.lineTo(sz * 0.22, sz * 0.80);
        ptr.lineTo(sz * 0.42, sz * 0.60);
        ptr.lineTo(sz * 0.62, sz * 0.82);
        ptr.lineTo(sz * 0.74, sz * 0.72);
        ptr.lineTo(sz * 0.53, sz * 0.51);
        ptr.lineTo(sz * 0.78, sz * 0.46);
        ptr.closeSubpath();
        p.setPen(QPen(col.darker(120), 1.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(col);
        p.drawPath(ptr);
    }
    else if (name == "point") {
        QRectF rc(sz * 0.15, sz * 0.28, sz * 0.70, sz * 0.44);
        p.setPen(QPen(col, 1.4));
        p.setBrush(col.lighter(160));
        p.drawRoundedRect(rc, 2, 2);
        p.setPen(QPen(QColor("#e74c3c"), 1.2));
        p.drawLine(QPointF(c - sz * 0.11, c), QPointF(c + sz * 0.11, c));
        p.drawLine(QPointF(c, c - sz * 0.11), QPointF(c, c + sz * 0.11));
    }
    else if (name == "line") {
        QPointF a(sz * 0.15, sz * 0.85), b2(sz * 0.85, sz * 0.15);
        p.setPen(QPen(col, 1.8, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(a, b2);
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        p.drawEllipse(a, sz * 0.13, sz * 0.13);
        p.drawEllipse(b2, sz * 0.13, sz * 0.13);
    }
    else if (name == "curve") {
        QPointF a(sz * 0.08, sz * 0.88), ctrl(c, sz * 0.10), b2(sz * 0.92, sz * 0.88);
        QPainterPath path;
        path.moveTo(a);
        path.quadTo(ctrl, b2);
        p.setPen(QPen(col, 1.8, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        p.drawEllipse(a, sz * 0.12, sz * 0.12);
        p.drawEllipse(b2, sz * 0.12, sz * 0.12);
        p.setPen(QPen(col.lighter(170), 0.9, Qt::DotLine));
        p.setBrush(Qt::NoBrush);
        p.drawLine(a, ctrl);
        p.drawLine(ctrl, b2);
        p.drawEllipse(ctrl, sz * 0.10, sz * 0.10);
    }
    else if (name == "charge") {
        QPolygonF bolt;
        bolt << QPointF(c + sz * 0.10, sz * 0.06)  << QPointF(c - sz * 0.20, c + sz * 0.06)
             << QPointF(c + sz * 0.04, c + sz * 0.06) << QPointF(c - sz * 0.10, sz * 0.94)
             << QPointF(c + sz * 0.20, c - sz * 0.06) << QPointF(c - sz * 0.04, c - sz * 0.06);
        p.setPen(QPen(col.darker(115), 1.0));
        p.setBrush(col);
        p.drawPolygon(bolt);
    }
    else if (name == "highprecision") {
        double r = sz * 0.35, gap = sz * 0.14;
        p.setPen(QPen(col, 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(c, c), r, r);
        p.drawLine(QPointF(c, sz * 0.04), QPointF(c, c - gap));
        p.drawLine(QPointF(c, c + gap),   QPointF(c, sz * 0.96));
        p.drawLine(QPointF(sz * 0.04, c), QPointF(c - gap, c));
        p.drawLine(QPointF(c + gap, c),   QPointF(sz * 0.96, c));
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        p.drawEllipse(QPointF(c, c), sz * 0.09, sz * 0.09);
    }
    else if (name == "list") {
        p.setPen(QPen(col, 1.8, Qt::SolidLine, Qt::RoundCap));
        double x1 = sz * 0.18, x2 = sz * 0.82;
        p.drawLine(QPointF(x1, c - sz * 0.24), QPointF(x2, c - sz * 0.24));
        p.drawLine(QPointF(x1, c),             QPointF(x2, c));
        p.drawLine(QPointF(x1, c + sz * 0.24), QPointF(x2, c + sz * 0.24));
    }
    else if (name == "save") {
        p.setPen(QPen(col, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPainterPath cp;
        cp.moveTo(sz * 0.14, sz * 0.50);
        cp.lineTo(sz * 0.42, sz * 0.80);
        cp.lineTo(sz * 0.86, sz * 0.20);
        p.drawPath(cp);
    }
    else if (name == "grid") {
        p.setPen(QPen(col, 1.4));
        double m1 = sz * 0.18, m2 = sz * 0.82, t1 = sz * 0.40, t2 = sz * 0.60;
        p.drawRect(QRectF(m1, m1, m2 - m1, m2 - m1));
        p.drawLine(QPointF(t1, m1), QPointF(t1, m2));
        p.drawLine(QPointF(t2, m1), QPointF(t2, m2));
        p.drawLine(QPointF(m1, t1), QPointF(m2, t1));
        p.drawLine(QPointF(m1, t2), QPointF(m2, t2));
    }
    else if (name == "reset") {
        p.setPen(QPen(col, 1.8, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);
        QRectF r(sz * 0.22, sz * 0.22, sz * 0.56, sz * 0.56);
        p.drawArc(r, 30 * 16, 260 * 16);
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        QPolygonF arrow;
        arrow << QPointF(sz * 0.78, sz * 0.30)
              << QPointF(sz * 0.86, sz * 0.46)
              << QPointF(sz * 0.66, sz * 0.42);
        p.drawPolygon(arrow);
    }
    else if (name == "robot") {
        QPolygonF tri;
        tri << QPointF(sz * 0.85, c) << QPointF(sz * 0.20, sz * 0.82) << QPointF(sz * 0.20, sz * 0.18);
        p.setPen(QPen(col.darker(115), 1.0));
        p.setBrush(col);
        p.drawPolygon(tri);
    }
    else if (name == "devicepoint") {
        p.setPen(QPen(col, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPainterPath pin;
        double r = sz * 0.28;
        pin.moveTo(c - r, c - sz * 0.06);
        pin.cubicTo(c - r, c - sz * 0.30, c + r, c - sz * 0.30, c + r, c - sz * 0.06);
        pin.lineTo(c, sz * 0.88);
        pin.closeSubpath();
        p.setBrush(col);
        p.drawPath(pin);
        p.setPen(Qt::NoPen);
        p.setBrush(col.lighter(140));
        p.drawEllipse(QPointF(c, c - sz * 0.12), sz * 0.10, sz * 0.10);
    }
    else if (name == "brush") {
        p.setPen(QPen(col, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawLine(QPointF(sz * 0.25, sz * 0.75), QPointF(sz * 0.75, sz * 0.25));
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        p.drawEllipse(QPointF(sz * 0.22, sz * 0.78), sz * 0.12, sz * 0.12);
        p.drawEllipse(QPointF(sz * 0.78, sz * 0.22), sz * 0.08, sz * 0.08);
    }
    else if (name == "eraser") {
        p.setPen(QPen(col, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QRectF er(sz * 0.22, sz * 0.35, sz * 0.56, sz * 0.36);
        p.drawRoundedRect(er, 2, 2);
        p.drawLine(QPointF(sz * 0.48, sz * 0.35), QPointF(sz * 0.48, sz * 0.71));
    }
    else if (name == "undo") {
        p.setPen(QPen(col, 1.8, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);
        QRectF r(sz * 0.22, sz * 0.22, sz * 0.56, sz * 0.56);
        p.drawArc(r, 45 * 16, 220 * 16);
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        QPolygonF arrow;
        arrow << QPointF(sz * 0.20, sz * 0.28) << QPointF(sz * 0.40, sz * 0.18) << QPointF(sz * 0.35, sz * 0.42);
        p.drawPolygon(arrow);
    }
    else if (name == "redo") {
        p.setPen(QPen(col, 1.8, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);
        QRectF r(sz * 0.22, sz * 0.22, sz * 0.56, sz * 0.56);
        p.drawArc(r, -45 * 16, -220 * 16);
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        QPolygonF arrow;
        arrow << QPointF(sz * 0.80, sz * 0.28) << QPointF(sz * 0.60, sz * 0.18) << QPointF(sz * 0.65, sz * 0.42);
        p.drawPolygon(arrow);
    }
    else if (name == "teleop") {
        p.setPen(QPen(col, 1.4));
        p.setBrush(Qt::NoBrush);
        QRectF body(sz * 0.14, sz * 0.26, sz * 0.72, sz * 0.48);
        p.drawRoundedRect(body, sz * 0.18, sz * 0.18);
        p.setPen(QPen(col, 1.5, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(sz * 0.34, sz * 0.40), QPointF(sz * 0.34, sz * 0.60));
        p.drawLine(QPointF(sz * 0.24, sz * 0.50), QPointF(sz * 0.44, sz * 0.50));
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        p.drawEllipse(QPointF(sz * 0.68, sz * 0.44), sz * 0.05, sz * 0.05);
        p.drawEllipse(QPointF(sz * 0.60, sz * 0.54), sz * 0.05, sz * 0.05);
    }
    else if (name == "region" || name == "area") {
        p.setPen(QPen(col, 1.4, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        QRectF rf(sz * 0.22, sz * 0.22, sz * 0.56, sz * 0.56);
        p.drawRect(rf);
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        double dotR = sz * 0.06;
        p.drawRect(QRectF(rf.left() - dotR / 2, rf.top() - dotR / 2, dotR, dotR));
        p.drawRect(QRectF(rf.right() - dotR / 2, rf.top() - dotR / 2, dotR, dotR));
        p.drawRect(QRectF(rf.left() - dotR / 2, rf.bottom() - dotR / 2, dotR, dotR));
        p.drawRect(QRectF(rf.right() - dotR / 2, rf.bottom() - dotR / 2, dotR, dotR));
    }
    return pm;
}

QIcon makeToolIcon(const QString &name)
{
    QIcon icon;
    icon.addPixmap(drawToolIcon(name, QColor("#7a95b0")), QIcon::Normal,  QIcon::Off);
    icon.addPixmap(drawToolIcon(name, QColor("#c8ddef")), QIcon::Active,  QIcon::Off);
    icon.addPixmap(drawToolIcon(name, QColor("white")),   QIcon::Normal,  QIcon::On);
    icon.addPixmap(drawToolIcon(name, QColor("white")),   QIcon::Active,  QIcon::On);
    return icon;
}

} // namespace MapEditStyle
