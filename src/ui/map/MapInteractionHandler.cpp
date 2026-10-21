/**
 * @file    MapInteractionHandler.cpp
 * @layer   GUI
 * @brief   Implementation of viewport event filter and interaction dispatcher.
 * @deps    MapInteractionHandler.h, MapDrawController.h, MapDragController.h, MapRasterPaintController.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapInteractionHandler.h"
#include "MapDrawController.h"
#include "MapDragController.h"
#include "MapRasterPaintController.h"
#include "MapSnapHelper.h"

#include <QGraphicsView>
#include <QMouseEvent>
#include <QScrollBar>

MapInteractionHandler::MapInteractionHandler(QObject *parent)
    : QObject(parent)
{
}

void MapInteractionHandler::setRasterContext(QImage *mapImage, QGraphicsPixmapItem **bgItem, int *brushSize)
{
    m_mapImage = mapImage;
    m_mapBgItem = bgItem;
    m_brushSize = brushSize;
}

bool MapInteractionHandler::handleViewportEvent(QObject *obj, QEvent *event)
{
    Q_UNUSED(obj);
    switch (event->type()) {
    case QEvent::MouseMove:
        return handleMouseMove(static_cast<QMouseEvent*>(event));
    case QEvent::MouseButtonPress:
        return handleMousePress(static_cast<QMouseEvent*>(event));
    case QEvent::MouseButtonRelease:
        return handleMouseRelease(static_cast<QMouseEvent*>(event));
    default:
        break;
    }
    return false;
}

bool MapInteractionHandler::handleMouseMove(QMouseEvent *me)
{
    if (!m_view) return false;
    QPointF sp = m_view->mapToScene(me->pos());
    emit cursorScenePosChanged(sp);

    // 手动平移 (中键 / Alt+左键)
    if (m_panning) {
        QPoint delta = me->pos() - m_lastPanPos;
        m_lastPanPos = me->pos();
        m_view->horizontalScrollBar()->setValue(m_view->horizontalScrollBar()->value() - delta.x());
        m_view->verticalScrollBar()->setValue(m_view->verticalScrollBar()->value() - delta.y());
        emit rulersUpdateRequested();
        return true;
    }

    if (m_drawCtrl && m_drawCtrl->isDrawingArea()) {
        return m_drawCtrl->handleMouseMove(sp);
    }

    if (m_isPainting && m_paintCtrl && m_mapImage && m_mapBgItem && m_brushSize) {
        bool isBrush = (m_drawCtrl && m_drawCtrl->drawMode() == MapEdit::Brush);
        m_paintCtrl->paintStroke(*m_mapImage, *m_mapBgItem, m_lastPaintImgPos, sp, *m_brushSize, isBrush, false);
        m_lastPaintImgPos = sp;
        return true;
    }

    if (m_dragCtrl && m_dragCtrl->isDragging()) {
        return m_dragCtrl->handleMouseMove(sp, m_selType, m_selId);
    }

    if (m_drawCtrl && m_drawCtrl->drawMode() != MapEdit::Drag) {
        emit snapIndicatorRequested(false, sp);
        m_drawCtrl->updatePreview(sp);
    } else if (m_dragCtrl) {
        emit snapIndicatorRequested(false, {});
        m_dragCtrl->updateHoverCursor(sp, m_selType, m_selId);
    }
    return false;
}

bool MapInteractionHandler::handleMousePress(QMouseEvent *me)
{
    if (!m_view) return false;

    // 右键处理: 完成曲线绘制 / 取消连续绘制 / 切换回 Drag 模式
    if (me->button() == Qt::RightButton) {
        if (m_drawCtrl) {
            if (m_drawCtrl->drawMode() == MapEdit::Curve && m_drawCtrl->curveCtrlPtsCount() >= 2) {
                m_drawCtrl->finalizeCurve();
                return true;
            }
            if (m_drawCtrl->drawMode() == MapEdit::Line && m_drawCtrl->hasP1()) {
                m_drawCtrl->cancelDraw();
                return true;
            }
            if (m_drawCtrl->drawMode() != MapEdit::Drag) {
                emit modeChangeRequested(MapEdit::Drag);
            } else {
                m_drawCtrl->cancelDraw();
            }
        }
        return true;
    }

    // Ctrl + 左键: 放行给底图旋转交互
    if (me->button() == Qt::LeftButton && (me->modifiers() & Qt::ControlModifier)) {
        return false;
    }

    // 中键 或 Alt+左键: 开始手动平移画布
    if (me->button() == Qt::MiddleButton ||
        (me->button() == Qt::LeftButton && (me->modifiers() & Qt::AltModifier))) {
        m_panning = true;
        m_lastPanPos = me->pos();
        m_view->viewport()->setCursor(Qt::ClosedHandCursor);
        return true;
    }

    if (me->button() != Qt::LeftButton) return false;
    QPointF sp = m_view->mapToScene(me->pos());

    int drawMode = m_drawCtrl ? m_drawCtrl->drawMode() : MapEdit::Drag;

    // 栅格涂黑/擦白模式
    if (drawMode == MapEdit::Brush || drawMode == MapEdit::Eraser) {
        if (m_paintCtrl && m_mapImage && m_mapBgItem && m_brushSize) {
            m_paintCtrl->pushUndo(*m_mapImage);
            m_isPainting = true;
            m_lastPaintImgPos = sp;
            bool isBrush = (drawMode == MapEdit::Brush);
            m_paintCtrl->paintStroke(*m_mapImage, *m_mapBgItem, sp, sp, *m_brushSize, isBrush, true);
        }
        return true;
    }

    // 拖动选择模式
    if (drawMode == MapEdit::Drag) {
        if (m_dragCtrl && m_dragCtrl->handleMousePress(sp, m_selType, m_selId)) {
            return true;
        }
        emit selectFeatureRequested(sp);
        return false;
    }

    // 拓扑绘制模式
    if (m_drawCtrl) {
        MapEdit::SnapResult snap;
        return m_drawCtrl->handleMousePress(sp, snap);
    }
    return false;
}

bool MapInteractionHandler::handleMouseRelease(QMouseEvent *me)
{
    if (!m_view) return false;

    // 手动平移释放
    if (m_panning && (me->button() == Qt::MiddleButton || me->button() == Qt::LeftButton)) {
        m_panning = false;
        m_view->viewport()->setCursor(Qt::OpenHandCursor);
        return true;
    }

    // 栅格笔刷释放
    if (me->button() == Qt::LeftButton && m_isPainting) {
        m_isPainting = false;
        return true;
    }

    QPointF sp = m_view->mapToScene(me->pos());

    // 区域绘制释放
    if (m_drawCtrl && m_drawCtrl->isDrawingArea() && me->button() == Qt::LeftButton) {
        return m_drawCtrl->handleMouseRelease(sp);
    }

    // 拖拽释放
    if (m_dragCtrl && m_dragCtrl->isDragging() && me->button() == Qt::LeftButton) {
        return m_dragCtrl->handleMouseRelease(sp, m_selType, m_selId);
    }

    return false;
}
