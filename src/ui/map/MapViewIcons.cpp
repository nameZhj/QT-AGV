/**
 * @file    MapViewIcons.cpp
 * @layer   GUI
 * @brief   Procedural painter icons for MapView toolbar buttons.
 * @deps    MapViewIcons.h, QPainter, QPixmap, QColor
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapViewIcons.h"
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>

namespace MapViewIcons {

static QPixmap drawMapToolIcon(const QString &name, const QColor &col, int sz = 20)
{
    QPixmap pm(sz, sz);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    double c = sz * 0.5;

    if (name == "add") {
        p.setPen(QPen(col, 1.8, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(c, sz*0.12), QPointF(c, sz*0.88));
        p.drawLine(QPointF(sz*0.12, c), QPointF(sz*0.88, c));
    } else if (name == "swap") {
        p.setPen(QPen(col, 1.7, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(sz*0.18, sz*0.36), QPointF(sz*0.82, sz*0.36));
        p.drawLine(QPointF(sz*0.18, sz*0.64), QPointF(sz*0.82, sz*0.64));
        p.setPen(Qt::NoPen); p.setBrush(col);
        QPolygonF a1; a1 << QPointF(sz*0.82,sz*0.36) << QPointF(sz*0.66,sz*0.26) << QPointF(sz*0.66,sz*0.46);
        QPolygonF a2; a2 << QPointF(sz*0.18,sz*0.64) << QPointF(sz*0.34,sz*0.54) << QPointF(sz*0.34,sz*0.74);
        p.drawPolygon(a1); p.drawPolygon(a2);
    } else if (name == "target") {
        p.setPen(QPen(col, 1.6)); p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(c,c), sz*0.36, sz*0.36);
        p.drawEllipse(QPointF(c,c), sz*0.16, sz*0.16);
        p.setPen(QPen(col, 1.4));
        p.drawLine(QPointF(c, sz*0.02), QPointF(c, sz*0.16));
        p.drawLine(QPointF(c, sz*0.84), QPointF(c, sz*0.98));
        p.drawLine(QPointF(sz*0.02, c), QPointF(sz*0.16, c));
        p.drawLine(QPointF(sz*0.84, c), QPointF(sz*0.98, c));
    } else if (name == "route") {
        p.setPen(QPen(col, 1.7, Qt::SolidLine, Qt::RoundCap)); p.setBrush(Qt::NoBrush);
        QPainterPath path;
        path.moveTo(sz*0.14, sz*0.82);
        path.cubicTo(sz*0.14, sz*0.40, sz*0.86, sz*0.60, sz*0.86, sz*0.18);
        p.drawPath(path);
        p.setPen(Qt::NoPen); p.setBrush(col);
        p.drawEllipse(QPointF(sz*0.14, sz*0.82), sz*0.09, sz*0.09);
        p.drawEllipse(QPointF(sz*0.86, sz*0.18), sz*0.09, sz*0.09);
    } else if (name == "edit") {
        p.setPen(QPen(col, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawLine(QPointF(sz*0.20, sz*0.80), QPointF(sz*0.62, sz*0.38));
        p.setBrush(col);
        QPolygonF tip; tip << QPointF(sz*0.16,sz*0.88) << QPointF(sz*0.20,sz*0.68) << QPointF(sz*0.32,sz*0.80);
        p.drawPolygon(tip);
        p.drawRect(QRectF(sz*0.62, sz*0.20, sz*0.18, sz*0.18));
    } else if (name == "tag") {
        p.setPen(QPen(col, 1.5)); p.setBrush(Qt::NoBrush);
        QPolygonF tag;
        tag << QPointF(sz*0.14,sz*0.20) << QPointF(sz*0.58,sz*0.20)
            << QPointF(sz*0.88,sz*0.50) << QPointF(sz*0.58,sz*0.80) << QPointF(sz*0.14,sz*0.80);
        p.drawPolygon(tag);
        p.setPen(Qt::NoPen); p.setBrush(col);
        p.drawEllipse(QPointF(sz*0.30, sz*0.50), sz*0.06, sz*0.06);
    } else if (name == "download") {
        p.setPen(QPen(col, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(c, sz*0.10), QPointF(c, sz*0.62));
        p.setPen(Qt::NoPen); p.setBrush(col);
        QPolygonF arrow; arrow << QPointF(sz*0.32,sz*0.50) << QPointF(sz*0.68,sz*0.50) << QPointF(c,sz*0.78);
        p.drawPolygon(arrow);
        p.setPen(QPen(col, 1.6));
        p.drawLine(QPointF(sz*0.16, sz*0.88), QPointF(sz*0.84, sz*0.88));
    } else if (name == "file-download") {
        p.setPen(QPen(col, 1.4)); p.setBrush(Qt::NoBrush);
        QPolygonF file;
        file << QPointF(sz*0.22,sz*0.08) << QPointF(sz*0.62,sz*0.08) << QPointF(sz*0.78,sz*0.24)
             << QPointF(sz*0.78,sz*0.62) << QPointF(sz*0.22,sz*0.62);
        p.drawPolygon(file);
        p.setPen(Qt::NoPen); p.setBrush(col);
        QPolygonF arrow; arrow << QPointF(sz*0.36,sz*0.74) << QPointF(sz*0.64,sz*0.74) << QPointF(c,sz*0.96);
        p.drawPolygon(arrow);
    } else if (name == "upload") {
        p.setPen(QPen(col, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(c, sz*0.34), QPointF(c, sz*0.88));
        p.setPen(Qt::NoPen); p.setBrush(col);
        QPolygonF arrow; arrow << QPointF(sz*0.32,sz*0.44) << QPointF(sz*0.68,sz*0.44) << QPointF(c,sz*0.16);
        p.drawPolygon(arrow);
        p.setPen(QPen(col, 1.6));
        p.drawLine(QPointF(sz*0.16, sz*0.10), QPointF(sz*0.84, sz*0.10));
    } else if (name == "map-upload") {
        p.setPen(QPen(col, 1.3)); p.setBrush(Qt::NoBrush);
        QPolygonF mapSheet;
        mapSheet << QPointF(sz*0.16, sz*0.28) << QPointF(sz*0.38, sz*0.20)
                 << QPointF(sz*0.62, sz*0.28) << QPointF(sz*0.84, sz*0.20)
                 << QPointF(sz*0.84, sz*0.82) << QPointF(sz*0.62, sz*0.90)
                 << QPointF(sz*0.38, sz*0.82) << QPointF(sz*0.16, sz*0.90);
        p.drawPolygon(mapSheet);
        p.drawLine(QPointF(sz*0.38, sz*0.20), QPointF(sz*0.38, sz*0.82));
        p.drawLine(QPointF(sz*0.62, sz*0.28), QPointF(sz*0.62, sz*0.90));
        p.setPen(QPen(col, 1.6, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(c, sz*0.74), QPointF(c, sz*0.42));
        p.setPen(Qt::NoPen); p.setBrush(col);
        QPolygonF arrow; arrow << QPointF(sz*0.34, sz*0.52) << QPointF(sz*0.66, sz*0.52) << QPointF(c, sz*0.32);
        p.drawPolygon(arrow);
    } else if (name == "smap-upload") {
        p.setPen(QPen(col, 1.3)); p.setBrush(Qt::NoBrush);
        QPolygonF file;
        file << QPointF(sz*0.18, sz*0.14) << QPointF(sz*0.56, sz*0.14)
             << QPointF(sz*0.82, sz*0.36) << QPointF(sz*0.82, sz*0.88)
             << QPointF(sz*0.18, sz*0.88);
        p.drawPolygon(file);
        p.drawLine(QPointF(sz*0.56, sz*0.14), QPointF(sz*0.56, sz*0.36));
        p.drawLine(QPointF(sz*0.56, sz*0.36), QPointF(sz*0.82, sz*0.36));
        p.setPen(QPen(col, 1.6, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(sz*0.50, sz*0.76), QPointF(sz*0.50, sz*0.46));
        p.setPen(Qt::NoPen); p.setBrush(col);
        QPolygonF arrow; arrow << QPointF(sz*0.36, sz*0.54) << QPointF(sz*0.64, sz*0.54) << QPointF(sz*0.50, sz*0.36);
        p.drawPolygon(arrow);
        p.setPen(col);
        p.setFont(QFont("Consolas", 6, QFont::Bold));
        p.drawText(QRectF(sz*0.22, sz*0.20, sz*0.30, sz*0.30), Qt::AlignCenter, "S");
    } else if (name == "trash") {
        p.setPen(QPen(col, 1.5)); p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(sz*0.24, sz*0.32, sz*0.52, sz*0.56));
        p.drawLine(QPointF(sz*0.14, sz*0.30), QPointF(sz*0.86, sz*0.30));
        p.drawLine(QPointF(sz*0.38, sz*0.30), QPointF(sz*0.42, sz*0.16));
        p.drawLine(QPointF(sz*0.42, sz*0.16), QPointF(sz*0.58, sz*0.16));
        p.drawLine(QPointF(sz*0.58, sz*0.16), QPointF(sz*0.62, sz*0.30));
        p.setPen(QPen(col, 1.3));
        p.drawLine(QPointF(sz*0.38, sz*0.42), QPointF(sz*0.38, sz*0.76));
        p.drawLine(QPointF(c, sz*0.42), QPointF(c, sz*0.76));
        p.drawLine(QPointF(sz*0.62, sz*0.42), QPointF(sz*0.62, sz*0.76));
    } else if (name == "reset") {
        p.setPen(QPen(col, 1.8, Qt::SolidLine, Qt::RoundCap)); p.setBrush(Qt::NoBrush);
        QRectF r(sz*0.22, sz*0.22, sz*0.56, sz*0.56);
        p.drawArc(r, 30 * 16, 260 * 16);
        p.setPen(Qt::NoPen); p.setBrush(col);
        QPolygonF arrow; arrow << QPointF(sz*0.78, sz*0.30) << QPointF(sz*0.86, sz*0.46) << QPointF(sz*0.66, sz*0.42);
        p.drawPolygon(arrow);
    } else if (name == "refresh") {
        p.setPen(QPen(col, 1.8, Qt::SolidLine, Qt::RoundCap)); p.setBrush(Qt::NoBrush);
        QRectF r(sz*0.20, sz*0.20, sz*0.60, sz*0.60);
        p.drawArc(r, 45 * 16, 250 * 16);
        p.setPen(Qt::NoPen); p.setBrush(col);
        QPolygonF arrow; arrow << QPointF(sz*0.78, sz*0.24) << QPointF(sz*0.86, sz*0.44) << QPointF(sz*0.64, sz*0.38);
        p.drawPolygon(arrow);
    } else if (name == "status") {
        p.setPen(QPen(col, 1.4)); p.setBrush(Qt::NoBrush);
        QRectF box(sz*0.14, sz*0.14, sz*0.72, sz*0.72);
        p.drawRoundedRect(box, 3, 3);
        p.setPen(QPen(col, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPolygonF pulse;
        pulse << QPointF(sz*0.22, sz*0.52) << QPointF(sz*0.36, sz*0.52) << QPointF(sz*0.44, sz*0.30)
              << QPointF(sz*0.54, sz*0.74) << QPointF(sz*0.62, sz*0.44) << QPointF(sz*0.68, sz*0.52)
              << QPointF(sz*0.78, sz*0.52);
        p.drawPolyline(pulse);
    } else if (name == "move") {
        p.setPen(QPen(col, 1.6, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(c, sz*0.20), QPointF(c, sz*0.80));
        p.drawLine(QPointF(sz*0.20, c), QPointF(sz*0.80, c));
        p.setPen(Qt::NoPen); p.setBrush(col);
        p.drawPolygon(QPolygonF() << QPointF(c, sz*0.10) << QPointF(c - sz*0.10, sz*0.24) << QPointF(c + sz*0.10, sz*0.24));
        p.drawPolygon(QPolygonF() << QPointF(c, sz*0.90) << QPointF(c - sz*0.10, sz*0.76) << QPointF(c + sz*0.10, sz*0.76));
        p.drawPolygon(QPolygonF() << QPointF(sz*0.10, c) << QPointF(sz*0.24, c - sz*0.10) << QPointF(sz*0.24, c + sz*0.10));
        p.drawPolygon(QPolygonF() << QPointF(sz*0.90, c) << QPointF(sz*0.76, c - sz*0.10) << QPointF(sz*0.76, c + sz*0.10));
    } else if (name == "cancel") {
        p.setPen(QPen(col, 1.4)); p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(c, c), sz*0.38, sz*0.38);
        p.setPen(QPen(col, 1.6, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(c - sz*0.16, c - sz*0.16), QPointF(c + sz*0.16, c + sz*0.16));
        p.drawLine(QPointF(c + sz*0.16, c - sz*0.16), QPointF(c - sz*0.16, c + sz*0.16));
    } else if (name == "pause") {
        p.setPen(Qt::NoPen); p.setBrush(col);
        p.drawRoundedRect(QRectF(sz*0.26, sz*0.20, sz*0.16, sz*0.60), 2, 2);
        p.drawRoundedRect(QRectF(sz*0.58, sz*0.20, sz*0.16, sz*0.60), 2, 2);
    } else if (name == "resume" || name == "play") {
        p.setPen(Qt::NoPen); p.setBrush(col);
        p.drawPolygon(QPolygonF() << QPointF(sz*0.30, sz*0.20) << QPointF(sz*0.78, sz*0.50) << QPointF(sz*0.30, sz*0.80));
    } else if (name == "more") {
        p.setPen(Qt::NoPen); p.setBrush(col);
        p.drawEllipse(QPointF(c - sz*0.26, c), sz*0.075, sz*0.075);
        p.drawEllipse(QPointF(c, c), sz*0.075, sz*0.075);
        p.drawEllipse(QPointF(c + sz*0.26, c), sz*0.075, sz*0.075);
    } else if (name == "teleop") {
        p.setPen(QPen(col, 1.4)); p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(sz*0.14, sz*0.26, sz*0.72, sz*0.48), sz*0.18, sz*0.18);
        p.setPen(QPen(col, 1.5, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(sz*0.34, sz*0.40), QPointF(sz*0.34, sz*0.60));
        p.drawLine(QPointF(sz*0.24, sz*0.50), QPointF(sz*0.44, sz*0.50));
        p.setPen(Qt::NoPen); p.setBrush(col);
        p.drawEllipse(QPointF(sz*0.68, sz*0.44), sz*0.05, sz*0.05);
        p.drawEllipse(QPointF(sz*0.60, sz*0.54), sz*0.05, sz*0.05);
    } else if (name == "config" || name == "settings") {
        p.setPen(QPen(col, 1.4, Qt::SolidLine, Qt::RoundCap)); p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(sz*0.16, sz*0.28), QPointF(sz*0.84, sz*0.28));
        p.drawLine(QPointF(sz*0.16, sz*0.50), QPointF(sz*0.84, sz*0.50));
        p.drawLine(QPointF(sz*0.16, sz*0.72), QPointF(sz*0.84, sz*0.72));
        p.setPen(Qt::NoPen); p.setBrush(col);
        p.drawRoundedRect(QRectF(sz*0.32, sz*0.20, sz*0.14, sz*0.16), 1.5, 1.5);
        p.drawRoundedRect(QRectF(sz*0.58, sz*0.42, sz*0.14, sz*0.16), 1.5, 1.5);
        p.drawRoundedRect(QRectF(sz*0.26, sz*0.64, sz*0.14, sz*0.16), 1.5, 1.5);
    }

    p.end();
    return pm;
}

QIcon makeMapToolIcon(const QString &name)
{
    QIcon icon;
    icon.addPixmap(drawMapToolIcon(name, QColor("#9ab0c8")), QIcon::Normal, QIcon::Off);
    icon.addPixmap(drawMapToolIcon(name, QColor("white")),   QIcon::Active, QIcon::Off);
    return icon;
}

} // namespace MapViewIcons
