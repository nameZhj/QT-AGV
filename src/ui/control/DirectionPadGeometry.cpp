/**
 * @file    DirectionPadGeometry.cpp
 * @layer   GUILayer
 * @brief   Implementation of vector geometry and polygon path generators for D-pad.
 * @deps    DirectionPadGeometry.h, QPolygonF
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "DirectionPadGeometry.h"
#include <QPolygonF>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace DirectionPadGeometry {

void buildPaths(QPainterPath &up,
                QPainterPath &down,
                QPainterPath &left,
                QPainterPath &right,
                QPainterPath &center,
                QPainterPath &settings)
{
    // 1. 上正三角 (前进 W)
    static const QPointF pts_up[] = {
        QPointF(50.0, 32.5), QPointF(51.4, 33.8), QPointF(79.3, 33.8), QPointF(80.6, 31.9),
        QPointF(75.4, 24.1), QPointF(74.1, 20.8), QPointF(72.8, 20.1), QPointF(72.8, 18.9),
        QPointF(67.0, 10.4), QPointF(65.7, 9.8),  QPointF(63.0, 10.4), QPointF(59.8, 16.2),
        QPointF(57.9, 18.2), QPointF(57.2, 20.1), QPointF(50.7, 29.2)
    };
    up.clear();
    QPolygonF poly_up;
    for (const auto &p : pts_up) poly_up << p;
    up.addPolygon(poly_up);
    up.closeSubpath();

    // 2. 下倒三角 (后退 S)
    static const QPointF pts_down[] = {
        QPointF(49.4, 98.8), QPointF(57.2, 110.5), QPointF(58.5, 113.8), QPointF(63.7, 120.9),
        QPointF(66.3, 120.9), QPointF(74.1, 109.8), QPointF(75.4, 106.6), QPointF(76.7, 106.0),
        QPointF(80.6, 98.8),  QPointF(80.0, 97.5),  QPointF(50.7, 97.5)
    };
    down.clear();
    QPolygonF poly_down;
    for (const auto &p : pts_down) poly_down << p;
    down.addPolygon(poly_down);
    down.closeSubpath();

    // 3. 中心圆角方块 (刹车/急停 Space)
    static const QPointF pts_center[] = {
        QPointF(53.3, 54.6), QPointF(53.3, 71.5), QPointF(52.6, 72.2), QPointF(53.3, 72.8),
        QPointF(53.3, 75.4), QPointF(55.9, 76.7), QPointF(73.5, 76.7), QPointF(74.8, 76.0),
        QPointF(76.7, 72.8), QPointF(76.7, 56.5), QPointF(75.4, 55.2), QPointF(75.4, 54.0),
        QPointF(74.1, 54.0), QPointF(73.5, 53.3), QPointF(55.2, 53.3)
    };
    center.clear();
    QPolygonF poly_center;
    for (const auto &p : pts_center) poly_center << p;
    center.addPolygon(poly_center);
    center.closeSubpath();

    // 4. 左转弧弯箭头 (左转 A)
    static const QPointF pts_left[] = {
        QPointF(5.2, 65.0),  QPointF(5.2, 65.7),  QPointF(11.7, 72.2), QPointF(12.3, 72.2),
        QPointF(18.2, 77.3), QPointF(19.5, 76.7), QPointF(19.5, 71.5), QPointF(20.8, 70.2),
        QPointF(23.4, 70.2), QPointF(25.4, 71.5), QPointF(29.2, 71.5), QPointF(30.6, 72.8),
        QPointF(31.9, 72.8), QPointF(34.5, 74.8), QPointF(36.4, 77.3), QPointF(37.7, 77.3),
        QPointF(38.4, 76.7), QPointF(38.4, 74.8), QPointF(37.0, 72.8), QPointF(37.0, 70.8),
        QPointF(34.5, 66.3), QPointF(29.9, 63.0), QPointF(28.6, 63.0), QPointF(27.3, 61.8),
        QPointF(24.7, 61.8), QPointF(22.8, 60.5), QPointF(20.1, 60.5), QPointF(19.5, 59.8),
        QPointF(19.5, 54.0), QPointF(18.9, 53.3), QPointF(11.1, 59.1), QPointF(11.1, 59.8)
    };
    left.clear();
    QPolygonF poly_left;
    for (const auto &p : pts_left) poly_left << p;
    left.addPolygon(poly_left);
    left.closeSubpath();

    // 5. 右转弧弯箭头 (右转 D)
    static const QPointF pts_right[] = {
        QPointF(125.5, 65.0), QPointF(124.2, 64.3), QPointF(117.7, 57.9), QPointF(111.2, 53.3),
        QPointF(110.5, 54.0), QPointF(110.5, 59.8), QPointF(109.8, 60.5), QPointF(107.2, 60.5),
        QPointF(105.3, 61.8), QPointF(102.7, 61.8), QPointF(101.4, 63.0), QPointF(100.1, 63.0),
        QPointF(98.2, 64.3),  QPointF(95.5, 67.0),  QPointF(93.0, 70.8),  QPointF(93.0, 77.3),
        QPointF(93.6, 77.3),  QPointF(94.9, 75.4),  QPointF(97.5, 73.5),  QPointF(101.4, 71.5),
        QPointF(104.7, 71.5), QPointF(106.6, 70.2), QPointF(109.8, 70.2), QPointF(110.5, 70.8),
        QPointF(110.5, 76.7), QPointF(111.8, 77.3), QPointF(117.0, 72.8), QPointF(117.7, 72.8),
        QPointF(122.8, 67.6), QPointF(123.5, 67.6)
    };
    right.clear();
    QPolygonF poly_right;
    for (const auto &p : pts_right) poly_right << p;
    right.addPolygon(poly_right);
    right.closeSubpath();

    // 6. 设置齿轮 (右下角 ⚙ 线框轮廓)
    settings.clear();
    const double gx = 109.5;
    const double gy = 109.2;
    QPolygonF gearPoly;
    const int numSteps = 72;
    for (int i = 0; i < numSteps; ++i) {
        double angle = i * 2.0 * M_PI / numSteps;
        double r = 11.7 + 1.8 * std::cos(6.0 * angle);
        gearPoly << QPointF(gx + r * std::cos(angle), gy + r * std::sin(angle));
    }
    settings.addPolygon(gearPoly);
    settings.closeSubpath();

    // 齿轮中心内圆
    settings.addEllipse(QPointF(gx, gy), 3.8, 3.8);
}

} // namespace DirectionPadGeometry
