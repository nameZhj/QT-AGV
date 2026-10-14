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
#include <QLineEdit>
#include <QListWidget>
#include <QCheckBox>
#include <QVBoxLayout>
#include <cmath>

class QStackedWidget;

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

public slots:
    void onShowConfigDialog();

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
    void onSceneDoubleClicked(const QPointF &scenePos, Qt::MouseButton btn);
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

    // ══ 右侧面板: 属性 / 图元 / 图层 三个标签页 ══════════════════════════════
    enum RightTab  { TabProps = 0, TabElements = 1, TabLayers = 2 };
    // 属性页的五种内容: 设备 / 站点 / 线路 / 地图 / 高级区域
    enum PropsView { ViewDevice = 0, ViewStation = 1, ViewRoute = 2, ViewMap = 3, ViewArea = 4 };

    void setRightTab(int tab);            // 切换 属性/图元/图层
    void switchPropsView(int view);       // 切换属性页显示的内容
    void updateDeviceMonitor(const RobotInfo &info);   // 设备数据监视 (MQTT /rob/{id}/info)
    void updateDeviceStatusDisplay(const RobotInfo &info); // 统一设备与导航运行状态显示
    void updateMapPropsView();                         // 地图属性 (点击空白)

    void refreshElementList();                          // 图元列表载入
    void onElementItemClicked(QListWidgetItem *item);   // 图元列表点击 → 地图选中+居中
    void syncElementListSelection(const QString &type, const QString &id);
    void selectFeatureOnMap(const QString &type, const QString &id, bool center = false);
    void clearMapSelection();

    void applyLayerVisibility();       // 图层: 隐藏选中/隐藏所有站点/隐藏所有线路
    void onShowAllElements();          // 图层: 显示所有图元

    QStackedWidget *m_rightStack    = nullptr;
    QPushButton    *m_tabPropsBtn    = nullptr;
    QPushButton    *m_tabElementsBtn = nullptr;
    QPushButton    *m_tabLayersBtn   = nullptr;
    int             m_rightTab  = TabProps;
    int             m_propsView = ViewDevice;

    // 设备相关分组 (属性页-设备视图); m_deviceHasData 记录该分组当前是否有 MQTT 数据
    QList<QWidget*> m_deviceGroups;
    QSet<QWidget*>  m_deviceHasData;
    void addDeviceGroup(QWidget *g) { if (g) m_deviceGroups.append(g); }
    // 地图相关分组 (属性页-地图视图)
    QList<QWidget*> m_mapGroups;
    void addMapGroup(QWidget *g) { if (g) m_mapGroups.append(g); }

    // 设备数据监视 (MQTT /rob/{id}/info)
    QWidget *m_ctlGroup        = nullptr;   // 控制屏
    QLabel  *m_ctlBatteryVal   = nullptr;
    QLabel  *m_ctlActualVal    = nullptr;
    QLabel  *m_ctlCmdVal       = nullptr;
    QWidget *m_mileageGroup    = nullptr;   // 里程
    QLabel  *m_mileageTotalVal = nullptr;
    QLabel  *m_mileageTodayVal = nullptr;
    QWidget *m_runtimeGroup    = nullptr;   // 运行时间
    QLabel  *m_runtimeCurVal   = nullptr;
    QLabel  *m_runtimeTotalVal = nullptr;
    QWidget *m_obstacleGroup   = nullptr;   // 障碍
    QLabel  *m_obstacleTypeVal = nullptr;
    QLabel  *m_obstaclePointVal= nullptr;
    QWidget *m_blockedGroup    = nullptr;   // 阻挡
    QLabel  *m_blockedStateVal = nullptr;
    QWidget *m_poseGroup       = nullptr;   // 车体位姿与机台
    QLabel  *m_posePosVal      = nullptr;
    QLabel  *m_poseYawVal      = nullptr;
    QLabel  *m_poseThetaVal    = nullptr;
    QLabel  *m_poseEpcVal      = nullptr;
    QWidget *m_robotInfoGroup  = nullptr;   // 机器人信息
    QLabel  *m_robotCarIdVal   = nullptr;
    QLabel  *m_robotMapIdVal   = nullptr;
    QLabel  *m_robotTaskIdVal  = nullptr;
    QLabel  *m_robotStatusVal  = nullptr;

    // 地图属性 (点击地图空白)
    QWidget *m_mapPropsGroup   = nullptr;
    QLabel  *m_mapPropIdVal    = nullptr;
    QLabel  *m_mapPropAreaVal  = nullptr;
    QWidget *m_mapCountGroup   = nullptr;
    QLabel  *m_mapCountPointVal= nullptr;
    QLabel  *m_mapCountHpVal   = nullptr;
    QLabel  *m_mapCountChargeVal=nullptr;
    QLabel  *m_mapCountLineVal = nullptr;
    QLabel  *m_mapCountCurveVal= nullptr;
    QLabel  *m_mapCountAreaVal = nullptr;

    // 图元页
    QLineEdit   *m_elemSearchEdit = nullptr;
    QListWidget *m_elemList       = nullptr;

    // 名称辅助: 路网图元的显示名 (优先 name, 其次 "起点 ⟷ 终点", 最后 id)
    QString stationNameById(const QString &id) const;
    QString routeFeatureName(const QJsonObject &obj) const;

    // 图层页
    QCheckBox   *m_hideSelectedChk = nullptr;
    QCheckBox   *m_hideStationsChk = nullptr;
    QCheckBox   *m_hideRoutesChk   = nullptr;
    QPushButton *m_showAllElemsBtn = nullptr;

    // Map metadata
    QString m_mapId, m_mapName, m_tilemapPath;
    QMap<int, QString> m_mapIdToName;
    bool m_isFetchingMapList = false;
    void fetchMapNameList(std::function<void()> onDone = nullptr);
    QString lookupMapName(int mapId);
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
    QJsonArray m_highLevelAreas;
    QGraphicsItem *m_areaItem = nullptr;

    // Robot state
    struct RobotState {
        QString carId = "1", carName;
        double wx = 0, wy = 0, yawRad = 0;
        double speed = 0, battery = -1, covScore = -1;
        int latency = -1;
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
    QToolButton *m_configBtn      = nullptr; // 12. 参数 (参数配置弹窗)
    QToolButton *m_teleopBtn      = nullptr; // 13. 抢占控制权
    QToolButton *m_statusBtn      = nullptr; // 14. 状态
    QToolButton *m_moreBtn        = nullptr; // 15. 其它 (弹出工具列表)
    class QMenu *m_moreMenu       = nullptr;
    QToolButton *m_resetViewBtn   = nullptr;
    QToolButton *m_refreshBtn     = nullptr;
    QPushButton *m_gridBtn        = nullptr;
    class DirectionPadWidget *m_directionPad = nullptr;
    RobotStatusDialog *m_statusDialog = nullptr;
    class MotionDialog *m_motionDialog = nullptr;
    class EditConfigWidget *m_editConfigWidget = nullptr;
    class QDialog *m_configDialog = nullptr;

    // Navigation & Move modes
    bool        m_navMode          = false;
    bool        m_moveMode         = false;
    bool        m_navActive        = false;
    bool        m_navPaused        = false;
    bool        m_navEnteredRunning= false;
    qint64      m_navStartTime     = 0;

    enum CurrentOperationType {
        OpNone,
        OpNavigation,
        OpMotion
    };
    CurrentOperationType m_currentOpType = OpNone;
    QString     m_currentNavTargetName;
    QString     m_currentMotionDesc;
    QString     m_lastRobotNavStatus = "0";

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
    QCheckBox            *m_chkStationType       = nullptr;
    QLabel               *m_stationTypeVal       = nullptr;
    QCheckBox            *m_chkStationId         = nullptr;
    QLabel               *m_stationIdVal         = nullptr;
    QCheckBox            *m_chkStationName       = nullptr;
    QLabel               *m_stationNameVal       = nullptr;
    QCheckBox            *m_chkStationPos        = nullptr;
    QLabel               *m_stationPosVal        = nullptr;
    QCheckBox            *m_chkStationYaw        = nullptr;
    QLabel               *m_stationYawVal        = nullptr;
    QCheckBox            *m_chkStationDock       = nullptr;
    QLabel               *m_stationDockVal       = nullptr;
    QCheckBox            *m_chkStationRotate     = nullptr;
    QLabel               *m_stationRotateVal     = nullptr;
    QCheckBox            *m_chkStationPgv        = nullptr;
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
    QCheckBox            *m_chkRouteType       = nullptr;
    QLabel               *m_routeTypeVal       = nullptr;
    QCheckBox            *m_chkRouteId         = nullptr;
    QLabel               *m_routeIdVal         = nullptr;
    QCheckBox            *m_chkRouteName       = nullptr;
    QLabel               *m_routeNameVal       = nullptr;
    QCheckBox            *m_chkRouteStart      = nullptr;
    QLabel               *m_routeStartVal      = nullptr;
    QCheckBox            *m_chkRouteEnd        = nullptr;
    QLabel               *m_routeEndVal        = nullptr;
    QCheckBox            *m_chkRouteDirect     = nullptr;
    QLabel               *m_routeDirectVal     = nullptr;
    QCheckBox            *m_chkRouteMaxSpeed   = nullptr;
    QLabel               *m_routeMaxSpeedVal   = nullptr;
    QCheckBox            *m_chkRouteMaxAcc     = nullptr;
    QLabel               *m_routeMaxAccVal     = nullptr;
    QLabel               *m_routeMaxDecVal     = nullptr;
    QCheckBox            *m_chkRouteMaxRot     = nullptr;
    QLabel               *m_routeMaxRotVal     = nullptr;
    QCheckBox            *m_chkRouteMaxRotAcc  = nullptr;
    QLabel               *m_routeMaxRotAccVal  = nullptr;
    QCheckBox            *m_chkRouteReachDist  = nullptr;
    QLabel               *m_routeReachDistVal  = nullptr;
    QCheckBox            *m_chkRouteReachAngle = nullptr;
    QLabel               *m_routeReachAngleVal = nullptr;
    QCheckBox            *m_chkRouteWidth      = nullptr;
    QLabel               *m_routeWidthVal      = nullptr;
    QCheckBox            *m_chkRouteObsRange   = nullptr;
    QLabel               *m_routeObsRangeVal   = nullptr;

    // Area info (对齐"启用编辑"高级区域属性面板)
    void showAreaDetails(const QJsonObject &obj);
    void clearAreaDetails();

    QGroupBox            *m_areaGroup          = nullptr;
    QLabel               *m_areaBadge          = nullptr;
    QLabel               *m_areaEmptyHint      = nullptr;
    QWidget              *m_areaContentWidget  = nullptr;
    QCheckBox            *m_chkAreaName        = nullptr;
    QLabel               *m_areaNameVal        = nullptr;
    QLabel               *m_areaBoundsVal      = nullptr;
    QCheckBox            *m_chkAreaMaxSpeed    = nullptr;
    QLabel               *m_areaMaxSpeedVal    = nullptr;
    QCheckBox            *m_chkAreaMaxAcc      = nullptr;
    QLabel               *m_areaMaxAccVal      = nullptr;
    QCheckBox            *m_chkAreaMaxRot      = nullptr;
    QLabel               *m_areaMaxRotVal      = nullptr;
    QCheckBox            *m_chkAreaMaxRotAcc   = nullptr;
    QLabel               *m_areaMaxRotAccVal   = nullptr;
    QCheckBox            *m_chkAreaReachDist   = nullptr;
    QLabel               *m_areaReachDistVal   = nullptr;
    QCheckBox            *m_chkAreaReachAngle  = nullptr;
    QLabel               *m_areaReachAngleVal  = nullptr;
    QCheckBox            *m_chkAreaWidth       = nullptr;
    QLabel               *m_areaWidthVal       = nullptr;
    QCheckBox            *m_chkAreaObsRange    = nullptr;
    QLabel               *m_areaObsRangeVal    = nullptr;

    QGraphicsItem        *m_selItem            = nullptr;
    QGraphicsItem        *m_layerHiddenItem    = nullptr;   // 图层-隐藏选中图元 所隐藏的图元
    QString               m_selectedStationId;

    // 请求去重与防抖节流
    bool    m_isLoadingCar  = false;
    QString m_loadingMapId;
    qint64  m_lastRefreshMs = 0;

    bool         m_mapLoadFailed        = false;
    bool         m_devicePointPending   = false;

    void showRobotContextMenu(const QPoint &globalPos);
    void onMarkStation();
    void onMarkDevicePoint(const QString &type);
    void saveRouteToServer(std::function<void(bool ok)> onDone = nullptr);

signals:
    void mapStatusChanged();

public:
    QString mapStatusText() const {
        if (m_mapReady) return QStringLiteral("成功");
        if (m_mapLoadFailed) return QStringLiteral("失败");
        if (m_mapId.isEmpty()) return QStringLiteral("未加载");
        return QStringLiteral("加载中...");
    }
    QString mapStatusColor() const {
        if (m_mapReady) return "#22c55e";
        if (m_mapLoadFailed) return "#ef4444";
        if (m_mapId.isEmpty()) return "#8898a8";
        return "#eab308";
    }
};

#endif // MAPVIEWWIDGET_H
