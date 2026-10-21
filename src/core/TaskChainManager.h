/**
 * @file    TaskChainManager.h
 * @module  CoreTopology
 * @brief   Mission dispatching manager and sequential task execution state machine.
 * @deps    QObject, ApiService, RobotInfoManager
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QList>
#include <QPair>
#include <QReadWriteLock>

struct TaskChainData {
    QString name;
    QJsonArray tasks;       // 任务列表, 每个任务含 name/note/checked/groups
};

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
 * 4. 提供任务链纯数据序列化与动作类型归一化解析算法 (解耦 UI 渲染与报文组装)。
 */
class TaskChainManager : public QObject
{
    Q_OBJECT

// ==============================================================================
// === [Section: Public API & State Queries] ===
// ==============================================================================
public:
    static constexpr double kMaxLiftLinearSpeed = 0.1;
    static constexpr double kMaxLiftHeight = 0.15;

    static TaskChainManager &instance();

    void init();

    // --- Action & Protocol Conversion Helpers ---
    static QString actionTypeFromName(const QString &name);
    static QString actionDescFromType(const QString &type);
    static QString normalizeActionType(const QString &type, const QString &name);

    // --- Chain Serialization & Deserialization ---
    static QJsonObject exportChainToJson(const TaskChainData &chain,
                                         bool onlyChecked = false,
                                         bool forExecution = false,
                                         const QList<QPair<QString, QString>> &stations = {});
    static TaskChainData importChainFromJson(const QJsonObject &json);

    // --- State Queries ---
    bool isWorking() const;
    bool isPaused() const;
    QString currentTask() const;
    QString currentGroup() const;
    bool hasStatus() const;
    bool isLocalExecutionActive() const;

    // --- State Mutations ---
    void setLocalExecutionActive(bool active);
    void resetExecutionState();

// ==============================================================================
// === [Section: Signals & Slots] ===
// ==============================================================================
signals:
    void chainStatusChanged(bool working, bool paused);
    void currentTaskChanged(const QString &task, const QString &group);

private slots:
    void onMqttConnected();

// ==============================================================================
// === [Section: Private State & Payload Handlers] ===
// ==============================================================================
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

