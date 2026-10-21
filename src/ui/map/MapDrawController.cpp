/**
 * @file    MapDrawController.cpp
 * @layer   GUI
 * @brief   Implementation of map topology drawing state machine and rubber band previews.
 * @deps    MapDrawController.h, MapEditDataModel.h, MapSnapHelper.h, MapFeatureFactory.h, MapPgmUtil.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapDrawController.h"
#include "MapEditDataModel.h"
#include "MapSnapHelper.h"
#include "MapFeatureFactory.h"
#include "utils/MapPgmUtil.h"
#include "core/AlignConfigManager.h"

#include <QGraphicsScene>
#include <QGraphicsLineItem>
#include <QGraphicsRectItem>
#include <QPen>
#include <QBrush>

MapDrawController::MapDrawController(QObject *parent)
    : QObject(parent)
{
}

void MapDrawController::setDrawMode(int mode)
{
    cancelDraw();
    m_drawStatus = mode;
}

void MapDrawController::cancelDraw()
{
    m_drawHasP1 = false;
    m_drawP1 = {};
    m_drawP1Id.clear();
    m_curveCtrlPts.clear();
    m_curveCtrlIds.clear();
    m_isDrawingArea = false;
    if (m_areaPreviewItem) {
        if (m_scene) m_scene->removeItem(m_areaPreviewItem);
        delete m_areaPreviewItem;
        m_areaPreviewItem = nullptr;
    }
    clearPreview();
}

void MapDrawController::clearPreview()
{
    if (m_previewItem) {
        if (m_scene) m_scene->removeItem(m_previewItem);
        delete m_previewItem;
        m_previewItem = nullptr;
    }
    for (auto *ln : m_previewAids) {
        if (m_scene) m_scene->removeItem(ln);
        delete ln;
    }
    m_previewAids.clear();
}

void MapDrawController::updatePreview(const QPointF &sp)
{
    if (!m_scene) return;
    clearPreview();

    if (m_drawStatus == MapEdit::Curve && !m_curveCtrlPts.isEmpty()) {
        QList<QPointF> pts = m_curveCtrlPts;
        pts.append(sp);
        if (pts.size() >= 2) {
            QPainterPath path = MapPgmUtil::getBezierPath(pts);
            QPen pen(QColor("#9B59B6"), 1.5, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin);
            m_previewItem = m_scene->addPath(path, pen);
            m_previewItem->setZValue(100);

            QPen aidPen(QColor(170, 185, 215, 180), 1.0, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin);
            for (int i = 0; i + 1 < pts.size(); ++i) {
                auto *ln = m_scene->addLine(pts[i].x(), pts[i].y(), pts[i + 1].x(), pts[i + 1].y(), aidPen);
                ln->setZValue(99);
                m_previewAids.append(ln);
            }
        }
        return;
    }

    if (!m_drawHasP1) return;
    if (m_drawStatus == MapEdit::Line || m_drawStatus == MapEdit::Curve) {
        QPen pen(m_drawStatus == MapEdit::Curve ? QColor("#9B59B6") : QColor("#5DADE2"), 1.5, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin);
        m_previewItem = m_scene->addLine(m_drawP1.x(), m_drawP1.y(), sp.x(), sp.y(), pen);
        m_previewItem->setZValue(100);
    }
}

bool MapDrawController::handleMouseMove(const QPointF &sp)
{
    if (m_isDrawingArea && m_areaPreviewItem) {
        m_areaPreviewItem->setRect(QRectF(m_areaStartScenePos, sp).normalized());
        return true;
    }
    return false;
}

bool MapDrawController::handleMouseRelease(const QPointF &sp)
{
    if (!m_isDrawingArea) return false;
    m_isDrawingArea = false;
    if (m_areaPreviewItem) {
        if (m_scene) m_scene->removeItem(m_areaPreviewItem);
        delete m_areaPreviewItem;
        m_areaPreviewItem = nullptr;
    }

    QRectF r = QRectF(m_areaStartScenePos, sp).normalized();
    if (r.width() >= 5.0 && r.height() >= 5.0 && m_s2w && m_model) {
        QPointF tl = m_s2w(r.left(), r.top()), br = m_s2w(r.right(), r.bottom());
        double minX = qMin(tl.x(), br.x()), maxX = qMax(tl.x(), br.x());
        double minY = qMin(tl.y(), br.y()), maxY = qMax(tl.y(), br.y());

        QString newId = MapFeatureFactory::addHighLevelArea(m_model, minX, maxX, minY, maxY);
        emit dataChanged();
        emit showPropertiesRequested("highLevelArea", newId);
        emit modeChangeRequested(MapEdit::Drag);
    }
    return true;
}

bool MapDrawController::handleMousePress(const QPointF &sp, const MapEdit::SnapResult &snap)
{
    QPointF usePos = snap.valid ? snap.scenePos : sp;
    QString useId  = snap.valid ? snap.id       : QString();

    switch (m_drawStatus) {
    case MapEdit::Point:
        if (MapSnapHelper::hasOverlap(sp, m_model, m_s2w)) emit selectFeatureRequested(sp);
        else addPoint(sp);
        break;
    case MapEdit::Charge:
        if (MapSnapHelper::hasOverlap(sp, m_model, m_s2w)) emit selectFeatureRequested(sp);
        else addCharge(sp, AlignConfigManager::instance().epcX(), 0.0, 0.0);
        break;
    case MapEdit::HighPrecision:
        if (MapSnapHelper::hasOverlap(sp, m_model, m_s2w)) emit selectFeatureRequested(sp);
        else addHighPrecisionPoint(sp, AlignConfigManager::instance().epcX(), 0.0, 0.0);
        break;
    case MapEdit::Line: {
        if (!m_drawHasP1) {
            MapEdit::SnapResult head = MapSnapHelper::ensureNodeAt(usePos, m_model, m_w2s, m_s2w);
            m_drawP1 = head.scenePos; m_drawP1Id = head.id; m_drawHasP1 = true;
        } else {
            MapEdit::SnapResult tail = MapSnapHelper::ensureNodeAt(usePos, m_model, m_w2s, m_s2w);
            if (tail.id == m_drawP1Id) {
                emit warningToast("起点与终点不能为同一站点"); clearPreview();
            } else if (m_model->hasRouteBetween(m_drawP1Id, tail.id, m_drawP1, tail.scenePos)) {
                emit warningToast("两站点之间已存在连线，不可重复绘制"); clearPreview();
            } else {
                addLine(m_drawP1, tail.scenePos, m_drawP1Id, tail.id);
                m_drawP1 = tail.scenePos; m_drawP1Id = tail.id; m_drawHasP1 = true;
                clearPreview();
            }
        }
        break;
    }
    case MapEdit::Curve: {
        int n = m_curveCtrlPts.size();
        if (n == 0) {
            MapEdit::SnapResult head = MapSnapHelper::ensureNodeAt(usePos, m_model, m_w2s, m_s2w);
            m_curveCtrlPts.append(head.scenePos); m_curveCtrlIds.append(head.id);
            m_drawP1 = head.scenePos; m_drawP1Id = head.id; m_drawHasP1 = true;
        } else if (n < 3) {
            m_curveCtrlPts.append(usePos); m_curveCtrlIds.append(useId);
        } else if (n == 3) {
            MapEdit::SnapResult tail = MapSnapHelper::ensureNodeAt(usePos, m_model, m_w2s, m_s2w);
            QString sId = m_curveCtrlIds.isEmpty() ? QString() : m_curveCtrlIds.first();
            if (!sId.isEmpty() && tail.id == sId) {
                emit warningToast("起点与终点不能为同一站点"); cancelDraw();
            } else if (!sId.isEmpty() && m_model->hasRouteBetween(sId, tail.id, m_curveCtrlPts.first(), tail.scenePos)) {
                emit warningToast("两站点之间已存在连线，不可重复绘制"); cancelDraw();
            } else {
                m_curveCtrlPts.append(tail.scenePos); m_curveCtrlIds.append(tail.id); finalizeCurve();
            }
        }
        break;
    }
    case MapEdit::Area: {
        m_isDrawingArea = true;
        m_areaStartScenePos = sp;
        if (m_areaPreviewItem && m_scene) {
            m_scene->removeItem(m_areaPreviewItem); delete m_areaPreviewItem;
        }
        if (m_scene) {
            m_areaPreviewItem = m_scene->addRect(QRectF(sp, sp), QPen(QColor("#8b5cf6"), 2.0, Qt::DashLine), QBrush(QColor(139, 92, 246, 30)));
            m_areaPreviewItem->setZValue(100);
        }
        return true;
    }
    default: break;
    }
    return true;
}

void MapDrawController::addPoint(const QPointF &sp, double yaw, double theta)
{
    if (!m_s2w || !m_model) return;
    MapFeatureFactory::addPoint(m_model, m_s2w(sp.x(), sp.y()), yaw, theta);
    emit dataChanged();
}

void MapDrawController::addCharge(const QPointF &sp, double epcX, double epcY, double epcTheta)
{
    if (!m_s2w || !m_model) return;
    MapFeatureFactory::addCharge(m_model, m_s2w(sp.x(), sp.y()), epcX, epcY, epcTheta);
    emit dataChanged();
}

void MapDrawController::addHighPrecisionPoint(const QPointF &sp, double epcX, double epcY, double epcTheta)
{
    if (!m_s2w || !m_model) return;
    MapFeatureFactory::addHighPrecisionPoint(m_model, m_s2w(sp.x(), sp.y()), epcX, epcY, epcTheta);
    emit dataChanged();
}

void MapDrawController::addLine(const QPointF &a, const QPointF &b, const QString &startId, const QString &endId)
{
    if (!m_s2w || !m_model) return;
    MapFeatureFactory::addLine(m_model, m_s2w(a.x(), a.y()), m_s2w(b.x(), b.y()), startId, endId);
    emit dataChanged();
}

void MapDrawController::finalizeCurve()
{
    if (m_curveCtrlPts.size() < 2) { cancelDraw(); return; }
    if (m_curveCtrlIds.isEmpty() || m_curveCtrlIds.first().isEmpty()) {
        MapEdit::SnapResult head = MapSnapHelper::ensureNodeAt(m_curveCtrlPts.first(), m_model, m_w2s, m_s2w);
        m_curveCtrlPts.first() = head.scenePos; m_curveCtrlIds.first() = head.id;
    }
    if (m_curveCtrlIds.size() < 2 || m_curveCtrlIds.last().isEmpty()) {
        MapEdit::SnapResult tail = MapSnapHelper::ensureNodeAt(m_curveCtrlPts.last(), m_model, m_w2s, m_s2w);
        m_curveCtrlPts.last() = tail.scenePos; m_curveCtrlIds.last() = tail.id;
    }

    QString startId = m_curveCtrlIds.first(), endId = m_curveCtrlIds.last();
    if (startId == endId) {
        emit warningToast("起点与终点不能为同一站点"); cancelDraw(); return;
    }
    if (m_model->hasRouteBetween(startId, endId, m_curveCtrlPts.first(), m_curveCtrlPts.last())) {
        emit warningToast("两站点之间已存在连线，不可重复绘制"); cancelDraw(); return;
    }

    QList<QPointF> pts = m_curveCtrlPts;
    QList<QString> ids = m_curveCtrlIds;
    cancelDraw();
    addBezierCurve(pts, ids);
}

void MapDrawController::addBezierCurve(const QList<QPointF> &scenePts, const QList<QString> &snapIds)
{
    if (scenePts.size() < 2 || !m_s2w || !m_model) return;
    QList<QPointF> worldPts;
    for (const auto &sp : scenePts) worldPts.append(m_s2w(sp.x(), sp.y()));
    QString startId = snapIds.isEmpty() ? QString() : snapIds.first();
    QString endId   = (snapIds.size() >= 2) ? snapIds.last() : QString();
    MapFeatureFactory::addBezierCurve(m_model, worldPts, startId, endId);
    emit dataChanged();
}
