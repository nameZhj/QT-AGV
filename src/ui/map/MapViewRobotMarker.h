/**
 * @file    MapViewRobotMarker.h
 * @layer   GUI
 * @brief   Robot graphic marker rendering with SVG pixmap, orientation, and real metric sizing.
 * @deps    QObject, QGraphicsItem, QPixmap
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QObject>
#include <QPixmap>
#include <QPointF>

class QGraphicsScene;
class QGraphicsItem;

class MapViewRobotMarker : public QObject
{
    Q_OBJECT
public:
    explicit MapViewRobotMarker(QObject *parent = nullptr);
    ~MapViewRobotMarker() override = default;

    void updateRobot(QGraphicsScene *scene, const QPointF &scenePos,
                     double yawRad, double ppm);

    void clear(QGraphicsScene *scene);

    QGraphicsItem* item() const { return m_robotItem; }

private:
    QPixmap        m_robotPixmap;
    QGraphicsItem *m_robotItem = nullptr;
};
