/**
 * @file    MapDrawHelper.h
 * @module  GUILayer
 * @brief   Coordinate transformation and geometric primitives drawing helper for maps.
 * @deps    QPainter, QPointF, ThemeColors
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QPointF>
#include <QPolygonF>
#include <QColor>

class QPainter;

/**
 * @brief 地图坐标转换与公共图元绘制辅助类
 */
class MapDrawHelper
{
public:
    MapDrawHelper() = delete;

    /**
     * @brief 世界坐标转换为场景像素坐标 (考虑 Y 轴反转)
     */
    static QPointF worldToScene(double wx, double wy,
                                double worldL, double worldR,
                                double worldT, double worldB,
                                double imgW, double imgH);

    /**
     * @brief 场景像素坐标转换为世界坐标 (考虑 Y 轴反转)
     */
    static QPointF sceneToWorld(double sx, double sy,
                                double worldL, double worldR,
                                double worldT, double worldB,
                                double imgW, double imgH);

    /**
     * @brief 计算两点间的欧几里得距离
     */
    static double distance(const QPointF &p1, const QPointF &p2);

    /**
     * @brief 计算点到线段的最短距离及垂足
     */
    static double distancePointToSegment(const QPointF &pt, const QPointF &a, const QPointF &b, QPointF *closestPt = nullptr);

    /**
     * @brief 计算二次贝塞尔曲线采样点
     */
    static QPointF quadraticBezier(const QPointF &p0, const QPointF &p1, const QPointF &p2, double t);

    /**
     * @brief 计算三次贝塞尔曲线采样点
     */
    static QPointF cubicBezier(const QPointF &p0, const QPointF &p1, const QPointF &p2, const QPointF &p3, double t);

    /**
     * @brief 计算有向线段端点的箭头多边形
     */
    static QPolygonF makeArrowHead(const QPointF &from, const QPointF &to, double arrowLength = 10.0, double arrowAngleDeg = 25.0);

    /**
     * @brief 绘制带朝向指示的 AGV 车体图标
     */
    static void drawRobotPose(QPainter *painter, const QPointF &center, double radius, double headingRad,
                              const QColor &bodyColor = QColor(0x2d, 0x8c, 0xff),
                              const QColor &arrowColor = QColor(0xff, 0xff, 0xff));
};
