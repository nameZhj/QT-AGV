/**
 * @file    MapInteractionHandler.h
 * @layer   GUI
 * @brief   Viewport event filter dispatcher routing pan, paint, draw, drag, and selection events.
 * @deps    QObject, QGraphicsView, MapDrawController.h, MapDragController.h, MapRasterPaintController.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QObject>
#include <QPointF>
#include <QPoint>

class QGraphicsView;
class QGraphicsScene;
class QGraphicsPixmapItem;
class QMouseEvent;
class MapDrawController;
class MapDragController;
class MapRasterPaintController;

class MapInteractionHandler : public QObject
{
    Q_OBJECT

public:
    explicit MapInteractionHandler(QObject *parent = nullptr);
    ~MapInteractionHandler() override = default;

    void setView(QGraphicsView *view) { m_view = view; }
    void setControllers(MapDrawController *drawCtrl,
                        MapDragController *dragCtrl,
                        MapRasterPaintController *paintCtrl) {
        m_drawCtrl = drawCtrl;
        m_dragCtrl = dragCtrl;
        m_paintCtrl = paintCtrl;
    }
    void setRasterContext(QImage *mapImage, QGraphicsPixmapItem **bgItem, int *brushSize);
    void setSelectedFeature(const QString &type, const QString &id) {
        m_selType = type;
        m_selId = id;
    }

    bool handleViewportEvent(QObject *obj, QEvent *event);

signals:
    void cursorScenePosChanged(const QPointF &sp);
    void rulersUpdateRequested();
    void selectFeatureRequested(const QPointF &sp);
    void modeChangeRequested(int mode);
    void snapIndicatorRequested(bool valid, const QPointF &pos);

private:
    bool handleMouseMove(QMouseEvent *me);
    bool handleMousePress(QMouseEvent *me);
    bool handleMouseRelease(QMouseEvent *me);

    QGraphicsView            *m_view = nullptr;
    MapDrawController        *m_drawCtrl = nullptr;
    MapDragController        *m_dragCtrl = nullptr;
    MapRasterPaintController *m_paintCtrl = nullptr;

    QImage                   *m_mapImage = nullptr;
    QGraphicsPixmapItem     **m_mapBgItem = nullptr;
    int                      *m_brushSize = nullptr;

    QString                   m_selType;
    QString                   m_selId;

    // Viewport panning
    bool                      m_panning = false;
    QPoint                    m_lastPanPos;

    // Raster painting
    bool                      m_isPainting = false;
    QPointF                   m_lastPaintImgPos;
};
