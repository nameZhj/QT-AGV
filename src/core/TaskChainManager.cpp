#include "TaskChainManager.h"
#include "MqttService.h"
#include <QJsonDocument>
#include <QDebug>

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

void TaskChainManager::onMqttConnected()
{
    auto &mqtt = MqttService::instance();

    // 全局订阅任务链运行状态: {"robotWorking":true,"robotPause":false}
    mqtt.subscribeTopic(QStringLiteral("/taskchain/status"), 0,
        [this](const QString &, const QByteArray &payload) {
            handleStatusPayload(payload);
        });

    // 全局订阅任务链执行进度: {"currentTask":"sub_task_1","currentGroup":"group_2"}
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
    bool working = data.value("robotWorking").toBool(false);
    bool paused  = data.value("robotPause").toBool(false);

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
    QString task  = data.value("currentTask").toString();
    QString group = data.value("currentGroup").toString();

    {
        QWriteLocker locker(&m_lock);
        m_currentTask  = task;
        m_currentGroup = group;
    }

    emit currentTaskChanged(task, group);
}

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
        if (active) {
            m_isWorking = true;
        }
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
