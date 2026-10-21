/**
 * @file    MapViewRobotMarker.cpp
 * @layer   GUI
 * @brief   Robot graphic marker rendering implementation.
 * @deps    MapViewRobotMarker.h, QSvgRenderer, QGraphicsPixmapItem, QGraphicsPolygonItem
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapViewRobotMarker.h"
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsPolygonItem>
#include <QSvgRenderer>
#include <QPainter>
#include <QCursor>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

MapViewRobotMarker::MapViewRobotMarker(QObject *parent)
    : QObject(parent)
{
    QSvgRenderer renderer(QString(":/ic-robot1.svg"));
    if (renderer.isValid()) {
        m_robotPixmap = QPixmap(180, 330);
        m_robotPixmap.fill(Qt::transparent);
        QPainter rp(&m_robotPixmap);
        rp.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        renderer.render(&rp);
    }
}

void MapViewRobotMarker::clear(QGraphicsScene *scene)
{
    if (m_robotItem) {
        if (scene && m_robotItem->scene() == scene) {
            scene->removeItem(m_robotItem);
        }
        delete m_robotItem;
        m_robotItem = nullptr;
    }
}

void MapViewRobotMarker::updateRobot(QGraphicsScene *scene, const QPointF &sp,
                                    double yawRadVal, double ppm)
{
    if (!scene) return;

    double validYawRad = (qAbs(yawRadVal) > M_PI * 2) ? yawRadVal * M_PI / 180.0 : yawRadVal;
    double yawDeg      = validYawRad * 180.0 / M_PI;
    double screenAngle = 90.0 - yawDeg;

    const double kRobotLength = 1.0;
    const double kRobotWidth  = 0.5;
    double sceneW = kRobotWidth * ppm;
    double sceneH = kRobotLength * ppm;

    if (!m_robotPixmap.isNull()) {
        const double srcW   = m_robotPixmap.width();
        const double srcH   = m_robotPixmap.height();
        const double pivotX = srcW / 2.0;
        const double pivotY = srcH / 2.0;
        const double scale  = (srcW > 0) ? (sceneW / srcW) : 1.0;

        auto *pixItem = dynamic_cast<QGraphicsPixmapItem *>(m_robotItem);
        if (pixItem) {
            pixItem->setScale(scale);
            pixItem->setTransformOriginPoint(pivotX, pivotY);
            pixItem->setPos(sp.x() - pivotX, sp.y() - pivotY);
            pixItem->setRotation(screenAngle);
            pixItem->setCursor(Qt::PointingHandCursor);
        } else {
            clear(scene);
            auto *robotItem = scene->addPixmap(m_robotPixmap);
            robotItem->setScale(scale);
            robotItem->setTransformOriginPoint(pivotX, pivotY);
            robotItem->setPos(sp.x() - pivotX, sp.y() - pivotY);
            robotItem->setRotation(screenAngle);
            robotItem->setZValue(120);
            robotItem->setCursor(Qt::PointingHandCursor);
            m_robotItem = robotItem;
        }
    } else {
        clear(scene);
        double halfW = sceneW / 2.0;
        double halfH = sceneH / 2.0;
        QPolygonF rectPoly;
        rectPoly << QPointF(-halfW, -halfH)
                 << QPointF(halfW, -halfH)
                 << QPointF(halfW, halfH)
                 << QPointF(-halfW, halfH);
        QTransform t;
        t.translate(sp.x(), sp.y());
        t.rotate(screenAngle);
        rectPoly = t.map(rectPoly);
        auto *marker = scene->addPolygon(rectPoly,
            QPen(QColor(56, 189, 248), 1.5),
            QBrush(QColor(30, 41, 59, 220)));
        marker->setZValue(120);
        marker->setCursor(Qt::PointingHandCursor);
        m_robotItem = marker;
    }
}
