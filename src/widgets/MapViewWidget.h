#include "MapCanvas.h"
#ifndef MAPVIEWWIDGET_H
#define MAPVIEWWIDGET_H

#include <QWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QScrollArea>
#include <QPixmap>
#include <cmath>

class RobotStatusDialog;
struct RobotInfo;

// ── Ruler widget drawn as an overlay alongside the map view ──────────────────
class RulerWidget : public QWidget
{
    Q_OBJECT
public:
    enum Orientation { Horizontal, Vertical };
    explicit RulerWidget(Orientation ori, QWidget *parent = nullptr);

    // worldL/B/R/T = world extent (metres); imgW/H = thumbnail pixel size
    void syncWithView(QGraphicsView *view,
                      double worldL, double worldB, double worldR, double worldT,
                      double imgW,   double imgH);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    Orientation    m_ori;
    QGraphicsView *m_view  = nullptr;
    double m_worldL = 0, m_worldB = 0, m_worldR = 0, m_worldT = 0;
    double m_imgW   = 0, m_imgH   = 0;
};

// ── Map canvas (QGraphicsView subclass) ──────────────────────────────────────


// ── Main comprehensive map view widget ───────────────────────────────────────
class MapViewWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MapViewWidget(QWidget *parent = nullptr);
    ~MapViewWidget() override;

    // 刷新当前地图数据(由 MainWindow 在切换菜单时调用)
    void refreshData();

signals:
    void navigateRequested(const QString &target, const QString &id);

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
    void onEditRoute();
    void onEditMap();
    void onRenameMap();
    void onDownloadSource();
    void onDeleteMap();
    void onMqttMessage(const QString &topic, const QByteArray &payload);
    void onViewTransformChanged();
    void onSceneClicked(const QPointF &scenePos, Qt::MouseButton btn);
    void onSceneMoved(const QPointF &scenePos);
    void onSceneReleased(const QPointF &scenePos, Qt::MouseButton btn);
    void onResetView();
    void onShowRobotStatus();
    void onToggleNavMode();

private:
    void setupUI();
    void setupToolbar(QWidget *parent);
    void setupRightPanel(QWidget *parent);

    void loadCurrentCarMap();
    void loadMapById(const QString &mapId, const QString &mapName = QString());
    bool findLocalMapFiles(const QString &dirPath, QString &outYamlPath, QString &outPgmPath,
                           const QString &preferredMapName = QString(),
                           const QString &preferredYamlName = QString(),
                           const QString &preferredPgmName = QString());
    bool loadMapFromLocalFiles(const QString &yamlPath, const QString &pgmPath);
    void applyMapData(const QJsonObject &mapInfo);
    void loadMapImage();
    void loadMapFromPgm(double apiOriginX, double apiOriginY, double apiPhysW, double apiPhysH);
    void installMapPixmap(const QPixmap &pix);
    void loadRoute();

    void initMqtt();
    void disconnectMqtt();

    void renderAll();
    void renderAxesAndGrid();
    void renderRoute();
    void renderRobot();
    void updateRulers();
    void updateInfoPanel();
    void onRobotInfoUpdated(const RobotInfo &info);
    void setStatus(const QString &msg, const QString &color = "#8898a8");

    // Coordinate transform (world ↔ scene/pixel)
    QPointF worldToScene(double wx, double wy) const;
    QPointF sceneToWorld(double sx, double sy) const;

    // Map metadata
    QString m_mapId, m_mapName, m_tilemapPath;
    // World extent (metres): bottom-left = (m_worldL, m_worldB), top-right = (m_worldR, m_worldT)
    double m_worldL = 0, m_worldB = 0, m_worldR = 0, m_worldT = 0;
    // Thumbnail pixel size (set after image loads)
    double m_imgW   = 0, m_imgH   = 0;
    bool   m_mapReady = false;

    // Route data
    QJsonArray m_points, m_lines, m_curves, m_charges, m_highPrecisionPoints;
    bool m_routeLoaded = false;

    // Robot / car state
    struct RobotState {
        QString carId = "1", carName;
        double wx = 0, wy = 0, yawRad = 0;
        double speed = 0, battery = -1, covScore = -1;
        bool posValid = false;
    } m_robot;

    // Scene
    QGraphicsScene *m_scene       = nullptr;
    MapCanvas      *m_canvas      = nullptr;
    RulerWidget    *m_topRuler    = nullptr;
    RulerWidget    *m_leftRuler   = nullptr;

    // Scene items (owned by scene)
    QGraphicsItem *m_mapBg     = nullptr;
    QGraphicsItem *m_axesLayer = nullptr;
    QGraphicsItem *m_routeLayer= nullptr;
    QGraphicsItem *m_robotItem = nullptr;

    // Grid visibility
    bool m_showGrid = false;

    // Toolbar buttons
    QToolButton *m_createMapBtn = nullptr;
    QToolButton *m_uploadMapBtn = nullptr;
    QToolButton *m_uploadSmapBtn = nullptr;
    QToolButton *m_refreshBtn    = nullptr;
    QToolButton *m_switchMapBtn = nullptr;
    QToolButton *m_relocBtn     = nullptr;
    QToolButton *m_navModeBtn   = nullptr;
    QToolButton *m_statusBtn    = nullptr;
    QToolButton *m_resetViewBtn = nullptr;
    QPushButton *m_gridBtn      = nullptr;
    RobotStatusDialog *m_statusDialog = nullptr;
    QByteArray  m_lastObstaclePayload;
    QString     m_lastObstacleTopic;

    // Navigation mode
    bool        m_navMode          = false;

    // Relocalization mode (click+drag to set position and heading)
    bool        m_relocMode        = false;
    bool        m_relocDragging    = false;
    QPointF     m_relocStartWorld;       // world coords of press point
    QGraphicsItem *m_poseArrowItem = nullptr;  // preview arrow line
    QGraphicsItem *m_poseDotItem   = nullptr;  // preview dot at start
    QLabel      *m_mapNameLabel = nullptr;
    QLabel      *m_statusLabel  = nullptr;

    // Robot SVG pixmap (loaded once at startup)
    QPixmap m_robotPixmap;

    // Right info panel labels
    QLabel *m_carNameLabel      = nullptr;
    QLabel *m_deviceStatusLabel = nullptr;
    QLabel *m_batteryLabel      = nullptr;
    QLabel *m_speedLabel        = nullptr;
    QLabel *m_covLabel      = nullptr;
    QLabel *m_posLabel      = nullptr;
    QLabel *m_coordLabel    = nullptr;  // cursor world coordinate overlay

    // Map info (moved here from the status bar above the canvas)
    QLabel *m_infoNameLabel   = nullptr;
    QLabel *m_infoSizeLabel   = nullptr;
    QLabel *m_infoResLabel    = nullptr;
    QLabel *m_infoOriginLabel = nullptr;

    // 请求去重与防抖节流
    bool    m_isLoadingCar  = false;
    QString m_loadingMapId;
    qint64  m_lastRefreshMs = 0;
};

#endif // MAPVIEWWIDGET_H
