/**
 * @file    MapViewCanvasHandler.h
 * @layer   GUI
 * @brief   Event handler for mouse picking, double-click zoom, reloc drag, and coordinate tracking on MapCanvas.
 * @deps    QObject, QPointF
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QObject>
#include <QPointF>
#include <functional>

class MapCanvas;
class QGraphicsScene;
class QLabel;
class MapViewSceneRenderer;
class MapViewRelocController;
class MapViewNavInteraction;
class MapViewMarkHelper;

class MapViewCanvasHandler : public QObject
{
    Q_OBJECT
public:
    explicit MapViewCanvasHandler(QObject *parent = nullptr);
    ~MapViewCanvasHandler() override = default;

    void onTransformChanged(MapCanvas *canvas, QLabel *coordLabel,
                            MapViewSceneRenderer *renderer,
                            std::function<void()> updateRulersFn);

    void onSceneClicked(const QPointF &scenePos, Qt::MouseButton btn,
                        MapCanvas *canvas, QGraphicsScene *scene,
                        QLabel *coordLabel, MapViewSceneRenderer *renderer,
                        MapViewRelocController *relocCtrl,
                        MapViewNavInteraction *navInteraction,
                        MapViewMarkHelper *markHelper,
                        int rightTab,
                        std::function<void(const QString &, const QString &)> selectFeatureFn,
                        std::function<void()> clearSelectionFn,
                        std::function<void(const QString &, const QString &)> setStatusFn,
                        std::function<void()> rightClickMarkFn,
                        QWidget *parentWidget);

    void onSceneDoubleClicked(const QPointF &scenePos, Qt::MouseButton btn,
                              MapCanvas *canvas, QGraphicsScene *scene,
                              MapViewSceneRenderer *renderer,
                              int rightTab,
                              std::function<void(const QString &, const QString &)> selectFeatureFn,
                              std::function<void()> updateRulersFn);

    void onSceneMoved(const QPointF &scenePos,
                      QLabel *coordLabel,
                      MapViewSceneRenderer *renderer,
                      MapViewRelocController *relocCtrl,
                      QGraphicsScene *scene);

    void onSceneReleased(const QPointF &scenePos, Qt::MouseButton btn,
                         MapViewSceneRenderer *renderer,
                         MapViewRelocController *relocCtrl,
                         QGraphicsScene *scene,
                         const QString &carId,
                         const QString &mapId,
                         double fallbackYaw,
                         std::function<void(const QString &, const QString &)> setStatusFn,
                         std::function<void()> onFinishedFn,
                         QWidget *parentWidget);
};
