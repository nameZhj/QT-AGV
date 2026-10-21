/**
 * @file    RobotInfoManager.cpp
 * @layer   CoreScheduling
 * @brief   Global robot telemetry data bus, MQTT subscription, and thread-safe registry.
 * @deps    RobotInfoManager.h, RobotInfoParser.h, MqttService.h, AppConfig.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */
#include "RobotInfoManager.h"
#include "RobotInfoParser.h"
#include "MqttService.h"
#include "AppConfig.h"
#include <QJsonDocument>
#include <QRegularExpression>
#include <QDebug>

RobotInfoManager &RobotInfoManager::instance()
{
    static RobotInfoManager s_instance;
    return s_instance;
}

RobotInfoManager::RobotInfoManager(QObject *parent)
    : QObject(parent)
{
    // 刚开机默认初始化状态为“就绪” (status: "0")，但未接收到有效数据时 receivedData 为 false
    RobotInfo def;
    def.carIdStr = QStringLiteral("1");
    def.car_id = 1;
    def.status = QStringLiteral("0");
    def.receivedData = false;
    def.latency = -1;
    m_robots[QStringLiteral("1")] = def;
}

void RobotInfoManager::init()
{
    if (m_initialized) return;
    m_initialized = true;

    auto &mqtt = MqttService::instance();
    connect(&mqtt, &MqttService::connected, this, &RobotInfoManager::onMqttConnected);
    connect(&mqtt, &MqttService::disconnected, this, &RobotInfoManager::onMqttDisconnected);

    // 无论当前 MQTT 是否已连接完成，立刻全局注册核心订阅
    subscribeGlobalTopics();
}

void RobotInfoManager::subscribeGlobalTopics()
{
    auto &mqtt = MqttService::instance();
    auto handlePayload = [this](const QString &topic, const QByteArray &payload) {
        updateFromPayload(topic, payload);
    };

    const QString curCarId = AppConfig::instance().agvId().trimmed().isEmpty()
                             ? QStringLiteral("1")
                             : AppConfig::instance().agvId().trimmed();

    mqtt.setTopicSubscription(QStringLiteral("/rob/1/info"), 0, handlePayload);
    if (curCarId != QStringLiteral("1")) {
        mqtt.setTopicSubscription(QStringLiteral("/rob/%1/info").arg(curCarId), 0, handlePayload);
    }
    mqtt.setTopicSubscription(QStringLiteral("/rob/+/info"), 0, handlePayload);
    mqtt.setTopicSubscription(QStringLiteral("/rob/info"), 0, handlePayload);
    mqtt.setTopicSubscription(QStringLiteral("/info"), 0, handlePayload);
    mqtt.setTopicSubscription(QStringLiteral("info"), 0, handlePayload);

    // 订阅车辆状态MQTT消息：/rob/navigate/status
    mqtt.setTopicSubscription(QStringLiteral("/rob/navigate/status"), 0, [this](const QString &, const QByteArray &payload) {
        updateNavigateStatus(payload);
    });
    mqtt.setTopicSubscription(QStringLiteral("/rob/+/navigate/status"), 0, [this](const QString &, const QByteArray &payload) {
        updateNavigateStatus(payload);
    });
}

void RobotInfoManager::onMqttConnected()
{
    subscribeGlobalTopics();
}

void RobotInfoManager::onMqttDisconnected()
{
    QWriteLocker locker(&m_lock);
    for (auto &r : m_robots) {
        r.receivedData = false;
        r.latency = -1;
    }
}

void RobotInfoManager::resetForCar(const QString &newCarId)
{
    {
        QWriteLocker locker(&m_lock);
        m_robots.clear(); // 清理旧车全部数据与缓存
        RobotInfo def;
        def.carIdStr = QStringLiteral("1");
        def.car_id = 1;
        def.status = QStringLiteral("0"); // 重置初始化状态为就绪
        def.receivedData = false;
        def.latency = -1;
        m_robots[QStringLiteral("1")] = def;
        if (!newCarId.isEmpty() && newCarId != QStringLiteral("1")) {
            def.carIdStr = newCarId;
            def.car_id = newCarId.toInt();
            m_robots[newCarId] = def;
        }
    }

    subscribeGlobalTopics();
}

RobotInfo RobotInfoManager::currentInfo() const
{
    QReadLocker locker(&m_lock);
    // 用户明确规定：/rob/{car_id}/info 中 car_id 永远是常数 1，优先返回 ID 为 1 的车辆信息
    if (m_robots.contains(QStringLiteral("1"))) {
        return m_robots.value(QStringLiteral("1"));
    }
    QString curId = AppConfig::instance().agvId().trimmed();
    if (curId.isEmpty()) {
        curId = AppConfig::instance().currentDevice().carId.trimmed();
    }
    if (!curId.isEmpty() && m_robots.contains(curId)) {
        return m_robots.value(curId);
    }
    if (!m_robots.isEmpty()) {
        return m_robots.constBegin().value();
    }
    // 返回初始化空对象（默认 car_id 为 1）
    RobotInfo def;
    def.carIdStr = QStringLiteral("1");
    def.car_id = 1;
    return def;
}

