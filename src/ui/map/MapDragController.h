/**
 * @file    MapDragController.h
 * @layer   GUI
 * @brief   Controller managing node dragging, area resizing, and Bezier curve handle manipulation.
 * @deps    QObject, QGraphicsScene, QGraphicsView, MapEditTypes.h, MapEditDataModel.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QObject>
#include <QPointF>
#include <QString>
#include <functional>
#include "MapEditTypes.h"

class QGraphicsScene;
class QGraphicsView;
class QGraphicsItem;
class MapEditDataModel;
class MapCurveHandleManager;

class MapDragController : public QObject
{
    Q_OBJECT

public:
    using CoordToSceneFn = std::function<QPointF(double wx, double wy)>;
    using SceneToWorldFn = std::function<QPointF(double sx, double sy)>;

    explicit MapDragController(QObject *parent = nullptr);
    ~MapDragController() override = default;

    void setScene(QGraphicsScene *scene) { m_scene = scene; }
    void setView(QGraphicsView *view) { m_view = view; }
    void setModel(MapEditDataModel *model) { m_model = model; }
    void setHandleManager(MapCurveHandleManager *mgr) { m_handleMgr = mgr; }
    void setTransformFns(CoordToSceneFn w2s, SceneToWorldFn s2w) { m_w2s = w2s; m_s2w = s2w; }

    bool isDragging() const { return m_isDraggingNode || m_isDraggingHandle || m_isDraggingArea; }
    bool isDraggingNode() const { return m_isDraggingNode; }
    bool isDraggingArea() const { return m_isDraggingArea; }
    bool isDraggingHandle() const { return m_isDraggingHandle; }

    QString draggingNodeType() const { return m_draggingNodeType; }
    QString draggingNodeId() const { return m_draggingNodeId; }

    bool handleMousePress(const QPointF &sp, const QString &selType, const QString &selId);
    bool handleMouseMove(const QPointF &sp, const QString &selType, const QString &selId);
    bool handleMouseRelease(const QPointF &sp, const QString &selType, const QString &selId);
    void updateHoverCursor(const QPointF &sp, const QString &selType, const QString &selId);

    static MapEdit::AreaHitPart getAreaHitPart(const QPointF &sp, const QJsonObject &areaObj,
                                              CoordToSceneFn w2s, double cornerTol, double edgeTol);

signals:
    void dataChanged();
    void featureSelected(const QString &type, const QString &id);
    void nodeDragCommitted(const QString &type, const QString &id);
    void areaDragCommitted(const QString &id);
    void curveHandleDragCommitted(const QString &curveId);
    void areaBoundsChanged(double minX, double maxX, double minY, double maxY);

private:
    void dragMoveNode(const QPointF &sp);
    void dragMoveArea(const QPointF &sp, const QString &selId);
    void dragMoveCurveHandle(const QPointF &sp);

    QGraphicsScene         *m_scene = nullptr;
    QGraphicsView          *m_view = nullptr;
    MapEditDataModel       *m_model = nullptr;
    MapCurveHandleManager  *m_handleMgr = nullptr;
    CoordToSceneFn          m_w2s;
    SceneToWorldFn          m_s2w;

    // Node dragging
    bool                    m_isDraggingNode = false;
    QString                 m_draggingNodeType;
    QString                 m_draggingNodeId;
    QPointF                 m_dragStartScenePos;
    QPointF                 m_dragStartWorldPos;

    // Curve handle dragging
    bool                    m_isDraggingHandle = false;
    QGraphicsItem          *m_activeHandle = nullptr;
    QString                 m_handleCurveId;
    int                     m_handleIndex = -1;

    // Area dragging & resizing
    bool                    m_isDraggingArea = false;
    MapEdit::AreaHitPart    m_areaDragPart = MapEdit::AreaHit_None;
    QPointF                 m_areaDragStartScenePos;
    double                  m_areaDragStartMinX = 0;
    double                  m_areaDragStartMaxX = 0;
    double                  m_areaDragStartMinY = 0;
    double                  m_areaDragStartMaxY = 0;
};
