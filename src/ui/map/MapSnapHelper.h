/**
 * @file    MapSnapHelper.h
 * @layer   GUI
 * @brief   Snapping and coordinate proximity helpers for node and endpoint alignment.
 * @deps    MapEditTypes.h, MapEditDataModel.h, QGraphicsView
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include "MapEditTypes.h"
#include <functional>
#include <QPointF>

class QGraphicsView;
class MapEditDataModel;

namespace MapSnapHelper {

using CoordToSceneFn = std::function<QPointF(double wx, double wy)>;
using SceneToWorldFn = std::function<QPointF(double sx, double sy)>;

double hitTolScene(const QGraphicsView *view, double screenPx);

MapEdit::SnapResult findSnap(const QPointF &sp,
                             const MapEditDataModel *model,
                             CoordToSceneFn worldToScene,
                             double worldTol = 0.3);

bool hasOverlap(const QPointF &sp,
                const MapEditDataModel *model,
                SceneToWorldFn sceneToWorld,
                double tol = 0.3);

MapEdit::SnapResult ensureNodeAt(const QPointF &sp,
                                 MapEditDataModel *model,
                                 CoordToSceneFn worldToScene,
                                 SceneToWorldFn sceneToWorld);

} // namespace MapSnapHelper
