#include "MapDrawHelper.h"
#include <QPainter>
#include <QPainterPath>
#include <cmath>

QPointF MapDrawHelper::worldToScene(double wx, double wy,
                                    double worldL, double worldR,
                                    double worldT, double worldB,
                                    double imgW, double imgH) {
    double mapW = worldR - worldL;
    double mapH = worldT - worldB;
    if (imgW <= 0 || imgH <= 0 || mapW <= 0 || mapH <= 0) {
        return QPointF(wx, wy);
    }
    return QPointF((wx - worldL) / mapW * imgW,
                   (worldT - wy) / mapH * imgH);
}

QPointF MapDrawHelper::sceneToWorld(double sx, double sy,
                                    double worldL, double worldR,
                                    double worldT, double worldB,
                                    double imgW, double imgH) {
    double mapW = worldR - worldL;
    double mapH = worldT - worldB;
    if (imgW <= 0 || imgH <= 0 || mapW <= 0 || mapH <= 0) {
        return QPointF(sx, sy);
    }
    return QPointF(sx / imgW * mapW + worldL,
                   worldT - sy / imgH * mapH);
}

double MapDrawHelper::distance(const QPointF &p1, const QPointF &p2) {
    double dx = p1.x() - p2.x();
    double dy = p1.y() - p2.y();
    return std::hypot(dx, dy);
}

double MapDrawHelper::distancePointToSegment(const QPointF &pt, const QPointF &a, const QPointF &b, QPointF *closestPt) {
    double l2 = (b.x() - a.x()) * (b.x() - a.x()) + (b.y() - a.y()) * (b.y() - a.y());
    if (l2 < 1e-9) {
        if (closestPt) *closestPt = a;
        return distance(pt, a);
    }
    double t = ((pt.x() - a.x()) * (b.x() - a.x()) + (pt.y() - a.y()) * (b.y() - a.y())) / l2;
    t = std::max(0.0, std::min(1.0, t));
    QPointF proj(a.x() + t * (b.x() - a.x()), a.y() + t * (b.y() - a.y()));
    if (closestPt) *closestPt = proj;
    return distance(pt, proj);
}

QPointF MapDrawHelper::quadraticBezier(const QPointF &p0, const QPointF &p1, const QPointF &p2, double t) {
    double u = 1.0 - t;
    return u * u * p0 + 2.0 * u * t * p1 + t * t * p2;
}

QPointF MapDrawHelper::cubicBezier(const QPointF &p0, const QPointF &p1, const QPointF &p2, const QPointF &p3, double t) {
    double u = 1.0 - t;
    return u * u * u * p0 + 3.0 * u * u * t * p1 + 3.0 * u * t * t * p2 + t * t * t * p3;
}

QPolygonF MapDrawHelper::makeArrowHead(const QPointF &from, const QPointF &to, double arrowLength, double arrowAngleDeg) {
    double angle = std::atan2(to.y() - from.y(), to.x() - from.x());
    double rad = arrowAngleDeg * M_PI / 180.0;

    QPointF p1(to.x() - arrowLength * std::cos(angle - rad),
               to.y() - arrowLength * std::sin(angle - rad));
    QPointF p2(to.x() - arrowLength * std::cos(angle + rad),
               to.y() - arrowLength * std::sin(angle + rad));

    QPolygonF poly;
    poly << to << p1 << p2;
    return poly;
}

void MapDrawHelper::drawRobotPose(QPainter *painter, const QPointF &center, double radius, double headingRad,
                                  const QColor &bodyColor, const QColor &arrowColor) {
    if (!painter) return;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    // 绘制车身圆底
    painter->setPen(QPen(bodyColor.darker(120), 1.5));
    painter->setBrush(bodyColor);
    painter->drawEllipse(center, radius, radius);

    // 绘制朝向箭头
    double arrowLen = radius * 0.8;
    QPointF tip(center.x() + arrowLen * std::cos(headingRad),
                center.y() - arrowLen * std::sin(headingRad)); // 场景坐标 Y 向下，世界坐标向上的角度取反

    painter->setPen(QPen(arrowColor, 2.0, Qt::SolidLine, Qt::RoundCap));
    painter->drawLine(center, tip);

    // 绘制箭头侧翼
    double wingLen = radius * 0.35;
    double wingAngle = 30.0 * M_PI / 180.0;
    QPointF wing1(tip.x() - wingLen * std::cos(headingRad - wingAngle),
                  tip.y() + wingLen * std::sin(headingRad - wingAngle));
    QPointF wing2(tip.x() - wingLen * std::cos(headingRad + wingAngle),
                  tip.y() + wingLen * std::sin(headingRad + wingAngle));

    QPolygonF arrowPoly;
    arrowPoly << tip << wing1 << wing2;
    painter->setBrush(arrowColor);
    painter->drawPolygon(arrowPoly);

    painter->restore();
}
