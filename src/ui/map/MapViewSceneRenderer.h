/**
 * @file    MapViewSceneRenderer.h
 * @layer   GUI
 * @brief   Renderer for topological stations, lines, curves, areas, and grid axes on QGraphicsScene.
 * @deps    QObject, QGraphicsScene, QJsonObject, QJsonArray
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QObject>
#include <QPointF>
#include <QJsonObject>
#include <QJsonArray>
#include <QList>

class QGraphicsScene;
class QGraphicsItem;

class MapViewSceneRenderer : public QObject
{
    Q_OBJECT
public:
    explicit MapViewSceneRenderer(QObject *parent = nullptr);
    ~MapViewSceneRenderer() override = default;

    // Extents
    void setExtent(double wL, double wB, double wR, double wT, double imgW, double imgH);

    QPointF worldToScene(double wx, double wy) const;
    QPointF sceneToWorld(double sx, double sy) const;
    double hitTolScene(double screenPx, double scale) const;

    void renderAxesAndGrid(QGraphicsScene *scene, bool showGrid, QList<QGraphicsItem*> &gridItems);
    void clearGridItems(QGraphicsScene *scene, QList<QGraphicsItem*> &gridItems);

    void renderRoute(QGraphicsScene *scene,
                     const QJsonArray &points,
                     const QJsonArray &lines,
                     const QJsonArray &curves,
                     const QJsonArray &charges,
                     const QJsonArray &highPrecisionPoints,
                     const QJsonArray &highLevelAreas,
                     QList<QGraphicsItem*> &routeItems,
                     QGraphicsItem *&selItem);

    void clearRouteItems(QGraphicsScene *scene, QList<QGraphicsItem*> &routeItems, QGraphicsItem *&selItem);

    static void extractAreaBounds(const QJsonObject &obj, double &minX, double &maxX, double &minY, double &maxY);

private:
    double m_worldL = 0, m_worldB = 0, m_worldR = 0, m_worldT = 0;
    double m_imgW = 0, m_imgH = 0;
};
