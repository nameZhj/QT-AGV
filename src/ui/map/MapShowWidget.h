#pragma once

#include <QWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QPixmap>

#include <QSet>
#include "MapCanvas.h"

// 对应 map/show.vue - 地图导航页面
// 包含地图显示、设备位置实时更新(MQTT)、任务加载、初始化位姿
class MapShowWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MapShowWidget(const QString &mapId, QWidget *parent = nullptr);
    ~MapShowWidget();

signals:
    void goBack();

private slots:
    void onDrawStatusChanged(int status);
    void onLoadTask();
    void onMqttMessage(const QString &topic, const QByteArray &payload);
    void onCarSelected(int index);

private:
    void setupUI();
    void loadMapData();
    void loadRouteData();
    void loadCarList();
    void updateCarList();
    void loadCurrentTask();
    void updateTaskButtons();
    void renderMap();
    void updateCarPosition(const QString &carId, double x, double y, double angle);
    void initMqtt();
    void disconnectMqtt();

    bool eventFilter(QObject *obj, QEvent *event) override;

    QPointF worldToScene(double wx, double wy) const;
    QPointF sceneToWorld(double sx, double sy) const;

    // 调试辅助
    void renderDebugMarkers();
    void updateDebugLabel();

    enum DrawStatus { Drag = 0, Pose = 1 };

    // UI
    MapCanvas *m_mapView = nullptr;
    QGraphicsScene *m_scene = nullptr;
    QComboBox *m_drawStatusCombo = nullptr;
    QPushButton *m_loadTaskBtn = nullptr;
    QLabel *m_coordLabel = nullptr;
    QLabel *m_mapInfoLabel = nullptr;
    QComboBox *m_carCombo = nullptr;
    QLabel *m_carNameLabel = nullptr;
    QLabel *m_batteryLabel = nullptr;
    QLabel *m_covarianceLabel = nullptr;
    QLabel *m_speedLabel = nullptr;
    QLabel *m_xLabel = nullptr;
    QLabel *m_yLabel = nullptr;

    // 当前任务信息
    QLabel *m_taskNameLabel = nullptr;
    QLabel *m_taskStatusLabel = nullptr;
    QPushButton *m_taskStartBtn = nullptr;
    QPushButton *m_taskPauseBtn = nullptr;
    QPushButton *m_taskContinueBtn = nullptr;
    QPushButton *m_taskStopBtn = nullptr;
    QPushButton *m_taskRemoveBtn = nullptr;
    QJsonObject m_currentTask;

    // 数据
    QString m_mapId;
    QString m_mapName;
    QJsonObject m_mapConfig;
    QImage m_mapImage;
    QJsonArray m_points;
    QJsonArray m_lines;
    QJsonArray m_curves;
    QJsonArray m_carList;

    // 坐标转换参数 (与 Vue show.vue 完全一致)
    // Vue: extent = [-offsetX, -offsetY, mapWidth-offsetX, mapHeight-offsetY]
    double m_worldL = 0, m_worldB = 0, m_worldR = 0, m_worldT = 0;
    double m_imgW = 0, m_imgH = 0;
    bool m_hasInitialFit = false;

    // 调试: 原始API参数 (用于诊断漂移)
    double m_offsetX = 0, m_offsetY = 0;
    double m_mapWidth = 0, m_mapHeight = 0;
    QString m_tilemapPath;
    QLabel *m_debugLabel = nullptr;

    DrawStatus m_drawStatus = Drag;

    // 设备位置图元
    struct CarInfo {
        QString carId;
        QString carName;
        double x = 0;
        double y = 0;
        double angle = 0;
        double speed = 0;
        double battery = -1;
        double covarianceScore = -1;
        QString status;
        QString taskId;
        bool online = false;
        QGraphicsItem *marker = nullptr;
    };
    QMap<QString, CarInfo> m_carInfos;
    QSet<QString> m_onlineCarIds;

    QPixmap m_robotPix;
};

