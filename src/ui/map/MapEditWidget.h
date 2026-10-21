/**
 * @file    MapEditWidget.h
 * @layer   GUI
 * @brief   Topological map editor with station placement, lane routing, and property inspection.
 * @deps    MapCanvas, MapEditDataModel, MapSceneRenderer, MapPropsManager
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QWidget>
#include <QString>
#include <QImage>
#include <QPointF>
#include <functional>
#include "MapEditToolBarHelper.h"
#include "MapEditLayoutHelper.h"

class MapCanvas;
class RulerWidget;
class DirectionPadWidget;
class QLabel;
class QKeyEvent;
class QResizeEvent;
class QGraphicsScene;
class QGraphicsPixmapItem;

class MapEditDataModel;
class MapCurveHandleManager;
class MapSceneRenderer;
class MapDrawController;
class MapDragController;
class MapRasterPaintController;
class MapInteractionHandler;
class MapPropsManager;
class MapFeatureTablePanel;
class MapEditTelemetryController;

class MapEditWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MapEditWidget(const QString &mapId, QWidget *parent = nullptr);
    ~MapEditWidget() override;

    bool checkUnsavedChanges();

signals:
    void goBack();

public slots:
    void fitView();
    void setDrawMode(int mode);
    void updateRulers();
    void onSaveMap(std::function<void(bool success)> onFinished = nullptr);
    void onImportSmap();
    void onUndo();
    void onRedo();
    void onEditMap();
    void onTableToggle();
    void showHelpDialog();
    void deleteSelected();
    void clearSelection();
    void cancelDraw();
    void syncMotionSpinBoxRanges();
    void onTeleopActiveChanged(bool active);
    void onGridToggled(bool on);
    void onReflectorToggled(bool on);
    void onDataChanged();
    void applyPropsEdit();
    void deleteFeatureFromTable(const QString &type, const QString &id);
    void onAreaBoundsChanged();
    void showFeatureProps(const QString &type, const QString &id);
    void focusFeature(const QString &type, const QString &id);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupUI();
    void loadData();
    void redrawMap();

    QPointF worldToScene(double wx, double wy) const;
    QPointF sceneToWorld(double sx, double sy) const;

    QString m_mapId;
    QString m_mapName;
    bool    m_dirty = false;
    bool    m_showGrid = false;
    bool    m_showReflectors = false;
    bool    m_hasInitialFit = false;
    int     m_brushSize = 4;
    int     m_drawStatus = 0;

    double  m_worldL = 0.0, m_worldB = 0.0, m_worldR = 0.0, m_worldT = 0.0;
    int     m_imgW = 0, m_imgH = 0;
    QImage  m_mapImage;
    QImage  m_originalMapImage;
    QGraphicsPixmapItem *m_mapBgItem = nullptr;

    QGraphicsScene              *m_scene = nullptr;
    MapCanvas                   *m_mapView = nullptr;
    RulerWidget                 *m_topRuler = nullptr;
    RulerWidget                 *m_leftRuler = nullptr;
    QLabel                      *m_coordLabel = nullptr;
    DirectionPadWidget          *m_directionPad = nullptr;

    MapEditDataModel            *m_model = nullptr;
    MapCurveHandleManager       *m_handleMgr = nullptr;
    MapSceneRenderer            *m_renderer = nullptr;
    MapDrawController           *m_drawCtrl = nullptr;
    MapDragController           *m_dragCtrl = nullptr;
    MapRasterPaintController    *m_paintCtrl = nullptr;
    MapInteractionHandler       *m_interactionHandler = nullptr;
    MapPropsManager             *m_propsMgr = nullptr;
    MapFeatureTablePanel        *m_tablePanel = nullptr;
    MapEditTelemetryController  *m_telemetryCtrl = nullptr;

    MapEditToolBarHelper::TopBarWidgets     m_topWidgets;
    MapEditToolBarHelper::SubToolBarWidgets  m_subWidgets;
    MapEditToolBarHelper::LeftToolBarWidgets m_leftWidgets;
};
