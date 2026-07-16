#ifndef TASKEDITWIDGET_H
#define TASKEDITWIDGET_H

#include <QWidget>
#include "MapCanvas.h"
#include "core/ToggleSwitch.h"
#include <QGraphicsScene>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QLineEdit>
#include <QTextEdit>
#include <QTimeEdit>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QTableWidget>
#include <QResizeEvent>
#include <QDialog>

class RulerWidget;
class QGraphicsEllipseItem;

class TaskEditWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TaskEditWidget(const QString &taskId, QWidget *parent = nullptr);
    ~TaskEditWidget();

signals:
    void goBack();

private slots:
    void onSaveDraw();
    void onCarChanged(int index);
    void onMapComboChanged(int index);
    void onTableToggle();
    void onDeleteTarget();
    void onMoveTargetUp();
    void onMoveTargetDown();
    void onAddAction();
    void onDeleteAction();
    void onEditAction();
    void onActionTargetSelectionChanged();
    void onApplyTargetProps();

private:
    void setupUI();
    void loadMapList();
    void loadCarList();
    void loadTaskData();
    void loadMapData();
    void loadRouteData();
    void loadReflectorData();
    void renderMap();
    void fitView();
    void updateRulers();
    void addTargetPoint(const QPointF &scenePos);
    void refreshTargetTable();
    void refreshActionTable();
    void refreshTargetPropsPanel();
    void updateCoordinateDisplay(const QPointF &pos);
    void updateTargetSelectionHighlight();
    QGraphicsEllipseItem *targetItemAt(const QPointF &scenePos) const;

    QPointF worldToScene(double wx, double wy) const;
    QPointF sceneToWorld(double sx, double sy) const;

    // 任务点全局最近吸附: 在所有路网点/线/曲线中找到离点击位置最近的目标，
    // 不限距离，始终返回最近的一个。用于绘制任务点时强制吸附到路网。
    struct SnapResult {
        bool    valid    = false;
        QString type;        // "point" | "charge" | "highPrecision" | "line" | "curve"
        QString id;
        QPointF scenePos;    // 吸附目标在场景中的坐标
        QPointF worldPos;    // 吸附目标在世界坐标中的坐标
    };
    SnapResult findNearestRoadSnap(const QPointF &scenePos) const;
    // 显示/隐藏吸附指示圈(绿色环)，nullptr 或 invalid 时清除
    void updateSnapIndicator(const SnapResult &r);

    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent *e) override;

    enum DrawStatus { Drag = 0, Target = 1 };

    // UI - 地图
    MapCanvas      *m_mapView    = nullptr;
    QGraphicsScene *m_scene      = nullptr;
    QLabel         *m_coordLabel = nullptr;
    QLabel         *m_mapInfoLabel = nullptr;
    RulerWidget    *m_topRuler   = nullptr;
    RulerWidget    *m_leftRuler  = nullptr;

    // UI - 工具
    QPushButton  *m_saveBtn          = nullptr;
    QPushButton  *m_taskSettingsBtn  = nullptr;
    QPushButton  *m_gridBtn          = nullptr;
    ToggleSwitch *m_reflectorBtn     = nullptr;
    QComboBox    *m_targetTableCombo = nullptr;
    QTableWidget *m_targetTable      = nullptr;
    QWidget      *m_tablePanel       = nullptr;

    // UI - 地图/设备选择
    QComboBox      *m_mapCombo           = nullptr;
    QComboBox      *m_carCombo           = nullptr;  // 保留用于数据兼容，不再显示在UI
    QPushButton    *m_dragBtn            = nullptr;
    QPushButton    *m_targetBtn          = nullptr;
    QLineEdit      *m_taskNameEdit       = nullptr;
    QTextEdit      *m_remarkEdit         = nullptr;
    QCheckBox      *m_isLoopCheck        = nullptr;
    QSpinBox       *m_executionCountSpin = nullptr;
    QDoubleSpinBox *m_speedSpin          = nullptr;
    QComboBox      *m_timingCombo        = nullptr;
    QTimeEdit      *m_startTimeEdit      = nullptr;
    QDialog        *m_taskSettingsDialog = nullptr;

    // UI - 动作
    QComboBox    *m_actionCombo    = nullptr;
    QPushButton  *m_addActionBtn   = nullptr;
    QPushButton  *m_editActionBtn  = nullptr;
    QPushButton  *m_deleteActionBtn = nullptr;
    QTableWidget *m_actionTable    = nullptr;

    // UI - 目标点属性（选中目标点列表某行后显示）
    QWidget        *m_targetPropsPanel = nullptr;
    QLineEdit      *m_targetIdEdit     = nullptr;
    QLineEdit      *m_targetNameEdit   = nullptr;
    QDoubleSpinBox *m_targetXSpin      = nullptr;
    QDoubleSpinBox *m_targetYSpin      = nullptr;
    QDoubleSpinBox *m_targetYawSpin    = nullptr;
    QPushButton    *m_applyTargetBtn   = nullptr;
    QPushButton    *m_cancelTargetBtn  = nullptr;

    // 数据
    QString    m_taskId, m_mapId, m_mapName;
    QImage     m_mapImage;
    QJsonArray m_points, m_lines, m_curves, m_targets, m_carList;
    QJsonArray m_charges, m_highPrecisionPoints;
    QJsonObject m_taskForm, m_form;

    // 坐标转换参数 (world extent + thumbnail size)
    double m_worldL = 0, m_worldB = 0, m_worldR = 0, m_worldT = 0;
    double m_imgW   = 0, m_imgH   = 0;

    DrawStatus m_drawStatus      = Drag;
    bool       m_tableVisible    = false;
    bool       m_hasInitialFit   = false;
    bool       m_showGrid        = false;

    // 反光柱（开关控制显示，默认隐藏）
    bool       m_showReflectors  = false;
    QJsonArray m_reflectorPoles;

    // 图元映射
    QMap<QString, QGraphicsItem *> m_targetItems;

    // 当前在画布上高亮(变绿)的目标点，随列表选中行同步
    QGraphicsItem *m_selectedTargetItem = nullptr;

    // 吸附指示圈(绘制任务点时显示最近的路网吸附位置)
    QGraphicsEllipseItem *m_snapIndicator = nullptr;
};

#endif // TASKEDITWIDGET_H
