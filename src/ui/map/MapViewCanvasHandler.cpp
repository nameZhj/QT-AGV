/**
 * @file    MapViewCanvasHandler.cpp
 * @layer   GUI
 * @brief   Coordinate label updates, reloc preview line dragging, and item hit test routing implementation.
 * @deps    MapViewCanvasHandler.h, MapCanvas.h, MapViewSceneRenderer.h, MapViewRelocController.h, MapViewNavInteraction.h, MapViewMarkHelper.h, RouteFeatureSelect.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapViewCanvasHandler.h"
#include "MapCanvas.h"
#include "MapViewSceneRenderer.h"
#include "MapViewRelocController.h"
#include "MapViewNavInteraction.h"
#include "MapViewMarkHelper.h"
#include "core/RouteFeatureSelect.h"
#include <QLabel>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QCursor>

MapViewCanvasHandler::MapViewCanvasHandler(QObject *parent)
    : QObject(parent)
{
}

void MapViewCanvasHandler::onTransformChanged(MapCanvas *canvas, QLabel *coordLabel,
                                             MapViewSceneRenderer *renderer,
                                             std::function<void()> updateRulersFn)
{
    if (updateRulersFn) updateRulersFn();
    if (!canvas || !coordLabel || !renderer) return;
    QPoint cp = canvas->mapFromGlobal(QCursor::pos());
    if (canvas->rect().contains(cp)) {
        QPointF w = renderer->sceneToWorld(canvas->mapToScene(cp).x(), canvas->mapToScene(cp).y());
        coordLabel->setText(QString("X: %1  Y: %2").arg(w.x(), 0, 'f', 2).arg(w.y(), 0, 'f', 2));
    }
}

void MapViewCanvasHandler::onSceneClicked(const QPointF &scenePos, Qt::MouseButton btn,
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
                                         QWidget *parentWidget)
{
    QPointF w = renderer->sceneToWorld(scenePos.x(), scenePos.y());
    if (coordLabel) coordLabel->setText(QString("X: %1  Y: %2").arg(w.x(), 0, 'f', 2).arg(w.y(), 0, 'f', 2));

    if (btn == Qt::RightButton) {
        if (rightClickMarkFn) rightClickMarkFn();
        return;
    }
    if (btn != Qt::LeftButton || rightTab != 0) return;

    if (navInteraction && navInteraction->isNavMode()) {
        double scale = canvas ? canvas->transform().m11() : 1.0;
        QGraphicsItem *pk = RouteFeatureSelect::pickItem(scene, scenePos, renderer->hitTolScene(12.0, scale));
        if (pk && !pk->data(0).toString().isEmpty() && pk->data(0).toString() != "line" && pk->data(0).toString() != "curve") {
            navInteraction->handleStationNavClick(pk->data(1).toString(), pk->data(0).toString(), pk->data(3).toString(), pk->data(4).toPointF(), parentWidget);
        }
        return;
    }

    if (relocCtrl && relocCtrl->isActive()) {
        relocCtrl->handlePress(w, scenePos, scene, QString(), setStatusFn);
        return;
    }

    double scale = canvas ? canvas->transform().m11() : 1.0;
    QGraphicsItem *pk = RouteFeatureSelect::pickItem(scene, scenePos, renderer->hitTolScene(12.0, scale));
    if (!pk && scene) {
        for (auto *it : scene->items(scenePos)) {
            if (it && it->data(0).toString() == "highLevelArea") { pk = it; break; }
        }
    }
    if (pk && selectFeatureFn) {
        selectFeatureFn(pk->data(0).toString(), pk->data(1).toString());
    } else if (clearSelectionFn) {
        clearSelectionFn();
    }
}

void MapViewCanvasHandler::onSceneDoubleClicked(const QPointF &scenePos, Qt::MouseButton btn,
                                               MapCanvas *canvas, QGraphicsScene *scene,
                                               MapViewSceneRenderer *renderer,
                                               int rightTab,
                                               std::function<void(const QString &, const QString &)> selectFeatureFn,
                                               std::function<void()> updateRulersFn)
{
    if (btn != Qt::LeftButton || rightTab != 0 || !scene || !canvas) return;
    double scale = canvas->transform().m11();
    QGraphicsItem *pk = RouteFeatureSelect::pickItem(scene, scenePos, renderer->hitTolScene(12.0, scale));
    if (!pk && scene) {
        for (auto *it : scene->items(scenePos)) {
            if (it && it->data(0).toString() == "highLevelArea") { pk = it; break; }
        }
    }
    if (pk) {
        canvas->scale(1.4, 1.4);
        canvas->centerOn(pk->sceneBoundingRect().center());
        if (updateRulersFn) updateRulersFn();
        if (selectFeatureFn) selectFeatureFn(pk->data(0).toString(), pk->data(1).toString());
    }
}

void MapViewCanvasHandler::onSceneMoved(const QPointF &scenePos,
                                       QLabel *coordLabel,
                                       MapViewSceneRenderer *renderer,
                                       MapViewRelocController *relocCtrl,
                                       QGraphicsScene *scene)
{
    QPointF w = renderer->sceneToWorld(scenePos.x(), scenePos.y());
    if (coordLabel) coordLabel->setText(QString("X: %1  Y: %2").arg(w.x(), 0, 'f', 2).arg(w.y(), 0, 'f', 2));
    if (relocCtrl && relocCtrl->isDragging()) {
        relocCtrl->handleMove(scenePos, scene, [renderer](double x, double y){ return renderer->worldToScene(x, y); });
    }
}

void MapViewCanvasHandler::onSceneReleased(const QPointF &scenePos, Qt::MouseButton btn,
                                          MapViewSceneRenderer *renderer,
                                          MapViewRelocController *relocCtrl,
                                          QGraphicsScene *scene,
                                          const QString &carId,
                                          const QString &mapId,
                                          double fallbackYaw,
                                          std::function<void(const QString &, const QString &)> setStatusFn,
                                          std::function<void()> onFinishedFn,
                                          QWidget *parentWidget)
{
    if (btn != Qt::LeftButton || !relocCtrl || !relocCtrl->isDragging()) return;
    QPointF w = renderer->sceneToWorld(scenePos.x(), scenePos.y());
    relocCtrl->handleRelease(w, scenePos, scene, carId, mapId, fallbackYaw, setStatusFn, onFinishedFn, parentWidget);
}
