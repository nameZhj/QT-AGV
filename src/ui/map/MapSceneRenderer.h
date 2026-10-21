/**
 * @file    MapSceneRenderer.h
 * @layer   GUI
 * @brief   Graphics scene rendering engine for map background, nodes, routes, areas, and grid.
 * @deps    QObject, QGraphicsScene, MapEditDataModel, MapCurveHandleManager
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QObject>
#include <QMap>
#include <QPointF>
#include <QImage>
#include <QColor>
#include <functional>

class QGraphicsScene;
class QGraphicsItem;
class QGraphicsPixmapItem;
class QGraphicsLineItem;
class MapEditDataModel;
class MapCurveHandleManager;

class MapSceneRenderer : public QObject
{
    Q_OBJECT

public:
    using CoordToSceneFn = std::function<QPointF(double wx, double wy)>;

    explicit MapSceneRenderer(QGraphicsScene *scene,
                              MapEditDataModel *model,
                              CoordToSceneFn worldToScene,
                              QObject *parent = nullptr);
    ~MapSceneRenderer() override = default;

    void setCurveHandleManager(MapCurveHandleManager *mgr) { m_curveHandleMgr = mgr; }

    // === [Section: Whole Scene & Layer Rendering] ===
    void renderMap(const QImage &mapImage, double imgW, double imgH,
                   const QString &selType, const QString &selId,
                   bool showReflectors, bool isDragStatus);
    void renderHighLevelAreas(const QString &selType, const QString &selId, double hitTolSize);
    void renderStationNodes(double pxPerMeterX, double pxPerMeterY);
    void renderRouteLines();
    void renderRouteCurves(bool isDragStatus);
    void renderReflectorPoles();

    // === [Section: Grid & Background] ===
    void updateGrid(bool showGrid, double worldL, double worldB, double worldR, double worldT,
                    double imgW, double imgH);
    void setBackgroundPixmap(const QPixmap &pixmap);

    // === [Section: Item Queries] ===
    QGraphicsItem* getFeatureItem(const QString &key) const { return m_featureItems.value(key, nullptr); }
    const QMap<QString, QGraphicsItem*>& featureItems() const { return m_featureItems; }
    void clearFeatureItems() { m_featureItems.clear(); }
    void setFeatureItem(const QString &key, QGraphicsItem *item) { m_featureItems[key] = item; }
    void removeFeatureItem(const QString &key) { m_featureItems.remove(key); }

private:
    QGraphicsScene         *m_scene          = nullptr;
    MapEditDataModel       *m_model          = nullptr;
    MapCurveHandleManager  *m_curveHandleMgr = nullptr;
    CoordToSceneFn          m_worldToScene;

    QMap<QString, QGraphicsItem*> m_featureItems;
    QGraphicsPixmapItem          *m_mapBgItem = nullptr;
    QList<QGraphicsLineItem*>     m_gridLines;
};
