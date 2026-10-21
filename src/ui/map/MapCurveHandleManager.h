/**
 * @file    MapCurveHandleManager.h
 * @layer   GUI
 * @brief   Manager for Bezier curve control handles, drag points, and path updates.
 * @deps    QObject, QGraphicsScene, QGraphicsEllipseItem, MapEditDataModel
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QObject>
#include <QMap>
#include <QList>
#include <QPointF>
#include <functional>

class QGraphicsScene;
class QGraphicsEllipseItem;
class QGraphicsItem;
class MapEditDataModel;

class MapCurveHandleManager : public QObject
{
    Q_OBJECT

public:
    using CoordToSceneFn = std::function<QPointF(double wx, double wy)>;
    using CoordToWorldFn = std::function<QPointF(double sx, double sy)>;
    using FeatureItemLookupFn = std::function<QGraphicsItem*(const QString &key)>;

    explicit MapCurveHandleManager(QGraphicsScene *scene,
                                  MapEditDataModel *model,
                                  CoordToSceneFn worldToScene,
                                  CoordToWorldFn sceneToWorld,
                                  FeatureItemLookupFn getFeatureItem,
                                  QObject *parent = nullptr);
    ~MapCurveHandleManager() override;

    // === [Section: Handle Lifecycle] ===
    void rebuildCurveHandles(const QString &id, bool isDragStatus);
    void removeAllHandles();
    void removeHandlesForCurve(const QString &id);
    void setHandlesVisible(bool visible);

    // === [Section: Shape Synchronization] ===
    void updateCurveShape(const QString &id);
    void flushCurveHandlesToJson();

    // === [Section: Handle Queries] ===
    const QMap<QString, QList<QGraphicsEllipseItem*>>& handles() const { return m_curveHandles; }
    QList<QGraphicsEllipseItem*> getHandles(const QString &id) const { return m_curveHandles.value(id); }

private:
    QGraphicsScene      *m_scene         = nullptr;
    MapEditDataModel    *m_model         = nullptr;
    CoordToSceneFn       m_worldToScene;
    CoordToWorldFn       m_sceneToWorld;
    FeatureItemLookupFn  m_getFeatureItem;

    QMap<QString, QList<QGraphicsEllipseItem*>> m_curveHandles;
    QList<QGraphicsItem*>                       m_armLines;
};
