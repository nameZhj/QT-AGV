#ifndef MAPEDITWIDGET_H
#define MAPEDITWIDGET_H

#include <QWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsEllipseItem>
#include <QGraphicsPolygonItem>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QTableWidget>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
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

signals:
    void goBack();

private slots:
    void onSaveRoute();
    void onEditMap();
    void onTableToggle();
    void onDeleteFeature();

private:
    void setupUI();
    void loadMapData();
    void loadRouteData();
    void renderMap();
    void fitView();

    void addPoint(const QPointF &sp);
    void addCharge(const QPointF &sp);
    void addHighPrecisionPoint(const QPointF &sp);
    void addLine(const QPointF &a, const QPointF &b,
                 const QString &startId = {}, const QString &endId = {});
    void addCurve(const QPointF &a, const QPointF &b,
                  const QString &startId = {}, const QString &endId = {});

    void updateCurveShape(const QString &id);            // update path item in-place
    void rebuildCurveHandles(const QString &id);         // sync handle items to JSON
    void removeAllHandles();                              // clears m_curveHandles from scene

    // Snap helpers
    struct SnapResult {
        bool    valid    = false;
        QString type, id;
        QPointF scenePos, worldPos;
    };
    SnapResult findSnap(const QPointF &sp, double pixelTol = 15.0) const;
    bool       hasOverlap(const QPointF &sp) const;   // 0.3 m world check
    void       updateSnapIndicator(const SnapResult &r);

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

    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent *e) override;

    enum DrawStatus { Drag=0, Point=1, Line=2, Curve=3, Charge=4, HighPrecision=5 };

    // Map view
    QGraphicsView  *m_mapView      = nullptr;
    QGraphicsScene *m_scene        = nullptr;
    QLabel         *m_coordLabel   = nullptr;
    QLabel         *m_mapInfoLabel = nullptr;

    // Toolbar: mode → button
    QMap<int, QPushButton*> m_toolBtns;
    QPushButton *m_saveBtn      = nullptr;
    QPushButton *m_editMapBtn   = nullptr;
    QPushButton *m_tableModeBtn = nullptr;
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

    // Properties panel
    QWidget   *m_propsPanel      = nullptr;
    QLabel    *m_propTypeLabel   = nullptr;
    QLineEdit *m_propIdEdit      = nullptr;
    QLineEdit *m_propNameEdit    = nullptr;
    QWidget   *m_propDirectRow   = nullptr;
    QComboBox *m_propDirectCombo = nullptr;

    // Data
    QString    m_mapId, m_mapName;
    QImage     m_mapImage;
    QJsonArray m_points, m_lines, m_curves, m_charges, m_highPrecisionPoints;

    // Coordinate transform (world extent + thumbnail size)
    double m_worldL = 0, m_worldB = 0, m_worldR = 0, m_worldT = 0;
    double m_imgW   = 0, m_imgH   = 0;

    // Draw state
    DrawStatus m_drawStatus    = Drag;
    bool       m_hasInitialFit = false;
    bool       m_tableVisible  = false;
    QString    m_currentTable  = "point";
    QPointF    m_drawP1;
    QString    m_drawP1Id;               // snap ID of first draw point
    bool       m_drawHasP1     = false;

    // Preview rubber-band
    QGraphicsItem        *m_previewItem    = nullptr;
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

    // MQTT
    QJsonArray   m_carList;
    QSet<QString> m_onlineCarIds;
    QString      m_currentCarId;

    // 实时设备位置标记（开关控制显示）
    bool                   m_showRobotPosition = false;
    QGraphicsPolygonItem  *m_robotItem         = nullptr;
    double                 m_carX = 0, m_carY = 0, m_carYaw = 0;
    bool                   m_hasCarPose        = false;
};

#endif // MAPEDITWIDGET_H
