#pragma once

#include <QWidget>
#include <QJsonObject>
#include <QJsonArray>
#include <QImage>
#include <QPixmap>
#include <QMap>
#include <QSet>
#include <QList>
#include <QPointer>
#include <QPointF>
#include <functional>
#include "MapCheckResultDialog.h"

// 前置声明轻量化头文件，减少编译依赖
class MapCanvas;
class RulerWidget;
class ToggleSwitch;
class DirectionPadWidget;

class QGraphicsScene;
class QGraphicsPixmapItem;
class QGraphicsEllipseItem;
class QGraphicsPolygonItem;
class QGraphicsLineItem;
class QGraphicsRectItem;
class QGraphicsItem;

class QLabel;
class QPushButton;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QCheckBox;
class QTableWidget;
class QVBoxLayout;
class QHBoxLayout;
class QSplitter;
class QKeyEvent;
class QMouseEvent;
class QResizeEvent;

class MapEditWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MapEditWidget(const QString &mapId, QWidget *parent = nullptr);
    ~MapEditWidget() override;

    // 检查是否有未保存的修改，有则弹窗询问。返回 true 表示可以离开，false 表示用户取消
    bool checkUnsavedChanges();

signals:
    void goBack();

private slots:
    void onSaveMap(std::function<void(bool success)> onFinished = nullptr);
    void onImportSmap();
    void onUndo();
    void onRedo();
    void onEditMap();
    void onTableToggle();
    void onDeleteFeature();
    void showHelpDialog();

