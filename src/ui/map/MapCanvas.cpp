/**
 * @file    MapCanvas.cpp
 * @layer   GUI
 * @brief   Implementation of interactive QGraphicsView canvas.
 * @deps    MapCanvas.h, QGraphicsScene, QWheelEvent, MapCanvasRobotTracker.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapCanvas.h"
#include "ui/common/ThemeColors.h"
#include <QWheelEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QNativeGestureEvent>
#include <QGestureEvent>
#include <QPinchGesture>
#include <QTimer>
#include <QApplication>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

MapCanvas::MapCanvas(QGraphicsScene *scene, QWidget *parent)
    : QGraphicsView(scene, parent)
{
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    setDragMode(QGraphicsView::NoDrag);
    setBackgroundBrush(ThemeColors::Canvas::Background());
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setCursor(Qt::CrossCursor);

    m_robotTracker = new MapCanvasRobotTracker(scene, this);

    setAttribute(Qt::WA_AcceptTouchEvents);
    viewport()->setAttribute(Qt::WA_AcceptTouchEvents);
    viewport()->grabGesture(Qt::PinchGesture);
}

void MapCanvas::setPanWithLeftButton(bool enabled) { m_panWithLeftButton = enabled; }

void MapCanvas::resetPanState()
{
    m_panning = false;
    m_rotating = false;
    setCursor(Qt::CrossCursor);
}

void MapCanvas::setWorldInfo(double left, double bottom, double right, double top, double width, double height)
{
    if (m_robotTracker) m_robotTracker->setWorldInfo(left, bottom, right, top, width, height);
}

void MapCanvas::setCarId(const QString &carId)
{
    if (m_robotTracker) m_robotTracker->setCarId(carId);
}

void MapCanvas::wheelEvent(QWheelEvent *e)
{
    bool hasCtrl = (e->modifiers() & Qt::ControlModifier);
    bool hasPixelDelta = !e->pixelDelta().isNull();
    int angleY = e->angleDelta().y();
    int angleX = e->angleDelta().x();
    bool isContinuousScroll = hasPixelDelta || (angleX != 0 && (angleX % 120) != 0) || (angleY != 0 && (angleY % 120) != 0);

    if (hasCtrl) {
        if (isContinuousScroll) {
            double dx = hasPixelDelta ? e->pixelDelta().x() : (angleX * 0.3);
            double dy = hasPixelDelta ? e->pixelDelta().y() : (angleY * 0.3);
            double delta = (qAbs(dx) > qAbs(dy)) ? dx : dy;
            if (qAbs(delta) > 0.05) {
                double deg = delta * 0.04;
                m_rotAngle += deg;
                setTransformationAnchor(QGraphicsView::AnchorViewCenter);
                rotate(deg);
                setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
                emit transformChanged();
            }
            e->accept();
        } else {
            int delta = (qAbs(angleY) >= qAbs(angleX)) ? angleY : angleX;
            if (delta != 0) {
                double factor = delta > 0 ? 1.12 : (1.0 / 1.12);
                setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
                scale(factor, factor);
                emit transformChanged();
            }
            e->accept();
        }
    } else {
        if (isContinuousScroll) {
            double dx = (hasPixelDelta ? e->pixelDelta().x() : (angleX * 0.3)) * 0.5;
            double dy = (hasPixelDelta ? e->pixelDelta().y() : (angleY * 0.3)) * 0.5;
            if (qAbs(dx) > 0.05 || qAbs(dy) > 0.05) {
                horizontalScrollBar()->setValue(horizontalScrollBar()->value() - dx);
                verticalScrollBar()->setValue(verticalScrollBar()->value() - dy);
                emit transformChanged();
            }
            e->accept();
        } else {
            if (angleY != 0) {
                double factor = angleY > 0 ? 1.15 : (1.0 / 1.15);
                setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
                scale(factor, factor);
                emit transformChanged();
            }
            e->accept();
        }
    }
}

bool MapCanvas::viewportEvent(QEvent *event)
{
    if (event->type() == QEvent::Gesture) {
        auto *ge = static_cast<QGestureEvent*>(event);
        if (QGesture *pinch = ge->gesture(Qt::PinchGesture)) {
            pinchTriggered(static_cast<QPinchGesture*>(pinch));
            return true;
        }
    }
    return QGraphicsView::viewportEvent(event);
}

void MapCanvas::pinchTriggered(QPinchGesture *gesture)
{
    if (gesture->changeFlags() & QPinchGesture::ScaleFactorChanged) {
        double factor = gesture->scaleFactor();
        double sensitiveFactor = qBound(0.95, 1.0 + (factor - 1.0) * 0.35, 1.05);
        scale(sensitiveFactor, sensitiveFactor);
        emit transformChanged();

        m_justPinched = true;
        QTimer::singleShot(200, this, [this]() { m_justPinched = false; });
    }
}

bool MapCanvas::event(QEvent *e)
{
    if (e->type() == QEvent::NativeGesture) {
        handleNativeGesture(static_cast<QNativeGestureEvent*>(e));
        return true;
    }
    return QGraphicsView::event(e);
}

void MapCanvas::handleNativeGesture(QNativeGestureEvent *e)
{
    switch (e->gestureType()) {
    case Qt::BeginNativeGesture:
    case Qt::EndNativeGesture:
        m_gestureZoomActive = false;
        m_gestureRotateActive = false;
        break;
    case Qt::ZoomNativeGesture: {
        double value = e->value();
        if (qAbs(value) < 1e-6) break;
        double factor = qBound(0.95, 1.0 + value * 1.5, 1.05);
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        scale(factor, factor);
        emit transformChanged();
        m_gestureZoomActive = true;
        m_justPinched = true;
        QTimer::singleShot(200, this, [this]() { m_justPinched = false; });
        break;
    }
    default:
        break;
    }
}

void MapCanvas::mousePressEvent(QMouseEvent *e)
{
    QPointF sp = mapToScene(e->pos());
    emit sceneClicked(sp, e->button());

    if (!(QApplication::mouseButtons() & e->button())) {
        resetPanState();
        return;
    }

    if (e->button() == Qt::LeftButton && (e->modifiers() & Qt::ControlModifier)) {
        m_rotating = true;
        m_rotStartMousePos = e->pos();
        m_startViewAngle = m_rotAngle;
        setCursor(Qt::SizeAllCursor);
        e->accept();
        return;
    }

    if (e->button() == Qt::MiddleButton ||
        (e->button() == Qt::LeftButton && (m_panWithLeftButton || (e->modifiers() & Qt::AltModifier)))) {
        m_panning = true;
        m_lastMousePos = e->pos();
        setCursor(Qt::ClosedHandCursor);
        e->accept();
        return;
    }

    QGraphicsView::mousePressEvent(e);
}

void MapCanvas::mouseDoubleClickEvent(QMouseEvent *e)
{
    emit sceneDoubleClicked(mapToScene(e->pos()), e->button());
    e->accept();
}

void MapCanvas::mouseMoveEvent(QMouseEvent *e)
{
    if (m_rotating && !(e->buttons() & Qt::LeftButton)) resetPanState();
    if (m_panning && !(e->buttons() & (Qt::LeftButton | Qt::MiddleButton))) resetPanState();

    if (m_rotating) {
        QPoint center = viewport()->rect().center();
        QPointF v0 = m_rotStartMousePos - center;
        QPointF v1 = e->pos() - center;
        double a0 = std::atan2(v0.y(), v0.x());
        double a1 = std::atan2(v1.y(), v1.x());
        double delta = a1 - a0;
        while (delta > M_PI) delta -= 2 * M_PI;
        while (delta < -M_PI) delta += 2 * M_PI;
        double deltaDeg = delta * 180.0 / M_PI;
        double newAngle = m_startViewAngle + deltaDeg;
        double viewDelta = newAngle - m_rotAngle;
        m_rotAngle = newAngle;

        setTransformationAnchor(QGraphicsView::AnchorViewCenter);
        rotate(viewDelta);
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        emit transformChanged();
        e->accept();
        return;
    }

    if (!m_panning) {
        emit sceneMoved(mapToScene(e->pos()));
    } else {
        QPoint delta = e->pos() - m_lastMousePos;
        m_lastMousePos = e->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        emit transformChanged();
        e->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(e);
    emit transformChanged();
}

void MapCanvas::mouseReleaseEvent(QMouseEvent *e)
{
    if (m_rotating && e->button() == Qt::LeftButton) {
        m_rotating = false;
        setCursor(Qt::CrossCursor);
        e->accept();
        return;
    }

    if (m_panning && (e->button() == Qt::MiddleButton || e->button() == Qt::LeftButton)) {
        m_panning = false;
        setCursor(Qt::CrossCursor);
        e->accept();
        return;
    }

    QPointF sp = mapToScene(e->pos());
    emit sceneReleased(sp, e->button());
    QGraphicsView::mouseReleaseEvent(e);
}

void MapCanvas::resizeEvent(QResizeEvent *e)
{
    QGraphicsView::resizeEvent(e);
    emit canvasResized(e->size());
}

void MapCanvas::leaveEvent(QEvent *e)
{
    if ((m_panning || m_rotating) && !(QApplication::mouseButtons() & (Qt::LeftButton | Qt::MiddleButton))) resetPanState();
    QGraphicsView::leaveEvent(e);
}

void MapCanvas::focusOutEvent(QFocusEvent *e)
{
    resetPanState();
    QGraphicsView::focusOutEvent(e);
}
