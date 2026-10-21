/**
 * @file    MapDragController.cpp
 * @layer   GUI
 * @brief   Implementation of node dragging, area resizing, and curve handle manipulation.
 * @deps    MapDragController.h, MapEditDataModel.h, MapCurveHandleManager.h, MapSnapHelper.h, RouteNetworkValidator.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapDragController.h"
#include "MapEditDataModel.h"
#include "MapCurveHandleManager.h"
#include "MapSnapHelper.h"
#include "core/RouteNetworkValidator.h"
#include "core/RouteFeatureSelect.h"

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsItem>
#include <cmath>

MapDragController::MapDragController(QObject *parent)
    : QObject(parent)
{
}

MapEdit::AreaHitPart MapDragController::getAreaHitPart(const QPointF &sp, const QJsonObject &areaObj,
                                                      CoordToSceneFn w2s, double cornerTol, double edgeTol)
{
    if (areaObj.isEmpty() || !w2s) return MapEdit::AreaHit_None;
    double minX = 0, maxX = 0, minY = 0, maxY = 0;
    RouteNetworkValidator::extractAreaBounds(areaObj, minX, maxX, minY, maxY);
    QRectF r = QRectF(w2s(minX, maxY), w2s(maxX, minY)).normalized();

    if (std::hypot(sp.x() - r.left(), sp.y() - r.top()) <= cornerTol) return MapEdit::AreaHit_TopLeft;
    if (std::hypot(sp.x() - r.right(), sp.y() - r.top()) <= cornerTol) return MapEdit::AreaHit_TopRight;
    if (std::hypot(sp.x() - r.left(), sp.y() - r.bottom()) <= cornerTol) return MapEdit::AreaHit_BottomLeft;
    if (std::hypot(sp.x() - r.right(), sp.y() - r.bottom()) <= cornerTol) return MapEdit::AreaHit_BottomRight;

    if (std::abs(sp.x() - r.left()) <= edgeTol && sp.y() >= r.top() - edgeTol && sp.y() <= r.bottom() + edgeTol)
        return MapEdit::AreaHit_LeftEdge;
    if (std::abs(sp.x() - r.right()) <= edgeTol && sp.y() >= r.top() - edgeTol && sp.y() <= r.bottom() + edgeTol)
        return MapEdit::AreaHit_RightEdge;
    if (std::abs(sp.y() - r.top()) <= edgeTol && sp.x() >= r.left() - edgeTol && sp.x() <= r.right() + edgeTol)
        return MapEdit::AreaHit_TopEdge;
    if (std::abs(sp.y() - r.bottom()) <= edgeTol && sp.x() >= r.left() - edgeTol && sp.x() <= r.right() + edgeTol)
        return MapEdit::AreaHit_BottomEdge;

    if (r.contains(sp)) return MapEdit::AreaHit_Body;
    return MapEdit::AreaHit_None;
}

bool MapDragController::handleMousePress(const QPointF &sp, const QString &selType, const QString &selId)
{
    const double tol = MapSnapHelper::hitTolScene(m_view, 10.0);

    // 1. 检查曲线控制点手柄
    if (m_handleMgr) {
        const auto &handlesMap = m_handleMgr->handles();
        for (auto lit = handlesMap.begin(); lit != handlesMap.end(); ++lit) {
            const auto &handles = lit.value();
            for (int i = 0; i < handles.size(); ++i) {
                if (!handles[i]) continue;
                if (std::hypot(sp.x() - handles[i]->pos().x(), sp.y() - handles[i]->pos().y()) < tol) {
                    m_activeHandle = handles[i];
                    m_handleCurveId = lit.key();
                    m_handleIndex = i;
                    m_isDraggingHandle = true;
                    if (m_view) m_view->viewport()->setCursor(Qt::ClosedHandCursor);
                    return true;
                }
            }
        }
    }

    // 2. 检查站点节点 (point / charge / highPrecision)
    MapEdit::SnapResult nodeSnap = MapSnapHelper::findSnap(sp, m_model, m_w2s, 0.4);
    if (!nodeSnap.valid && m_scene) {
        for (auto *it : m_scene->items(QRectF(sp.x() - tol, sp.y() - tol, tol * 2, tol * 2))) {
            QGraphicsItem *target = it;
            if (!target->data(0).isValid() && target->parentItem()) target = target->parentItem();
            QString type = target->data(0).toString();
            QString id = target->data(1).toString();
            if (type == "point" || type == "charge" || type == "highPrecision") {
                nodeSnap = {true, type, id, target->pos(), m_s2w ? m_s2w(target->pos().x(), target->pos().y()) : target->pos()};
                break;
            }
        }
    }
    if (nodeSnap.valid && (nodeSnap.type == "point" || nodeSnap.type == "charge" || nodeSnap.type == "highPrecision")) {
        m_isDraggingNode = true;
        m_draggingNodeType = nodeSnap.type;
        m_draggingNodeId = nodeSnap.id;
        m_dragStartScenePos = nodeSnap.scenePos;
        m_dragStartWorldPos = nodeSnap.worldPos;
        emit featureSelected(nodeSnap.type, nodeSnap.id);
        if (m_view) m_view->viewport()->setCursor(Qt::ClosedHandCursor);
        return true;
    }

    // 3. 检查选中的高级区域
    if (selType == "highLevelArea" && m_model) {
        QJsonObject selArea = m_model->findFeature("highLevelArea", selId);
        if (!selArea.isEmpty()) {
            double cTol = MapSnapHelper::hitTolScene(m_view, 10.0), eTol = MapSnapHelper::hitTolScene(m_view, 6.0);
            MapEdit::AreaHitPart hp = getAreaHitPart(sp, selArea, m_w2s, cTol, eTol);
            QGraphicsItem *ptTarget = m_scene ? RouteFeatureSelect::pickItem(m_scene, sp, cTol) : nullptr;
            if (!ptTarget && hp != MapEdit::AreaHit_None) {
                m_isDraggingArea = true;
                m_areaDragPart = hp;
                m_areaDragStartScenePos = sp;
                RouteNetworkValidator::extractAreaBounds(selArea, m_areaDragStartMinX, m_areaDragStartMaxX, m_areaDragStartMinY, m_areaDragStartMaxY);
                return true;
            }
        }
    }

    return false;
}

bool MapDragController::handleMouseMove(const QPointF &sp, const QString &selType, const QString &selId)
{
    if (m_isDraggingArea && selType == "highLevelArea") {
        dragMoveArea(sp, selId);
        return true;
    }
    if (m_isDraggingNode) {
        dragMoveNode(sp);
        return true;
    }
    if (m_isDraggingHandle && m_activeHandle) {
        dragMoveCurveHandle(sp);
        return true;
    }
    return false;
}

void MapDragController::dragMoveNode(const QPointF &sp)
{
    if (!m_s2w || !m_model) return;
    QPointF newWorld = m_s2w(sp.x(), sp.y());
    QJsonObject o = m_model->findFeature(m_draggingNodeType, m_draggingNodeId);
    if (!o.isEmpty()) {
        o["x"] = newWorld.x();
        o["y"] = newWorld.y();
        m_model->updateFeature(m_draggingNodeType, m_draggingNodeId, o);
        m_model->updateConnectedRoutesPos(m_draggingNodeId, newWorld.x(), newWorld.y(), true, true);
        emit dataChanged();
    }
}

void MapDragController::dragMoveArea(const QPointF &sp, const QString &selId)
{
    if (!m_s2w || !m_model) return;
    QJsonObject areaObj = m_model->findFeature("highLevelArea", selId);
    if (areaObj.isEmpty()) return;

    QPointF pStart = m_s2w(m_areaDragStartScenePos.x(), m_areaDragStartScenePos.y());
    QPointF pCur = m_s2w(sp.x(), sp.y());
    double dx = pCur.x() - pStart.x(), dy = pCur.y() - pStart.y();

    double nMinX = m_areaDragStartMinX, nMaxX = m_areaDragStartMaxX;
    double nMinY = m_areaDragStartMinY, nMaxY = m_areaDragStartMaxY;

    switch (m_areaDragPart) {
    case MapEdit::AreaHit_Body: nMinX += dx; nMaxX += dx; nMinY += dy; nMaxY += dy; break;
    case MapEdit::AreaHit_LeftEdge: nMinX = qMin(nMinX + dx, nMaxX - 0.2); break;
    case MapEdit::AreaHit_RightEdge: nMaxX = qMax(nMaxX + dx, nMinX + 0.2); break;
    case MapEdit::AreaHit_TopEdge: nMaxY = qMax(nMaxY + dy, nMinY + 0.2); break;
    case MapEdit::AreaHit_BottomEdge: nMinY = qMin(nMinY + dy, nMaxY - 0.2); break;
    case MapEdit::AreaHit_TopLeft: nMinX = qMin(nMinX + dx, nMaxX - 0.2); nMaxY = qMax(nMaxY + dy, nMinY + 0.2); break;
    case MapEdit::AreaHit_TopRight: nMaxX = qMax(nMaxX + dx, nMinX + 0.2); nMaxY = qMax(nMaxY + dy, nMinY + 0.2); break;
    case MapEdit::AreaHit_BottomLeft: nMinX = qMin(nMinX + dx, nMaxX - 0.2); nMinY = qMin(nMinY + dy, nMaxY - 0.2); break;
    case MapEdit::AreaHit_BottomRight: nMaxX = qMax(nMaxX + dx, nMinX + 0.2); nMinY = qMin(nMinY + dy, nMaxY - 0.2); break;
    default: break;
    }

    QJsonArray coords, pTL, pBR;
    pTL.append(nMinX); pTL.append(nMaxY);
    pBR.append(nMaxX); pBR.append(nMinY);
    coords.append(pTL); coords.append(pBR);
    areaObj["coordinates"] = coords;

    m_model->updateFeature("highLevelArea", selId, areaObj);
    emit dataChanged();
    emit areaBoundsChanged(nMinX, nMaxX, nMinY, nMaxY);
}

void MapDragController::dragMoveCurveHandle(const QPointF &sp)
{
    if (!m_activeHandle || !m_handleMgr) return;
    if (m_handleIndex == 0 || m_handleIndex == 2) {
        MapEdit::SnapResult snap = MapSnapHelper::findSnap(sp, m_model, m_w2s);
        m_activeHandle->setPos(snap.valid ? snap.scenePos : sp);
    } else {
        m_activeHandle->setPos(sp);
    }
    m_handleMgr->updateCurveShape(m_handleCurveId);
}

bool MapDragController::handleMouseRelease(const QPointF &sp, const QString &selType, const QString &selId)
{
    Q_UNUSED(sp);
    if (m_isDraggingArea) {
        m_isDraggingArea = false;
        m_areaDragPart = MapEdit::AreaHit_None;
        emit areaDragCommitted(selId);
        return true;
    }
    if (m_isDraggingNode) {
        m_isDraggingNode = false;
        emit nodeDragCommitted(m_draggingNodeType, m_draggingNodeId);
        if (m_view) m_view->viewport()->setCursor(Qt::OpenHandCursor);
        return true;
    }
    if (m_isDraggingHandle) {
        m_isDraggingHandle = false;
        m_activeHandle = nullptr;
        if (m_handleMgr) m_handleMgr->flushCurveHandlesToJson();
        emit curveHandleDragCommitted(m_handleCurveId);
        return true;
    }
    return false;
}

void MapDragController::updateHoverCursor(const QPointF &sp, const QString &selType, const QString &selId)
{
    if (!m_view) return;
    const double tol = MapSnapHelper::hitTolScene(m_view, 10.0);

    if (m_handleMgr) {
        for (const auto &handles : m_handleMgr->handles()) {
            for (auto *h : handles) {
                if (h && std::hypot(sp.x() - h->pos().x(), sp.y() - h->pos().y()) < tol) {
                    m_view->viewport()->setCursor(Qt::SizeAllCursor);
                    return;
                }
            }
        }
    }

    if (selType == "highLevelArea" && m_model) {
        QJsonObject selArea = m_model->findFeature("highLevelArea", selId);
        if (!selArea.isEmpty()) {
            double cTol = MapSnapHelper::hitTolScene(m_view, 10.0), eTol = MapSnapHelper::hitTolScene(m_view, 6.0);
            MapEdit::AreaHitPart hp = getAreaHitPart(sp, selArea, m_w2s, cTol, eTol);
            switch (hp) {
            case MapEdit::AreaHit_TopLeft: case MapEdit::AreaHit_BottomRight: m_view->viewport()->setCursor(Qt::SizeFDiagCursor); return;
            case MapEdit::AreaHit_TopRight: case MapEdit::AreaHit_BottomLeft: m_view->viewport()->setCursor(Qt::SizeBDiagCursor); return;
            case MapEdit::AreaHit_LeftEdge: case MapEdit::AreaHit_RightEdge: m_view->viewport()->setCursor(Qt::SizeHorCursor); return;
            case MapEdit::AreaHit_TopEdge: case MapEdit::AreaHit_BottomEdge: m_view->viewport()->setCursor(Qt::SizeVerCursor); return;
            case MapEdit::AreaHit_Body: m_view->viewport()->setCursor(Qt::SizeAllCursor); return;
            default: break;
            }
        }
    }

    MapEdit::SnapResult nodeSnap = MapSnapHelper::findSnap(sp, m_model, m_w2s, 0.4);
    if (nodeSnap.valid && (nodeSnap.type == "point" || nodeSnap.type == "charge" || nodeSnap.type == "highPrecision")) {
        m_view->viewport()->setCursor(Qt::SizeAllCursor);
        return;
    }
    m_view->viewport()->setCursor(Qt::OpenHandCursor);
}
