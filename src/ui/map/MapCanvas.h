/**
 * @file    MapCanvas.h
 * @layer   GUI
 * @brief   Underlying interactive QGraphicsView canvas handling panning, zooming, and coordinates.
 * @deps    QGraphicsView, QGraphicsScene, MapCanvasRobotTracker.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QJsonObject>
#include "MapCanvasRobotTracker.h"

class MapCanvas : public QGraphicsView
{
    Q_OBJECT
public:
    explicit MapCanvas(QGraphicsScene *scene, QWidget *parent = nullptr);
    ~MapCanvas() override = default;

    void setPanWithLeftButton(bool enabled);
    void resetPanState();

    void setWorldInfo(double left, double bottom, double right, double top, double width, double height);
    void setCarId(const QString &carId);

    // ── 地图缩放与视图控制常量 ──
    static constexpr double kMinScale      = 0.05;
    static constexpr double kMaxScale      = 20.0;
    static constexpr double kZoomFactorIn  = 1.15;
    static constexpr double kZoomFactorOut = 1.0 / 1.15;

signals:
    void transformChanged();
    void sceneClicked(const QPointF &scenePos, Qt::MouseButton btn);
    void sceneDoubleClicked(const QPointF &scenePos, Qt::MouseButton btn);
    void sceneMoved(const QPointF &scenePos);
    void sceneReleased(const QPointF &scenePos, Qt::MouseButton btn);
    void canvasResized(const QSize &sz);

protected:
    void wheelEvent(QWheelEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    void leaveEvent(QEvent *e) override;
    void focusOutEvent(QFocusEvent *e) override;
    bool viewportEvent(QEvent *event) override;
    bool event(QEvent *e) override;

private:
    void pinchTriggered(class QPinchGesture *gesture);
    void handleNativeGesture(class QNativeGestureEvent *e);

    MapCanvasRobotTracker *m_robotTracker = nullptr;

    bool   m_panning           = false;
    bool   m_panWithLeftButton = true;
    QPoint m_lastMousePos;

    // Center-based rotation state
    bool    m_rotating         = false;
    double  m_rotAngle         = 0.0;
    double  m_startViewAngle   = 0.0;
    QPointF m_rotStartMousePos;

    // Trackpad gestures state
    bool m_justPinched         = false;
    bool m_gestureZoomActive   = false;
    bool m_gestureRotateActive = false;
};