private:
    // ── 内部枚举与数据结构 ──────────────────────────────────────────────────────────
    enum DrawStatus {
        Drag = 0,
        Point = 1,
        Line = 2,
        Curve = 3,
        Brush = 4,
        Eraser = 5,
        Charge = 6,
        HighPrecision = 7,
        Area = 8
    };

    enum AreaHitPart {
        AreaHit_None = 0,
        AreaHit_TopLeft,
        AreaHit_TopRight,
        AreaHit_BottomLeft,
        AreaHit_BottomRight,
        AreaHit_TopEdge,
        AreaHit_BottomEdge,
        AreaHit_LeftEdge,
        AreaHit_RightEdge,
        AreaHit_Body
    };

    struct SnapResult {
        bool    valid = false;
        QString type;
        QString id;
        QPointF scenePos;
        QPointF worldPos;
    };

    // ── 1. UI 界面构建子模块 ───────────────────────────────────────────────────────
    void setupUI();
    void setupTopBar(QVBoxLayout *root);
    void setupSubToolBars(QVBoxLayout *root);
    void setupLeftToolbar(QHBoxLayout *contentLayout);
    void setupMapViewArea(QSplitter *splitter);
    void setupRightPanel(QSplitter *splitter);
    void setupStationPropsUI(QVBoxLayout *layout);
    void setupLinePropsUI(QVBoxLayout *layout);
    void setupAreaPropsUI(QVBoxLayout *layout);
    void setupFeatureTableUI(QVBoxLayout *layout);
    void setupShortcuts();

    // ── 2. 数据加载、校验与保存 ────────────────────────────────────────────────────
    void loadMapData(std::function<void()> onDone = nullptr);
    void loadRouteData();
    void loadReflectorData();
    void loadCarList();
    void flushCurveHandlesToJson();
    QJsonObject buildRouteGeoJson() const;
    bool validateRouteNetwork(QList<CheckResultItem> &results);
    void focusFeature(const QString &targetType, const QString &targetId);

    // ── 3. 地图渲染与视口刷新 ──────────────────────────────────────────────────────
    void renderMap();
    void renderHighLevelAreas();
    void renderStationNodes();
    void renderRouteLines();
    void renderRouteCurves();
    void renderReflectorPoles();
    void fitView();
    void updateGrid();
    void updateRulers();
    void updateCoordLabel(const QPointF &sp);

    // ── 4. 图元增删、曲线控制与设备打点 ───────────────────────────────────────────
    void addPoint(const QPointF &sp, double yaw = 0, double theta = 0);
    void addCharge(const QPointF &sp, double epcX = 0, double epcY = 0, double epcTheta = 0, double yaw = 0, double theta = 0);
    void addHighPrecisionPoint(const QPointF &sp, double epcX = 0, double epcY = 0, double epcTheta = 0, double yaw = 0, double theta = 0);
    void addDevicePoint(const QString &type);
    void addLine(const QPointF &a, const QPointF &b, const QString &startId = {}, const QString &endId = {});
    void addCurve(const QPointF &a, const QPointF &b, const QString &startId = {}, const QString &endId = {});
    void finalizeCurve();
    void addBezierCurve(const QList<QPointF> &scenePts, const QList<QString> &snapIds);
    void updateCurveShape(const QString &id);
    void rebuildCurveHandles(const QString &id);
    void removeAllHandles();
    void deleteSelected();

    // ── 5. 属性面板展示与应用修改 ──────────────────────────────────────────────────
    void selectFeatureAt(const QPointF &sp);
    void clearSelection();
    void showFeatureProperties(const QString &type, const QString &id);
    void populateStationProps(const QJsonObject &obj, const QString &type, const QString &id);
    void populateLineProps(const QJsonObject &obj, const QString &type, const QString &id);
    void populateAreaProps(const QJsonObject &obj);
    void applyPropsEdit();
    void applyStationPropsEdit();
    void applyLinePropsEdit();
    void applyAreaPropsEdit();
    void refreshTable();

    // ── 6. 画布交互事件与拖拽处理 ──────────────────────────────────────────────────
    bool eventFilter(QObject *obj, QEvent *event) override;
    bool handleCanvasMouseMove(QMouseEvent *me);
    bool handleCanvasMousePress(QMouseEvent *me);
    bool handleCanvasMouseRelease(QMouseEvent *me);
    void dragMoveArea(const QPointF &sp);
    void dragMoveNode(const QPointF &sp);
    void dragMoveCurveHandle(const QPointF &sp);
    void setDrawMode(int mode);
    void cancelDraw();
    void updatePreview(const QPointF &sp);
    void clearPreview();
    AreaHitPart getAreaHitPart(const QPointF &sp, const QJsonObject &areaObj) const;

    // ── 7. 栅格地图笔刷涂绘与历史撤销 ──────────────────────────────────────────────
    void paintStroke(const QPointF &from, const QPointF &to, bool isPoint);
    void pushMapUndo();
    void undoMap();
    void redoMap();
    void updateHistoryButtons();

    // ── 8. 几何计算、吸附与辅助工具 (DRY) ─────────────────────────────────────────
    SnapResult findSnap(const QPointF &sp, double worldTol = 0.3) const;
    bool hasOverlap(const QPointF &sp) const;
    void updateSnapIndicator(const SnapResult &r);
    SnapResult ensureNodeAt(const QPointF &sp);
    bool hasRouteBetween(const QString &startId, const QString &endId,
                         const QPointF &sceneStart = QPointF(),
                         const QPointF &sceneEnd = QPointF()) const;
    double hitTolScene(double screenPx) const;
    QPointF worldToScene(double wx, double wy) const;
    QPointF sceneToWorld(double sx, double sy) const;
    static int nextIdNum(const QJsonArray &arr, const QString &prefix);

    void syncMotionSpinBoxRanges();
    void updateConnectedRoutesPos(const QString &nodeId, double newX, double newY, bool updateX = true, bool updateY = true);
    void syncLineObstaclesToEndpoints();

    // ── 9. MQTT 通信与遥控 ────────────────────────────────────────────────────────
    void initMqtt();
    void disconnectMqtt();
    void updateRobotMarker();
    void onTeleopClicked();
    void onTeleopActiveChanged(bool active);

    // ── 10. Qt 窗口事件重写 ───────────────────────────────────────────────────────
    void resizeEvent(QResizeEvent *e) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    // ══════════════════════════════════════════════════════════════════════════════
    // 私有成员变量 (按职责分组排列)
    // ══════════════════════════════════════════════════════════════════════════════

    // ── 视图与基础画布控件 ────────────────────────────────────────────────────────
    MapCanvas           *m_mapView       = nullptr;
    QGraphicsScene      *m_scene         = nullptr;
    QGraphicsPixmapItem *m_mapBgItem     = nullptr;
    QLabel              *m_coordLabel    = nullptr;
    QLabel              *m_mapInfoLabel  = nullptr;
    RulerWidget         *m_topRuler      = nullptr;
    RulerWidget         *m_leftRuler     = nullptr;

    // ── 工具栏与操作按钮 ──────────────────────────────────────────────────────────
    QMap<int, QPushButton*>  m_toolBtns;
    QWidget                 *m_stationBar       = nullptr;
    QMap<int, QPushButton*>  m_stationBtns;
    QWidget                 *m_areaBar          = nullptr;
    QPushButton             *m_areaAdvBtn       = nullptr;
    QPushButton             *m_saveBtn          = nullptr;
    QPushButton             *m_teleopBtn        = nullptr;
    DirectionPadWidget      *m_directionPad     = nullptr;
    QPushButton             *m_importSmapBtn    = nullptr;
    QPushButton             *m_undoBtn          = nullptr;
    QPushButton             *m_redoBtn          = nullptr;
    QComboBox               *m_brushSizeCombo   = nullptr;
    QPushButton             *m_editMapBtn       = nullptr;
    QPushButton             *m_tableModeBtn     = nullptr;
    QPushButton             *m_resetViewBtn     = nullptr;
    QPushButton             *m_gridBtn          = nullptr;
    ToggleSwitch            *m_robotBtn         = nullptr;
    ToggleSwitch            *m_reflectorBtn     = nullptr;

    // ── 右侧面板 (属性与列表) ──────────────────────────────────────────────────────
    QComboBox               *m_carCombo         = nullptr;
    QWidget                 *m_tablePanel       = nullptr;
    QTableWidget            *m_featureTable     = nullptr;
    QComboBox               *m_tableTypeCombo   = nullptr;
    QWidget                 *m_propsPanel       = nullptr;
    QLabel                  *m_propHeaderTitle  = nullptr;
    QLabel                  *m_propHeaderBadge  = nullptr;
    QPushButton             *m_propApplyBtn     = nullptr;
    QPushButton             *m_propDeleteBtn    = nullptr;

    // 站点专属属性组 (WidgetMarkPPTW)
    QWidget                 *m_stationPropGroup   = nullptr;
    QCheckBox               *m_chkStationType     = nullptr;
    QComboBox               *m_stationTypeCombo   = nullptr;
    QLineEdit               *m_stationIdEdit      = nullptr;
    QCheckBox               *m_chkStationLabel    = nullptr;
    QLineEdit               *m_stationLabelEdit   = nullptr;
    QCheckBox               *m_chkStationX        = nullptr;
    QDoubleSpinBox          *m_stationXSpin       = nullptr;
    QCheckBox               *m_chkStationY        = nullptr;
    QDoubleSpinBox          *m_stationYSpin       = nullptr;
    QCheckBox               *m_chkStationYaw      = nullptr;
    QDoubleSpinBox          *m_stationYawSpin     = nullptr;
    QCheckBox               *m_chkStationDockable = nullptr;
    QCheckBox               *m_stationDockableChk = nullptr;
    QCheckBox               *m_chkStationRotate   = nullptr;
    QCheckBox               *m_stationRotateChk   = nullptr;
    QCheckBox               *m_chkStationPgv      = nullptr;
    QCheckBox               *m_stationPgvChk      = nullptr;
    QWidget                 *m_stationEpcGroup    = nullptr;
    QLabel                  *m_stationEpcXLabel   = nullptr;
    QLabel                  *m_stationEpcYLabel   = nullptr;

    // 线路专属属性组 (WidgetBezierPPTW)
    QWidget                 *m_linePropGroup      = nullptr;
    QComboBox               *m_lineTypeCombo      = nullptr;
    QLineEdit               *m_lineIdEdit         = nullptr;
    QCheckBox               *m_chkLineName        = nullptr;
    QLineEdit               *m_lineNameEdit       = nullptr;
    QLabel                  *m_lineStartLabel     = nullptr;
    QLabel                  *m_lineEndLabel       = nullptr;
    QCheckBox               *m_chkLineDirect      = nullptr;
    QComboBox               *m_lineDirectCombo    = nullptr;
    QPushButton             *m_lineFlipDirBtn     = nullptr;
    QCheckBox               *m_chkLineMaxSpeed    = nullptr;
    QDoubleSpinBox          *m_lineMaxSpeedSpin   = nullptr;
    QCheckBox               *m_chkLineMaxAcc      = nullptr;
    QDoubleSpinBox          *m_lineMaxAccSpin     = nullptr;
    QDoubleSpinBox          *m_lineMaxDecSpin     = nullptr;
    QCheckBox               *m_chkLineMaxRot      = nullptr;
    QDoubleSpinBox          *m_lineMaxRotSpin     = nullptr;
    QCheckBox               *m_chkLineMaxRotAcc   = nullptr;
    QDoubleSpinBox          *m_lineMaxRotAccSpin  = nullptr;
    QCheckBox               *m_chkLineReachDist   = nullptr;
    QDoubleSpinBox          *m_lineReachDistSpin  = nullptr;
    QCheckBox               *m_chkLineReachAngle  = nullptr;
    QDoubleSpinBox          *m_lineReachAngleSpin = nullptr;
    QCheckBox               *m_chkLineRoadWidth   = nullptr;
    QDoubleSpinBox          *m_lineRoadWidthSpin  = nullptr;

    // 实时避障范围属性组
    QCheckBox               *m_chkLineObs         = nullptr;
    QDoubleSpinBox          *m_lineObsStopLenSpin = nullptr;
    QDoubleSpinBox          *m_lineObsStopWidSpin = nullptr;
    QDoubleSpinBox          *m_lineObsDecelLenSpin= nullptr;
    QDoubleSpinBox          *m_lineObsDecelWidSpin= nullptr;

    // 高级区域专属属性组
    QWidget                 *m_areaPropGroup       = nullptr;
    QCheckBox               *m_chkAreaName         = nullptr;
    QLineEdit               *m_areaNameEdit        = nullptr;
    QLabel                  *m_areaBoundsLabel     = nullptr;
    QCheckBox               *m_chkAreaMaxSpeed     = nullptr;
    QDoubleSpinBox          *m_areaMaxSpeedSpin    = nullptr;
    QCheckBox               *m_chkAreaMaxAcc       = nullptr;
    QDoubleSpinBox          *m_areaMaxAccSpin      = nullptr;
    QCheckBox               *m_chkAreaMaxRot       = nullptr;
    QDoubleSpinBox          *m_areaMaxRotSpin      = nullptr;
    QCheckBox               *m_chkAreaMaxRotAcc    = nullptr;
    QDoubleSpinBox          *m_areaMaxRotAccSpin   = nullptr;
    QCheckBox               *m_chkAreaReachDist    = nullptr;
    QDoubleSpinBox          *m_areaReachDistSpin   = nullptr;
    QCheckBox               *m_chkAreaReachAngle   = nullptr;
    QDoubleSpinBox          *m_areaReachAngleSpin  = nullptr;
    QCheckBox               *m_chkAreaRoadWidth    = nullptr;
    QDoubleSpinBox          *m_areaRoadWidthSpin   = nullptr;
    QCheckBox               *m_chkAreaObs          = nullptr;
    QDoubleSpinBox          *m_areaObsStopLenSpin  = nullptr;
    QDoubleSpinBox          *m_areaObsStopWidSpin  = nullptr;
    QDoubleSpinBox          *m_areaObsDecelLenSpin = nullptr;
    QDoubleSpinBox          *m_areaObsDecelWidSpin = nullptr;

    // ── 地图与路网核心数据 ────────────────────────────────────────────────────────
    QString     m_mapId;
    QString     m_mapName;
    QImage      m_mapImage;
    QImage      m_originalMapImage;
    QList<QImage> m_undoStack;
    QList<QImage> m_redoStack;

    QJsonArray  m_points;
    QJsonArray  m_lines;
    QJsonArray  m_curves;
    QJsonArray  m_charges;
    QJsonArray  m_highPrecisionPoints;
    QJsonArray  m_highLevelAreas;
    QJsonArray  m_reflectorPoles;

    // ── 交互绘制与选择状态 ────────────────────────────────────────────────────────
    DrawStatus  m_drawStatus    = Drag;
    bool        m_hasInitialFit = false;
    bool        m_tableVisible  = false;
    bool        m_dirty         = false;
    QString     m_currentTable  = "point";

    // 栅格涂绘状态
    int         m_brushSize     = 4;
    bool        m_isPainting    = false;
    QPointF     m_lastPaintImgPos;

    // 直线/简单绘制状态
    QPointF     m_drawP1;
    QString     m_drawP1Id;
    bool        m_drawHasP1     = false;

    // 多阶贝塞尔绘制状态
    QList<QPointF> m_curveCtrlPts;
    QList<QString> m_curveCtrlIds;

    // 高级区域新建与拖拽调整状态
    bool        m_isDrawingArea         = false;
    QPointF     m_areaStartScenePos;
    QGraphicsRectItem *m_areaPreviewItem = nullptr;
    bool        m_isDraggingArea        = false;
    AreaHitPart m_areaDragPart          = AreaHit_None;
    QPointF     m_areaDragStartScenePos;
    double      m_areaDragStartMinX     = 0.0;
    double      m_areaDragStartMaxX     = 0.0;
    double      m_areaDragStartMinY     = 0.0;
    double      m_areaDragStartMaxY     = 0.0;

    // 坐标转换范围与图像尺寸
    double      m_worldL = 0.0, m_worldB = 0.0, m_worldR = 0.0, m_worldT = 0.0;
    double      m_imgW   = 0.0, m_imgH   = 0.0;

    // 交互预览图元
    QGraphicsItem             *m_previewItem    = nullptr;
    QList<QGraphicsLineItem*>  m_previewAids;
    QGraphicsEllipseItem      *m_snapIndicator  = nullptr;
    bool                       m_showGrid       = false;
    bool                       m_showReflectors = false;

    // 当前选中的图元
    QString                    m_selType;
    QString                    m_selId;
    QGraphicsItem             *m_selItem        = nullptr;

    // 图元缓存与手柄映射
    QMap<QString, QGraphicsItem*>               m_featureItems;
    QMap<QString, QList<QGraphicsEllipseItem*>> m_curveHandles;

    // 贝塞尔手柄拖拽状态
    QGraphicsEllipseItem      *m_activeHandle     = nullptr;
    QString                    m_handleCurveId;
    int                        m_handleIndex      = -1;
    bool                       m_isDraggingHandle = false;

    // 站点节点拖拽状态
    bool                       m_isDraggingNode   = false;
    QString                    m_draggingNodeType;
    QString                    m_draggingNodeId;
    QPointF                    m_dragStartScenePos;
    QPointF                    m_dragStartWorldPos;

    // 画布手动平移状态
    bool                       m_panning          = false;
    QPoint                     m_lastPanPos;

    // ── 设备通信与定位状态 ────────────────────────────────────────────────────────
    QJsonArray                 m_carList;
    QString                    m_currentCarId     = "1";
    bool                       m_showRobotPosition= false;
    QGraphicsPixmapItem       *m_robotItem        = nullptr;
    QPixmap                    m_robotPixmap;
    double                     m_carX = 0, m_carY = 0, m_carYaw = 0, m_carTheta = 0;
    bool                       m_hasCarPose       = false;
    double                     m_epcX = 0, m_epcY = 0, m_epcTheta = 0;
    bool                       m_hasEpcPose       = false;
    bool                       m_devicePointPending = false;

    // ── 校验结果弹窗 ──────────────────────────────────────────────────────────────
    QPointer<MapCheckResultDialog> m_checkResultDlg;
};

