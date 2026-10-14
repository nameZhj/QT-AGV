#ifndef MAPEDITWIDGET_H
#define MAPEDITWIDGET_H

#include <QWidget>
#include "MapCanvas.h"
#include <QGraphicsScene>
#include <QGraphicsEllipseItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsPixmapItem>
#include <QJsonObject>
#include <QJsonArray>
#include <functional>
#include <QMap>
#include <QTableWidget>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QSet>
#include <QResizeEvent>

class RulerWidget;
class ToggleSwitch;

class MapEditWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MapEditWidget(const QString &mapId, QWidget *parent = nullptr);
    ~MapEditWidget();

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
    void setupUI();
    void loadMapData(std::function<void()> onDone = nullptr);
    void loadRouteData();
    void renderMap();
    void fitView();

    void addPoint(const QPointF &sp, double yaw = 0, double theta = 0);
    void addCharge(const QPointF &sp, double epcX = 0, double epcY = 0, double epcTheta = 0, double yaw = 0, double theta = 0);
    void addHighPrecisionPoint(const QPointF &sp, double epcX = 0, double epcY = 0, double epcTheta = 0, double yaw = 0, double theta = 0);
    // 设备打点: 通过设备相对定位(epc)添加充电点/精定位点
    // type: "charge" 或 "highPrecision"
    void addDevicePoint(const QString &type);
    void addLine(const QPointF &a, const QPointF &b,
                 const QString &startId = {}, const QString &endId = {});
    void addCurve(const QPointF &a, const QPointF &b,
                  const QString &startId = {}, const QString &endId = {});
    void finalizeCurve();   // 完成多阶贝塞尔曲线绘制(基于 m_curveCtrlPts)
    void addBezierCurve(const QList<QPointF> &scenePts,
                        const QList<QString> &snapIds);  // 保存任意阶贝塞尔曲线

    void updateCurveShape(const QString &id);            // update path item in-place
    void rebuildCurveHandles(const QString &id);         // sync handle items to JSON
    void removeAllHandles();                              // clears m_curveHandles from scene

    // 栅格地图画笔/橡皮涂绘
    void paintStroke(const QPointF &from, const QPointF &to, bool isPoint);
    void pushMapUndo();
    void undoMap();
    void redoMap();
    void updateHistoryButtons();

    // Snap helpers
    struct SnapResult {
        bool    valid    = false;
        QString type, id;
        QPointF scenePos, worldPos;
    };
    SnapResult findSnap(const QPointF &sp, double worldTol = 0.3) const;
    bool       hasOverlap(const QPointF &sp) const;   // 0.3 m world check
    void       updateSnapIndicator(const SnapResult &r);
    // 确保曲线端点处存在节点: 优先吸附到附近已有点，否则在该位置建一个新节点，
    // 返回该端点对应的节点 id 与吸附/创建后的场景坐标。
    // 仅用于曲线首尾端点，中间控制点不应调用。
    SnapResult ensureNodeAt(const QPointF &sp);
    // 检查两个站点之间是否已存在连线(直线或曲线，无向)
    bool hasRouteBetween(const QString &startId, const QString &endId,
                         const QPointF &sceneStart = QPointF(),
                         const QPointF &sceneEnd = QPointF()) const;

    // 把"屏幕像素"半径换算成当前缩放下的场景坐标半径，避免缩小地图后命中容差跟着缩小
    double hitTolScene(double screenPx) const;

    // Map overlays
    void updateGrid();
    void updateRulers();

    void updatePreview(const QPointF &sp);
    void clearPreview();
    void setDrawMode(int mode);
    void cancelDraw();

    void selectFeatureAt(const QPointF &sp);
    void clearSelection();
    void applyPropsEdit();
    void deleteSelected();
    void showFeatureProperties(const QString &type, const QString &id);

    void refreshTable();
    void updateCoordLabel(const QPointF &sp);

    void loadCarList();
    void initMqtt();
    void disconnectMqtt();
    void updateRobotMarker();
    void loadReflectorData();

    QPointF worldToScene(double wx, double wy) const;
    QPointF sceneToWorld(double sx, double sy) const;

    void setupShortcuts();

    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent *e) override;
    void keyPressEvent(QKeyEvent *event) override;

    // 抢占控制权与遥控
    void onTeleopClicked();
    void onTeleopActiveChanged(bool active);

    // 从已有数组中找出指定前缀的 ID 里最大的数字编号，返回 max+1
    // 保证删除再新增时不产生重复 ID
    static int nextIdNum(const QJsonArray &arr, const QString &prefix);


    enum DrawStatus { Drag=0, Point=1, Line=2, Curve=3, Brush=4, Eraser=5, Charge=6, HighPrecision=7, Area=8 };

    // Map view
    MapCanvas            *m_mapView      = nullptr;
    QGraphicsScene       *m_scene        = nullptr;
    QGraphicsPixmapItem  *m_mapBgItem    = nullptr;
    QLabel               *m_coordLabel   = nullptr;
    QLabel               *m_mapInfoLabel = nullptr;

    // Toolbar: mode → button
    QMap<int, QPushButton*> m_toolBtns;

    // 站点类型子工具条: 站点 / 工作站 / 特殊工作站 (对应 mode Point / HighPrecision / Charge)
    QWidget                  *m_stationBar = nullptr;
    QMap<int, QPushButton*>   m_stationBtns;

    // 区域类型子工具条: 高级区域 (对应 mode Area)
    QWidget                  *m_areaBar    = nullptr;
    QPushButton              *m_areaAdvBtn = nullptr;
    QPushButton *m_saveBtn       = nullptr;
    QPushButton *m_teleopBtn     = nullptr;
    class DirectionPadWidget *m_directionPad = nullptr;
    QPushButton *m_importSmapBtn = nullptr;
    QPushButton *m_undoBtn       = nullptr;
    QPushButton *m_redoBtn       = nullptr;
    QComboBox   *m_brushSizeCombo= nullptr;
    QPushButton *m_editMapBtn    = nullptr;
    QPushButton *m_tableModeBtn = nullptr;
    QPushButton *m_resetViewBtn = nullptr;
    QPushButton  *m_gridBtn     = nullptr;
    ToggleSwitch *m_robotBtn    = nullptr;
    ToggleSwitch *m_reflectorBtn = nullptr;
    RulerWidget *m_topRuler     = nullptr;
    RulerWidget *m_leftRuler    = nullptr;

    // Right panel
    QComboBox    *m_carCombo       = nullptr;
    QWidget      *m_tablePanel     = nullptr;
    QTableWidget *m_featureTable   = nullptr;
    QComboBox    *m_tableTypeCombo = nullptr;

    // Properties panel (工业级图元属性编辑器，分别对齐 WidgetMarkPPTW 与 WidgetBezierPPTW 规范)
    QWidget        *m_propsPanel         = nullptr;
    QLabel         *m_propHeaderTitle    = nullptr;
    QLabel         *m_propHeaderBadge    = nullptr;

    // --- 站点专属属性组 (WidgetMarkPPTW) ---
    QWidget        *m_stationPropGroup   = nullptr;
    QCheckBox      *m_chkStationType     = nullptr;
    QComboBox      *m_stationTypeCombo   = nullptr; // 普通站点 LM / 充电桩 CP / 高精定位 TL
    QLineEdit      *m_stationIdEdit      = nullptr;
    QCheckBox      *m_chkStationLabel    = nullptr;
    QLineEdit      *m_stationLabelEdit   = nullptr;
    QCheckBox      *m_chkStationX        = nullptr;
    QDoubleSpinBox *m_stationXSpin       = nullptr;
    QCheckBox      *m_chkStationY        = nullptr;
    QDoubleSpinBox *m_stationYSpin       = nullptr;
    QCheckBox      *m_chkStationYaw      = nullptr;
    QDoubleSpinBox *m_stationYawSpin     = nullptr;
    QCheckBox      *m_chkStationDockable = nullptr;
    QCheckBox      *m_stationDockableChk = nullptr; // 允许停靠 rdsDockable
    QCheckBox      *m_chkStationRotate   = nullptr;
    QCheckBox      *m_stationRotateChk   = nullptr; // 允许转身 allowRotate
    QCheckBox      *m_chkStationPgv      = nullptr;
    QCheckBox      *m_stationPgvChk      = nullptr; // 使用下视 PGV
    QWidget        *m_stationEpcGroup    = nullptr; // EPC相对定位数据
    QLabel         *m_stationEpcXLabel   = nullptr;
    QLabel         *m_stationEpcYLabel   = nullptr;

    // --- 线路专属属性组 (WidgetBezierPPTW) ---
    QWidget        *m_linePropGroup      = nullptr;
    QComboBox      *m_lineTypeCombo      = nullptr; // 直线 / 贝塞尔曲线
    QLineEdit      *m_lineIdEdit         = nullptr;
    QCheckBox      *m_chkLineName        = nullptr;
    QLineEdit      *m_lineNameEdit       = nullptr;
    QLabel         *m_lineStartLabel     = nullptr; // 起点 (如 LM1)
    QLabel         *m_lineEndLabel       = nullptr; // 终点 (如 LM2)
    QCheckBox      *m_chkLineDirect      = nullptr;
    QComboBox      *m_lineDirectCombo    = nullptr; // 双向 ↔ / 单向前进 → / 单向倒走 ←
    QPushButton    *m_lineFlipDirBtn     = nullptr; // 翻转方向 ⇄
    QCheckBox      *m_chkLineMaxSpeed    = nullptr;
    QDoubleSpinBox *m_lineMaxSpeedSpin   = nullptr; // 最大速度 m/s
    QCheckBox      *m_chkLineMaxAcc      = nullptr;
    QDoubleSpinBox *m_lineMaxAccSpin     = nullptr; // 最大加速度 m/s²
    QDoubleSpinBox *m_lineMaxDecSpin     = nullptr; // 最大减速度 m/s²
    QCheckBox      *m_chkLineMaxRot      = nullptr;
    QDoubleSpinBox *m_lineMaxRotSpin     = nullptr; // 最大旋转速度 °/s
    QCheckBox      *m_chkLineMaxRotAcc   = nullptr;
    QDoubleSpinBox *m_lineMaxRotAccSpin  = nullptr; // 最大角加减速度 °/s²
    QCheckBox      *m_chkLineReachDist   = nullptr;
    QDoubleSpinBox *m_lineReachDistSpin  = nullptr; // 到达距离精度 m
    QCheckBox      *m_chkLineReachAngle  = nullptr;
    QDoubleSpinBox *m_lineReachAngleSpin = nullptr; // 到达角度精度 °
    QCheckBox      *m_chkLineRoadWidth   = nullptr;
    QDoubleSpinBox *m_lineRoadWidthSpin  = nullptr; // 路线宽度 m

    // --- 实时避障范围属性组 (对齐 base.json 规范) ---
    QCheckBox      *m_chkLineObsStopLen  = nullptr;
    QDoubleSpinBox *m_lineObsStopLenSpin  = nullptr; // 正向停止膨胀半径 (m)
    QCheckBox      *m_chkLineObsStopWid  = nullptr;
    QDoubleSpinBox *m_lineObsStopWidSpin  = nullptr; // 侧向停止膨胀半径 (m)
    QCheckBox      *m_chkLineObsDecelLen = nullptr;
    QDoubleSpinBox *m_lineObsDecelLenSpin = nullptr; // 正向减速膨胀半径 (m)
    QCheckBox      *m_chkLineObsDecelWid = nullptr;
    QDoubleSpinBox *m_lineObsDecelWidSpin = nullptr; // 侧向减速膨胀半径 (m)

    // --- 高级区域专属属性组 ---
    QWidget        *m_areaPropGroup       = nullptr;
    QCheckBox      *m_chkAreaName         = nullptr;
    QLineEdit      *m_areaNameEdit        = nullptr;
    QLabel         *m_areaBoundsLabel     = nullptr;
    QCheckBox      *m_chkAreaMaxSpeed     = nullptr;
    QDoubleSpinBox *m_areaMaxSpeedSpin    = nullptr;
    QCheckBox      *m_chkAreaMaxAcc       = nullptr;
    QDoubleSpinBox *m_areaMaxAccSpin      = nullptr;
    QCheckBox      *m_chkAreaMaxRot       = nullptr;
    QDoubleSpinBox *m_areaMaxRotSpin      = nullptr;
    QCheckBox      *m_chkAreaMaxRotAcc    = nullptr;
    QDoubleSpinBox *m_areaMaxRotAccSpin   = nullptr;
    QCheckBox      *m_chkAreaReachDist    = nullptr;
    QDoubleSpinBox *m_areaReachDistSpin   = nullptr;
    QCheckBox      *m_chkAreaReachAngle   = nullptr;
    QDoubleSpinBox *m_areaReachAngleSpin  = nullptr;
    QCheckBox      *m_chkAreaRoadWidth    = nullptr;
    QDoubleSpinBox *m_areaRoadWidthSpin   = nullptr;
    QCheckBox      *m_chkAreaObsStopLen   = nullptr;
    QDoubleSpinBox *m_areaObsStopLenSpin  = nullptr;
    QCheckBox      *m_chkAreaObsStopWid   = nullptr;
    QDoubleSpinBox *m_areaObsStopWidSpin  = nullptr;
    QCheckBox      *m_chkAreaObsDecelLen  = nullptr;
    QDoubleSpinBox *m_areaObsDecelLenSpin = nullptr;
    QCheckBox      *m_chkAreaObsDecelWid  = nullptr;
    QDoubleSpinBox *m_areaObsDecelWidSpin = nullptr;

    // 操作按钮
    QPushButton    *m_propApplyBtn       = nullptr;
    QPushButton    *m_propDeleteBtn      = nullptr;

    // Data
    QString    m_mapId, m_mapName;
    QImage     m_mapImage;
    QImage     m_originalMapImage;
    int        m_brushSize     = 4;
    bool       m_isPainting    = false;
    QPointF    m_lastPaintImgPos;
    QList<QImage> m_undoStack;
    QList<QImage> m_redoStack;
    QJsonArray m_points, m_lines, m_curves, m_charges, m_highPrecisionPoints;
    QJsonArray m_highLevelAreas; // 高级区域对象列表 (支持多个高级区域)
    bool       m_isDrawingArea = false;
    QPointF    m_areaStartScenePos;
    QGraphicsRectItem *m_areaPreviewItem = nullptr;

    // 高级区域拖拽移动与拉伸调整状态
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
    bool        m_isDraggingArea         = false;
    AreaHitPart m_areaDragPart           = AreaHit_None;
    QPointF     m_areaDragStartScenePos;
    double      m_areaDragStartMinX      = 0.0;
    double      m_areaDragStartMaxX      = 0.0;
    double      m_areaDragStartMinY      = 0.0;
    double      m_areaDragStartMaxY      = 0.0;

    AreaHitPart getAreaHitPart(const QPointF &sp, const QJsonObject &areaObj) const;

    // Coordinate transform (world extent + thumbnail size)
    double m_worldL = 0, m_worldB = 0, m_worldR = 0, m_worldT = 0;
    double m_imgW   = 0, m_imgH   = 0;

    // Draw state
    DrawStatus m_drawStatus    = Drag;
    bool       m_hasInitialFit = false;
    bool       m_tableVisible  = false;
    bool       m_dirty         = false;   // 路网是否有未保存的修改
    QString    m_currentTable  = "point";
    QPointF    m_drawP1;
    QString    m_drawP1Id;               // snap ID of first draw point
    bool       m_drawHasP1     = false;

    // Multi-control-point Bezier drawing state
    QList<QPointF> m_curveCtrlPts;        // 当前正在绘制的曲线控制点(场景坐标)
    QList<QString> m_curveCtrlIds;        // 控制点对应的吸附ID(首尾有效)
    int            m_curveHoverHandleIdx = -1;  // 鼠标悬停的控制点序号

    // Preview rubber-band
    QGraphicsItem        *m_previewItem    = nullptr;
    QList<QGraphicsLineItem*> m_previewAids;  // 多阶贝塞尔预览的辅助参考线
    QGraphicsEllipseItem *m_snapIndicator  = nullptr;

    // Map overlays
    bool m_showGrid = false;

    // 反光柱（开关控制显示，默认隐藏，与 Vue edit.vue 的 isShowReflectivePillar 一致）
    bool       m_showReflectors = false;
    QJsonArray m_reflectorPoles;

    // Selection
    QString        m_selType, m_selId;
    QGraphicsItem *m_selItem = nullptr;

    // Feature items: "type_id" → scene item
    QMap<QString, QGraphicsItem*> m_featureItems;

    // Bezier handles: "curve_id" → [p1, ctrl, p2] handle ellipses
    QMap<QString, QList<QGraphicsEllipseItem*>> m_curveHandles;

    // Handle drag state
    QGraphicsEllipseItem *m_activeHandle     = nullptr;
    QString               m_handleCurveId;
    int                   m_handleIndex      = -1;  // 0=P1, 1=ctrl, 2=P2
    bool                  m_isDraggingHandle = false;

    // Station node drag state
    bool                  m_isDraggingNode   = false;
    QString               m_draggingNodeType;
    QString               m_draggingNodeId;
    QPointF               m_dragStartScenePos;
    QPointF               m_dragStartWorldPos;

    // Manual pan state (替代 QGraphicsView::ScrollHandDrag，避免与手柄拖拽冲突)
    bool    m_panning     = false;
    QPoint  m_lastPanPos;

    void    syncLineObstaclesToEndpoints();

    // MQTT
    QJsonArray   m_carList;
    QSet<QString> m_onlineCarIds;
    QString      m_currentCarId = "1";

    // 实时设备位置标记（开关控制显示）
    bool                   m_showRobotPosition = false;
    QGraphicsPixmapItem   *m_robotItem         = nullptr;
    QPixmap                m_robotPixmap;
    double                 m_carX = 0, m_carY = 0, m_carYaw = 0, m_carTheta = 0;
    bool                   m_hasCarPose        = false;
    // 设备相对定位末端坐标(epc): 由 /rob/{id}/info 推送，用于充电桩/高精度点打点
    double                 m_epcX = 0, m_epcY = 0, m_epcTheta = 0;
    bool                   m_hasEpcPose        = false;
    // 设备打点进行中标志(防止重复触发)
    bool                   m_devicePointPending = false;
};

#endif // MAPEDITWIDGET_H
