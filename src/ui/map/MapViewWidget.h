/**
 * @file    MapViewWidget.h
 * @layer   GUI
 * @brief   Real-time robot map monitor displaying laser points, robot pose, and planned paths.
 * @deps    MapViewToolbarHelper.h, MapViewRightPanel.h, MapViewLayoutHelper.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QWidget>
#include <QJsonObject>
#include <QJsonArray>
#include <QList>
#include <functional>
#include "MapViewToolbarHelper.h"
#include "MapViewRightPanel.h"
#include "MapViewLayoutHelper.h"
#include "MapViewTelemetryController.h"
#include "RulerWidget.h"

class QGraphicsScene;
class QGraphicsItem;
class QLabel;
class QPushButton;
class DirectionPadWidget;
class RobotStatusDialog;
class EditConfigWidget;
class QDialog;
class MapViewDataLoader;
class MapViewSceneRenderer;
class MapViewRobotMarker;
class MapViewRelocController;
class MapViewNavInteraction;
class MapViewMarkHelper;
class MapViewCanvasHandler;

class MapViewWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MapViewWidget(QWidget *parent = nullptr);
    ~MapViewWidget() override;

    void refreshData();
    void forceRefreshAfterMapping();

    QString mapStatusText() const;
    QString mapStatusColor() const;

signals:
    void navigateRequested(const QString &target, const QString &id);
    void mapStatusChanged();

public slots:
    void onShowConfigDialog();

protected:
    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *) override;

private slots:
    void onCreateMap();
    void onUploadMap();
    void onUploadSmap();
    void onRefreshMap();
    void onSwitchMap();
    void onRelocalize();
    void onToggleNavMode();
    void onToggleMoveMode();
    void onCancelNavigation();
    void onPauseNavigation();
    void onTeleopClicked();
    void onTeleopActiveChanged(bool active);
    void onShowRobotStatus();
    void onShowMoreMenu();
    void onShowAudioManager();
    void onResetView();
    void onEditMap();
    void onDownloadSource();
    void onViewTransformChanged();
    void onSceneClicked(const QPointF &scenePos, Qt::MouseButton btn);
    void onSceneDoubleClicked(const QPointF &scenePos, Qt::MouseButton btn);
    void onSceneMoved(const QPointF &scenePos);
    void onSceneReleased(const QPointF &scenePos, Qt::MouseButton btn);

private:
    void setupUI();
    void updateRulers();
    void renderAll();
    void clearRouteData();
    void selectFeatureOnMap(const QString &type, const QString &id, bool center = false);
    void clearMapSelection();
    void applyLayerVisibility();
    void setStatus(const QString &msg, const QString &color = "#8898a8");
    void updateNavButtons();
    QString stationNameById(const QString &id) const;
    QString routeFeatureName(const QJsonObject &obj) const;

    // Sub-controllers
    MapViewDataLoader          *m_dataLoader     = nullptr;
    MapViewSceneRenderer      *m_sceneRenderer  = nullptr;
    MapViewRobotMarker        *m_robotMarker    = nullptr;
    MapViewTelemetryController *m_telemetryCtrl = nullptr;
    MapViewRelocController     *m_relocCtrl      = nullptr;
    MapViewNavInteraction      *m_navInteraction = nullptr;
    MapViewMarkHelper          *m_markHelper     = nullptr;
    MapViewCanvasHandler       *m_canvasHandler  = nullptr;

    // UI Widgets
    MapViewToolbarButtons       m_toolBtns;
    MapViewCanvasArea           m_canvasArea;
    QLabel                     *m_statusLabel     = nullptr;
    QGraphicsScene             *m_scene           = nullptr;
    MapViewRightPanel          *m_rightPanel      = nullptr;
    RobotStatusDialog          *m_statusDialog    = nullptr;
    QDialog                    *m_configDialog    = nullptr;
    EditConfigWidget           *m_editConfigWidget= nullptr;

    // Scene & Map State
    QGraphicsItem              *m_mapBg           = nullptr;
    QGraphicsItem              *m_selItem         = nullptr;
    QGraphicsItem              *m_layerHiddenItem = nullptr;
    QList<QGraphicsItem*>       m_routeTopLevelItems;
    QList<QGraphicsItem*>       m_gridItems;

    QString                     m_mapId;
    QString                     m_mapName;
    double                      m_worldL = 0, m_worldB = 0, m_worldR = 0, m_worldT = 0;
    double                      m_imgW = 0, m_imgH = 0;
    bool                        m_mapReady = false;
    bool                        m_mapLoadFailed = false;
    bool                        m_showGrid = false;

    QJsonArray                  m_points;
    QJsonArray                  m_lines;
    QJsonArray                  m_curves;
    QJsonArray                  m_charges;
    QJsonArray                  m_highPrecisionPoints;
    QJsonArray                  m_highLevelAreas;
};
