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
#include <QGroupBox>
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

    // 建图成功后强制刷新：清理本地缓存 → getCar获取绑定mapId → 重新下载zip → 解析展示
    void forceRefreshAfterMapping();

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
    void onShowMoreMenu();
    void onShowAudioManager();
    void onToggleNavMode();
    void onToggleMoveMode();
    void onCancelNavigation();
    void onPauseNavigation();
    void onTeleopClicked();
    void onTeleopActiveChanged(bool active);

private:
    void setupUI();
    void setupToolbar(QWidget *parent);
    void setupRightPanel(QWidget *parent);
    void exitRelocMode();
    void updateNavButtonsState();

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

    void clearScene();
    void clearRouteItems();
    void clearGridItems();

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
    double m_imgW = 0, m_imgH = 0;
    bool   m_mapReady = false;
    bool   m_routeLoaded = false;

    // JSON route objects (parsed from route.json)
    QJsonArray m_points;
    QJsonArray m_lines;
    QJsonArray m_curves;
    QJsonArray m_charges;
    QJsonArray m_highPrecisionPoints;

    // Robot state
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

    // Track top-level graphics items to prevent double-free crashes during cleanup
    QList<QGraphicsItem *> m_routeTopLevelItems;
    QList<QGraphicsItem *> m_gridItems;

    // Grid visibility
    bool m_showGrid = false;

    // Toolbar buttons (13 buttons from left to right)
    QToolButton *m_editMapBtn     = nullptr; // 1. 启用编辑
    QToolButton *m_uploadSmapBtn  = nullptr; // 2. 加载地图 (原 smap上传)
    QToolButton *m_createMapBtn   = nullptr; // 3. 2D建图
    QToolButton *m_relocBtn       = nullptr; // 4. 重定位
    QToolButton *m_navModeBtn     = nullptr; // 5. 路径导航
    QToolButton *m_moveBtn        = nullptr; // 6. 移动
    QToolButton *m_cancelNavBtn   = nullptr; // 7. 取消导航
    QToolButton *m_pauseNavBtn    = nullptr; // 8. 暂停导航
    QToolButton *m_uploadMapBtn   = nullptr; // 9. 推送地图 (原 地图上传)
    QToolButton *m_downloadMapBtn = nullptr; // 10. 拉取地图 (原 下载原文件)
    QToolButton *m_switchMapBtn   = nullptr; // 11. 地图管理 (原 切换地图)
    QToolButton *m_teleopBtn      = nullptr; // 12. 抢占控制权
    QToolButton *m_statusBtn      = nullptr; // 13. 状态
    QToolButton *m_moreBtn        = nullptr; // 14. 其它 (弹出工具列表)
    class QMenu *m_moreMenu       = nullptr;
    QToolButton *m_resetViewBtn   = nullptr;
    QToolButton *m_refreshBtn     = nullptr;
    QPushButton *m_gridBtn        = nullptr;
    class DirectionPadWidget *m_directionPad = nullptr;
    RobotStatusDialog *m_statusDialog = nullptr;
    class MotionDialog *m_motionDialog = nullptr;

    // Navigation & Move modes
    bool        m_navMode          = false;
    bool        m_moveMode         = false;
    bool        m_navActive        = false;

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

    // Station info (对齐"启用编辑"站点属性面板规范)
    void showStationDetails(const QJsonObject &pt, const QString &type);
    void clearStationDetails();

    QGroupBox            *m_stationGroup         = nullptr;
    QLabel               *m_stationBadge         = nullptr;
    QLabel               *m_stationEmptyHint     = nullptr;
    QWidget              *m_stationContentWidget = nullptr;
    QLabel               *m_stationTypeVal       = nullptr;
    QLabel               *m_stationIdVal         = nullptr;
    QLabel               *m_stationNameVal       = nullptr;
    QLabel               *m_stationPosVal        = nullptr;
    QLabel               *m_stationYawVal        = nullptr;
    QLabel               *m_stationDockVal       = nullptr;
    QLabel               *m_stationRotateVal     = nullptr;
    QLabel               *m_stationPgvVal        = nullptr;
    QWidget              *m_stationEpcGroup      = nullptr;
    QLabel               *m_stationEpcPosVal     = nullptr;
    QLabel               *m_stationEpcYawVal     = nullptr;
    // Route info (对齐"启用编辑"线路属性面板)
    void showRouteDetails(const QJsonObject &obj, const QString &type);
    void clearRouteDetails();

    // 命中容差: 把屏幕像素换算为场景单位 (与"启用编辑"一致)
    double hitTolScene(double screenPx) const;

    QGroupBox            *m_routeGroup         = nullptr;
    QLabel               *m_routeBadge         = nullptr;
    QLabel               *m_routeEmptyHint     = nullptr;
    QWidget              *m_routeContentWidget = nullptr;
    QLabel               *m_routeTypeVal       = nullptr;
    QLabel               *m_routeIdVal         = nullptr;
    QLabel               *m_routeNameVal       = nullptr;
    QLabel               *m_routeStartVal      = nullptr;
    QLabel               *m_routeEndVal        = nullptr;
    QLabel               *m_routeDirectVal     = nullptr;
    QLabel               *m_routeMaxSpeedVal   = nullptr;
    QLabel               *m_routeMaxAccVal     = nullptr;
    QLabel               *m_routeMaxDecVal     = nullptr;
    QLabel               *m_routeMaxRotVal     = nullptr;
    QLabel               *m_routeReachDistVal  = nullptr;
    QLabel               *m_routeReachAngleVal = nullptr;
    QLabel               *m_routeWidthVal      = nullptr;

    QGraphicsItem        *m_selItem            = nullptr;
    QString               m_selectedStationId;

    // 请求去重与防抖节流
    bool    m_isLoadingCar  = false;
    QString m_loadingMapId;
    qint64  m_lastRefreshMs = 0;
};

#endif // MAPVIEWWIDGET_H
