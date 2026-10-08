#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QReadWriteLock>

/**
 * @brief 全局任务链状态与进度管理器 (单例)
 * 
 * 核心功能:
 * 1. 在整个应用程序生命周期中全局自动订阅并监听 MQTT:
 *    - /taskchain/status: {"robotWorking": bool, "robotPause": bool}
 *    - /taskchain/currentTask: {"currentTask": string, "currentGroup": string}
 * 2. 全局持久缓存任务链的运行状态与当前执行进度，彻底解决在界面切换
 *    (如切到地图、配置等页面)导致状态丢失、任务状态判断错误的问题。
 * 3. 向所有 UI 界面提供信号广播与即时状态读取接口。
 */
class TaskChainManager : public QObject
{
    Q_OBJECT

public:
    static TaskChainManager &instance();

    void init();

    // --- 状态读取接口 ---
    bool isWorking() const;
    bool isPaused() const;
    QString currentTask() const;
    QString currentGroup() const;
    bool hasStatus() const;
    bool isLocalExecutionActive() const;

    // --- 主动状态控制与同步 ---
    void setLocalExecutionActive(bool active);
    void resetExecutionState();

signals:
    void chainStatusChanged(bool working, bool paused);
    void currentTaskChanged(const QString &task, const QString &group);

private slots:
    void onMqttConnected();

private:
    explicit TaskChainManager(QObject *parent = nullptr);
    ~TaskChainManager() override = default;
    Q_DISABLE_COPY(TaskChainManager)

    void handleStatusPayload(const QByteArray &payload);
    void handleCurrentTaskPayload(const QByteArray &payload);

    mutable QReadWriteLock m_lock;
    bool    m_initialized = false;
    bool    m_isWorking = false;
    bool    m_isPaused = false;
    QString m_currentTask;
    QString m_currentGroup;
    bool    m_hasStatus = false;
    bool    m_localExecutionActive = false;
};
