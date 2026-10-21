/**
 * @file    MapDrawController.h
 * @layer   GUI
 * @brief   Controller managing state machine for drawing points, lines, curves, and areas.
 * @deps    QObject, QGraphicsScene, MapEditTypes.h, MapEditDataModel.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QObject>
#include <QPointF>
#include <QList>
#include <functional>
#include "MapEditTypes.h"

class QGraphicsScene;
class QGraphicsItem;
class QGraphicsLineItem;
class QGraphicsRectItem;
class MapEditDataModel;

class MapDrawController : public QObject
{
    Q_OBJECT

public:
    using CoordToSceneFn = std::function<QPointF(double wx, double wy)>;
    using SceneToWorldFn = std::function<QPointF(double sx, double sy)>;

    explicit MapDrawController(QObject *parent = nullptr);
    ~MapDrawController() override = default;

    void setScene(QGraphicsScene *scene) { m_scene = scene; }
    void setModel(MapEditDataModel *model) { m_model = model; }
    void setTransformFns(CoordToSceneFn w2s, SceneToWorldFn s2w) { m_w2s = w2s; m_s2w = s2w; }

    void setDrawMode(int mode);
    int drawMode() const { return m_drawStatus; }
    bool isDrawingArea() const { return m_isDrawingArea; }
    bool hasP1() const { return m_drawHasP1; }
    int curveCtrlPtsCount() const { return m_curveCtrlPts.size(); }

    void cancelDraw();
    void updatePreview(const QPointF &sp);
    void clearPreview();
    void finalizeCurve();

    bool handleMousePress(const QPointF &sp, const MapEdit::SnapResult &snap);
    bool handleMouseMove(const QPointF &sp);
    bool handleMouseRelease(const QPointF &sp);

signals:
    void dataChanged();
    void warningToast(const QString &msg);
    void successToast(const QString &msg);
    void selectFeatureRequested(const QPointF &sp);
    void showPropertiesRequested(const QString &type, const QString &id);
    void modeChangeRequested(int newMode);

private:
    void addPoint(const QPointF &sp, double yaw = 0.0, double theta = 0.0);
    void addCharge(const QPointF &sp, double epcX, double epcY, double epcTheta);
    void addHighPrecisionPoint(const QPointF &sp, double epcX, double epcY, double epcTheta);
    void addLine(const QPointF &a, const QPointF &b, const QString &startId, const QString &endId);
    void addBezierCurve(const QList<QPointF> &scenePts, const QList<QString> &snapIds);

    QGraphicsScene       *m_scene = nullptr;
    MapEditDataModel     *m_model = nullptr;
    CoordToSceneFn        m_w2s;
    SceneToWorldFn        m_s2w;

    int                   m_drawStatus = MapEdit::Drag;
    bool                  m_drawHasP1 = false;
    QPointF               m_drawP1;
    QString               m_drawP1Id;
    QList<QPointF>        m_curveCtrlPts;
    QList<QString>        m_curveCtrlIds;

    bool                  m_isDrawingArea = false;
    QPointF               m_areaStartScenePos;
    QGraphicsRectItem    *m_areaPreviewItem = nullptr;
    QGraphicsItem        *m_previewItem = nullptr;
    QList<QGraphicsLineItem*> m_previewAids;
};
