#ifndef TASKWIDGET_H
#define TASKWIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QToolButton>
#include <QLabel>
#include <QComboBox>
#include <QJsonObject>
#include <QJsonArray>
#include "MapCanvas.h"
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QSet>
#include <QTimer>
#include <QPixmap>
#include <QResizeEvent>
#include <QShowEvent>

class RulerWidget;
class LoadingDialog;

// 对应 task/index.vue - 任务管理
// 左侧展示地图（当前选中任务的地图），右侧任务列表
// 地图上方横向放任务执行导航相关功能
class TaskWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TaskWidget(QWidget *parent = nullptr);
    void refreshList();

signals:
    void navigateRequested(const QString &target, const QString &id);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent *e) override;
    void showEvent(QShowEvent *e) override;

private slots:
    void onAdd();
    void onEdit();
    void onDelete();
    void onRefresh();
    void onTableSelectionChanged();
    void onTaskSelectionChanged();
    void onMqttMessage(const QString &topic, const QByteArray &payload);
    void onWebSocketNotification(const QJsonObject &msg);

    // 任务执行控制
    void onLoadTask();
    void onStartTask();
    void onPauseTask();
    void onContinueTask();
    void onStopTask();
    void onRemoveTask();

private:
    void setupUI();
    void getList();
    void loadTaskValidity();
    void applyTaskValidityStyle();
    void doLoadTaskRequest(LoadingDialog *loading, bool allowRetry = false);
    void loadMapForTask(const QString &mapId);
    void loadCurrentCarMap();
    void loadRouteData(const QString &mapId);
    void renderMap();
    void renderAxesAndGrid();
    void updateRulers();
    void fitView();
    void updateCoordinateDisplay(const QPointF &scenePos);
    void updateCarPosition(const QString &carId, double x, double y, double angle);
    void loadTaskTargets(const QString &taskId);
    void sendNavCommand(const QString &command, const QString &loadingText,
                        const QString &successText, const QString &failText);
    QPointF worldToScene(double wx, double wy) const;
    QPointF sceneToWorld(double sx, double sy) const;
    void initMqtt();
    void disconnectMqtt();
    void loadCurrentTask();
    void updateTaskButtons();
    void selectFirstTask();
    void startTaskPolling();    // 启动1Hz轮询任务状态(执行中时)
    void stopTaskPolling();     // 停止轮询
    void onTaskPollTimeout();   // 轮询定时器超时回调

    // UI - 地图区域 (与 MapViewWidget 一致的标尺+网格布局)
    MapCanvas      *m_mapView      = nullptr;
    QGraphicsScene *m_scene = nullptr;
    QLabel *m_mapInfoLabel = nullptr;
    QLabel *m_coordLabel = nullptr;
    RulerWidget *m_topRuler = nullptr;
    RulerWidget *m_leftRuler = nullptr;
    QToolButton *m_gridBtn = nullptr;
    bool m_showGrid = false;
    bool m_hasInitialFit = false;
    QPixmap m_robotPixmap;

    // UI - 任务执行导航按钮
    QToolButton *m_loadTaskBtn = nullptr;
    QToolButton *m_startTaskBtn = nullptr;
    QToolButton *m_pauseTaskBtn = nullptr;
    QToolButton *m_continueTaskBtn = nullptr;
    QToolButton *m_stopTaskBtn = nullptr;
    QToolButton *m_removeTaskBtn = nullptr;
    QToolButton *m_resetViewBtn = nullptr;
    QLabel *m_currentTaskLabel = nullptr;

    // UI - 任务列表
    QTableWidget *m_table = nullptr;
    QPushButton *m_addBtn = nullptr;
    QPushButton *m_editBtn = nullptr;
    QPushButton *m_deleteBtn = nullptr;
    QPushButton *m_refreshBtn = nullptr;

    // 数据
    QJsonArray m_taskList;
    QSet<QString> m_invalidTaskIds;   // 服务器标记为"已失效"的任务 (与 Vue 下拉框禁用逻辑一致)
    int m_pageNum = 1;
    int m_pageSize = 10;
    int m_total = 0;
    QString m_lastSelectedTaskId;      // 刷新前列表选中的 taskId，刷新后恢复选中
    QTimer *m_refreshDebounce = nullptr;  // 合并频繁的 WebSocket 触发的刷新请求
    bool m_suppressTaskLoad = false;      // 刷新列表恢复选中时，不触发 loadCurrentTask

    // 地图数据
    QString m_currentMapId;
    QString m_currentMapName;
    QJsonObject m_mapConfig;
    QImage m_mapImage;
    QJsonArray m_points;
    QJsonArray m_lines;
    QJsonArray m_curves;
    QJsonArray m_charges;
    QJsonArray m_highPrecisionPoints;
    QJsonArray m_targets;

    // 坐标转换参数 (world extent + thumbnail size)
    double m_worldL = 0, m_worldB = 0, m_worldR = 0, m_worldT = 0;
    double m_imgW   = 0, m_imgH   = 0;

    // 设备位置
    struct CarInfo {
        QString carId;
        QString carName;
        double x = 0;
        double y = 0;
        double angle = 0;
        double speed = 0;
        double battery = -1;
        bool online = false;
        QGraphicsItem *marker = nullptr;
        QGraphicsItem *nameLabel = nullptr;
    };
    QMap<QString, CarInfo> m_carInfos;
    QSet<QString> m_onlineCarIds;

    // 当前任务
    QJsonObject m_currentTask;
    QString m_currentTaskId;
    QString m_currentCarId;

    // 任务状态轮询定时器: 任务执行中以1Hz频率轮询API，直到暂停或结束
    QTimer *m_taskPollTimer = nullptr;
    bool m_taskPolling = false;       // 轮询是否活跃(防止重入)
};

#endif // TASKWIDGET_H