RobotInfo RobotInfoManager::infoForCar(const QString &carId) const
{
    QReadLocker locker(&m_lock);
    QString key = carId.trimmed();
    if (m_robots.contains(key)) {
        return m_robots.value(key);
    }
    // /rob/{car_id}/info 中 car_id 永远是常数 1，兜底回退到 1
    if (m_robots.contains(QStringLiteral("1"))) {
        return m_robots.value(QStringLiteral("1"));
    }
    RobotInfo def;
    def.carIdStr = key.isEmpty() ? QStringLiteral("1") : key;
    bool ok = false;
    int cid = def.carIdStr.toInt(&ok);
    def.car_id = ok ? cid : 1;
    return def;
}

QMap<QString, RobotInfo> RobotInfoManager::allRobotInfos() const
{
    QReadLocker locker(&m_lock);
    return m_robots;
}

bool RobotInfoManager::hasInfoForCar(const QString &carId) const
{
    QReadLocker locker(&m_lock);
    return m_robots.contains(carId.trimmed());
}

bool RobotInfoManager::updateNavigateStatus(const QByteArray &payload)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    QJsonObject obj = doc.object();
    if (!obj.contains(QStringLiteral("status"))) {
        return false;
    }

    QString statusVal;
    QJsonValue sv = obj.value(QStringLiteral("status"));
    if (sv.isString()) {
        statusVal = sv.toString().trimmed();
    } else if (sv.isDouble()) {
        statusVal = QString::number(sv.toInt());
    }

    if (statusVal.isEmpty()) {
        return false;
    }

    const QString carId = QStringLiteral("1");
    RobotInfo snapshot;
    {
        QWriteLocker locker(&m_lock);
        RobotInfo &target = m_robots[carId];
        target.carIdStr = carId;
        target.car_id = 1;
        target.status = statusVal; // "0" 就绪, "1" 导航中, "2" 暂停导航
        target.receivedData = true;
        snapshot = target;

        QString curId = AppConfig::instance().agvId().trimmed();
        if (curId.isEmpty()) curId = AppConfig::instance().currentDevice().carId.trimmed();
        if (!curId.isEmpty() && curId != carId) {
            m_robots[curId] = snapshot;
        }
    }

    qDebug().noquote() << QStringLiteral("[RobotInfoManager] 收到 /rob/navigate/status -> status:%1 (%2)")
                              .arg(statusVal, (statusVal == "0" ? "就绪" : (statusVal == "1" ? "导航中" : (statusVal == "2" ? "暂停导航" : statusVal))));

    emit robotInfoUpdated(carId, snapshot);
    emit currentRobotInfoUpdated(snapshot);
    return true;
}

bool RobotInfoManager::updateFromPayload(const QString &topic, const QByteArray &payload, QString *outCarId)
{
    if (topic.endsWith(QStringLiteral("/navigate/status"))) {
        if (outCarId) *outCarId = QStringLiteral("1");
        return updateNavigateStatus(payload);
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    QJsonObject d = doc.object();

    // 兼容外层包装 data 对象的结构: { code: 200, data: { ... } }
    QJsonObject source = d;
    if (d.contains(QStringLiteral("data")) && d.value(QStringLiteral("data")).isObject()) {
        QJsonObject dataObj = d.value(QStringLiteral("data")).toObject();
        for (auto it = dataObj.constBegin(); it != dataObj.constEnd(); ++it) {
            source.insert(it.key(), it.value());
        }
    }

    // 检查是否为合法的机器人设备状态报文（防止任务链/任务结构等非状态消息污染主车状态）
    bool isRobotInfoPayload = source.contains(QStringLiteral("car_id")) || source.contains(QStringLiteral("carId"))
                           || source.contains(QStringLiteral("car_x"))  || source.contains(QStringLiteral("x"))
                           || source.contains(QStringLiteral("status")) || source.contains(QStringLiteral("battery"))
                           || source.contains(QStringLiteral("power"))  || source.contains(QStringLiteral("map_id"));
    if (!isRobotInfoPayload) {
        return false;
    }

    // 用户明确规定：/rob/{car_id}/info 中 car_id 永远是常数 1
    const QString carId = QStringLiteral("1");

    if (outCarId) {
        *outCarId = carId;
    }

    RobotInfo snapshot;
    {
        QWriteLocker locker(&m_lock);
        RobotInfo &target = m_robots[carId];
        target.carIdStr = carId;
        target.car_id = 1;

        parseInfoJson(source, &target);
        target.car_id = 1; // 强制常数 1
        target.carIdStr = carId;
        snapshot = target;

        // 若当前配置有指定设备ID，同步写入镜像以兼容多处旧接口查询
        QString curId = AppConfig::instance().agvId().trimmed();
        if (curId.isEmpty()) curId = AppConfig::instance().currentDevice().carId.trimmed();
        if (!curId.isEmpty() && curId != carId) {
            m_robots[curId] = snapshot;
        }
    }

    // 派发信号
    emit robotInfoUpdated(carId, snapshot);
    emit currentRobotInfoUpdated(snapshot);

    return true;
}

bool RobotInfoManager::parseInfoJson(const QJsonObject &d, RobotInfo *outInfo)
{
    return RobotInfoParser::parseInfoJson(d, outInfo);
}

QString RobotInfoManager::formatDuration(qint64 totalSeconds)
{
    return RobotInfoParser::formatDuration(totalSeconds);
}
