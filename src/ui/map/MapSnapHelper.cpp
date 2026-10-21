/**
 * @file    MapSnapHelper.cpp
 * @layer   GUI
 * @brief   Implementation of snapping and hit-tolerance calculations.
 * @deps    MapSnapHelper.h, MapEditDataModel.h, MapDataSerializer.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapSnapHelper.h"
#include "MapEditDataModel.h"
#include "MapDataSerializer.h"
#include <QGraphicsView>
#include <cmath>

namespace MapSnapHelper {

double hitTolScene(const QGraphicsView *view, double screenPx)
{
    if (!view) return screenPx;
    double s = view->transform().m11();
    return s > 1e-6 ? screenPx / s : screenPx;
}

MapEdit::SnapResult findSnap(const QPointF &sp,
                             const MapEditDataModel *model,
                             CoordToSceneFn worldToScene,
                             double worldTol)
{
    MapEdit::SnapResult best;
    if (!model || !worldToScene) return best;

    QPointF w0 = worldToScene(0.0, 0.0);
    QPointF w1 = worldToScene(1.0, 0.0);
    double scenePerWorld = std::hypot(w1.x() - w0.x(), w1.y() - w0.y());
    double bestDist = (scenePerWorld > 1e-6) ? worldTol * scenePerWorld : worldTol;

    auto checkArr = [&](const QJsonArray &arr, const QString &type) {
        for (const auto &v : arr) {
            QJsonObject o = v.toObject();
            double wx = o.value("x").toDouble(), wy = o.value("y").toDouble();
            QPointF snap = worldToScene(wx, wy);
            double dist = std::hypot(sp.x() - snap.x(), sp.y() - snap.y());
            if (dist < bestDist) {
                bestDist = dist;
                best.valid    = true;
                best.type     = type;
                best.id       = o.value("id").toString();
                best.scenePos = snap;
                best.worldPos = {wx, wy};
            }
        }
    };

    checkArr(model->points(), "point");
    checkArr(model->charges(), "charge");
    checkArr(model->highPrecisionPoints(), "highPrecision");

    auto checkEndpoint = [&](double wx, double wy, const QString &id, const QString &type) {
        QPointF snap = worldToScene(wx, wy);
        double dist = std::hypot(sp.x() - snap.x(), sp.y() - snap.y());
        if (dist < bestDist) {
            bestDist = dist;
            best.valid    = true;
            best.type     = type;
            best.id       = id;
            best.scenePos = snap;
            best.worldPos = {wx, wy};
        }
    };

    for (const auto &v : model->lines()) {
        QJsonObject o = v.toObject();
        QString id = o.value("id").toString();
        checkEndpoint(o.value("x1").toDouble(), o.value("y1").toDouble(), id, "line");
        checkEndpoint(o.value("x2").toDouble(), o.value("y2").toDouble(), id, "line");
    }
    for (const auto &v : model->curves()) {
        QJsonObject o = v.toObject();
        QString id = o.value("id").toString();
        checkEndpoint(o.value("x1").toDouble(), o.value("y1").toDouble(), id, "curve");
        checkEndpoint(o.value("x2").toDouble(), o.value("y2").toDouble(), id, "curve");
    }

    return best;
}

bool hasOverlap(const QPointF &sp,
                const MapEditDataModel *model,
                SceneToWorldFn sceneToWorld,
                double tol)
{
    if (!model || !sceneToWorld) return false;
    QPointF wc = sceneToWorld(sp.x(), sp.y());

    auto chk = [&](const QJsonArray &arr) {
        for (const auto &v : arr) {
            QJsonObject o = v.toObject();
            if (std::hypot(o.value("x").toDouble() - wc.x(),
                           o.value("y").toDouble() - wc.y()) < tol) {
                return true;
            }
        }
        return false;
    };

    return chk(model->points()) || chk(model->charges()) || chk(model->highPrecisionPoints());
}

MapEdit::SnapResult ensureNodeAt(const QPointF &sp,
                                 MapEditDataModel *model,
                                 CoordToSceneFn worldToScene,
                                 SceneToWorldFn sceneToWorld)
{
    if (!model || !worldToScene || !sceneToWorld) return {};

    MapEdit::SnapResult snap = findSnap(sp, model, worldToScene);
    if (snap.valid) return snap;

    QPointF wc = sceneToWorld(sp.x(), sp.y());
    int nn = MapDataSerializer::nextIdNum(model->points(), "p_");
    QJsonObject pt;
    pt["id"] = QString("p_%1").arg(nn);
    pt["name"] = QString("LM%1").arg(nn);
    pt["x"] = wc.x();
    pt["y"] = wc.y();
    pt["yaw"] = 0.0;
    pt["theta"] = 0.0;
    pt["robotYaw"] = 0.0;
    pt["rdsDockable"] = true;
    pt["allowRotate"] = true;
    pt["useDownPGV"] = false;

    model->addFeature("point", pt);

    MapEdit::SnapResult r;
    r.valid    = true;
    r.type     = "point";
    r.id       = pt["id"].toString();
    r.worldPos = wc;
    r.scenePos = sp;
    return r;
}

} // namespace MapSnapHelper
