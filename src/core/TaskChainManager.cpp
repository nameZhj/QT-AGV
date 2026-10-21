/**
 * @file    TaskChainManager.cpp
 * @layer   CoreTopology
 * @brief   Implementation of task chain dispatching and sequential execution state machine.
 * @deps    TaskChainManager.h, TaskChainSerializer.h, MqttService.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "TaskChainManager.h"
#include "TaskChainSerializer.h"
#include "network/MqttService.h"

#include <QJsonDocument>
#include <QDebug>

// ==============================================================================
// === [Section: Lifecycle & Initialization] ===
// ==============================================================================

TaskChainManager &TaskChainManager::instance()
{
    static TaskChainManager s_instance;
    return s_instance;
}

TaskChainManager::TaskChainManager(QObject *parent)
    : QObject(parent)
{
}

void TaskChainManager::init()
{
    if (m_initialized) return;
    m_initialized = true;

    auto &mqtt = MqttService::instance();
    connect(&mqtt, &MqttService::connected, this, &TaskChainManager::onMqttConnected);
    if (mqtt.isConnected()) {
        onMqttConnected();
    }
}

// ==============================================================================
// === [Section: MQTT Handlers & Topic Subscriptions] ===
// ==============================================================================

void TaskChainManager::onMqttConnected()
{
    auto &mqtt = MqttService::instance();

    mqtt.subscribeTopic(QStringLiteral("/taskchain/status"), 0,
        [this](const QString &, const QByteArray &payload) {
            handleStatusPayload(payload);
        });

    mqtt.subscribeTopic(QStringLiteral("/taskchain/currentTask"), 0,
        [this](const QString &, const QByteArray &payload) {
            handleCurrentTaskPayload(payload);
        });

    qDebug() << "[TaskChainManager] Global MQTT topics subscribed: /taskchain/status, /taskchain/currentTask";
}

void TaskChainManager::handleStatusPayload(const QByteArray &payload)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return;

    const QJsonObject data = doc.object();
    const bool working = data.value(QStringLiteral("robotWorking")).toBool(false);
    const bool paused  = data.value(QStringLiteral("robotPause")).toBool(false);

    {
        QWriteLocker locker(&m_lock);
        m_hasStatus  = true;
        m_isWorking  = working;
        m_isPaused   = paused;
        if (!working) {
            m_localExecutionActive = false;
            m_currentTask.clear();
            m_currentGroup.clear();
        }
    }

    emit chainStatusChanged(working, paused);
}

void TaskChainManager::handleCurrentTaskPayload(const QByteArray &payload)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return;

    const QJsonObject data = doc.object();
    const QString task  = data.value(QStringLiteral("currentTask")).toString();
    const QString group = data.value(QStringLiteral("currentGroup")).toString();

    {
        QWriteLocker locker(&m_lock);
        m_currentTask  = task;
        m_currentGroup = group;
    }

    emit currentTaskChanged(task, group);
}

// ==============================================================================
// === [Section: State Accessors & Mutators] ===
// ==============================================================================

bool TaskChainManager::isWorking() const
{
    QReadLocker locker(&m_lock);
    return m_isWorking || m_localExecutionActive;
}

bool TaskChainManager::isPaused() const
{
    QReadLocker locker(&m_lock);
    return m_isPaused;
}

QString TaskChainManager::currentTask() const
{
    QReadLocker locker(&m_lock);
    return m_currentTask;
}

QString TaskChainManager::currentGroup() const
{
    QReadLocker locker(&m_lock);
    return m_currentGroup;
}

bool TaskChainManager::hasStatus() const
{
    QReadLocker locker(&m_lock);
    return m_hasStatus;
}

bool TaskChainManager::isLocalExecutionActive() const
{
    QReadLocker locker(&m_lock);
    return m_localExecutionActive;
}

void TaskChainManager::setLocalExecutionActive(bool active)
{
    {
        QWriteLocker locker(&m_lock);
        m_localExecutionActive = active;
        if (active) m_isWorking = true;
    }
    emit chainStatusChanged(isWorking(), isPaused());
}

void TaskChainManager::resetExecutionState()
{
    {
        QWriteLocker locker(&m_lock);
        m_isWorking = false;
        m_isPaused  = false;
        m_localExecutionActive = false;
        m_currentTask.clear();
        m_currentGroup.clear();
    }
    emit chainStatusChanged(false, false);
    emit currentTaskChanged(QString(), QString());
}

// ==============================================================================
// === [Section: Action & Protocol Conversion Delegates] ===
// ==============================================================================

QString TaskChainManager::actionTypeFromName(const QString &name)
{
    return TaskChainSerializer::actionTypeFromName(name);
}

QString TaskChainManager::actionDescFromType(const QString &type)
{
    return TaskChainSerializer::actionDescFromType(type);
}

QString TaskChainManager::normalizeActionType(const QString &type, const QString &name)
{
    return TaskChainSerializer::normalizeActionType(type, name);
}

// ==============================================================================
// === [Section: Chain Serialization Delegates] ===
// ==============================================================================

QJsonObject TaskChainManager::exportChainToJson(const TaskChainData &chain,
                                                 bool onlyChecked,
                                                 bool forExecution,
                                                 const QList<QPair<QString, QString>> &stations)
{
    return TaskChainSerializer::exportChainToJson(chain, onlyChecked, forExecution, stations);
}

TaskChainData TaskChainManager::importChainFromJson(const QJsonObject &json)
{
    return TaskChainSerializer::importChainFromJson(json);
}
