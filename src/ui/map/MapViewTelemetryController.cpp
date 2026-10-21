/**
 * @file    MapViewTelemetryController.cpp
 * @layer   GUI
 * @brief   Telemetry and MQTT subscription synchronization implementation.
 * @deps    MapViewTelemetryController.h, MqttService.h, AppConfig.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapViewTelemetryController.h"
#include "network/MqttService.h"
#include "app/AppConfig.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

MapViewTelemetryController::MapViewTelemetryController(QObject *parent)
    : QObject(parent)
{
    connect(&RobotInfoManager::instance(), &RobotInfoManager::currentRobotInfoUpdated,
            this, &MapViewTelemetryController::onRobotInfo);
    onRobotInfo(RobotInfoManager::instance().currentInfo());
}

MapViewTelemetryController::~MapViewTelemetryController()
{
    disconnectMqtt(m_activeCarId);
}

void MapViewTelemetryController::initMqtt(const QString &carId)
{
    QString serverIp = AppConfig::instance().serverIp();
    if (serverIp.isEmpty()) return;

    if (!MqttService::instance().isConnected()) {
        MqttService::instance().connectToHost(serverIp, 1883);
    }

    m_activeCarId = carId.isEmpty() ? AppConfig::instance().agvId() : carId;
    if (m_activeCarId.isEmpty()) m_activeCarId = "1";
    m_robot.carId = m_activeCarId;

    RobotInfoManager::instance().subscribeGlobalTopics();
}

void MapViewTelemetryController::disconnectMqtt(const QString &)
{
    // 全局核心话题受 MqttService 与 RobotInfoManager 托管守护，子页面无需亦不可退订
}

void MapViewTelemetryController::onMqttPayload(const QString &topic, const QByteArray &payload)
{
    QByteArray cleanPayload = payload.trimmed();
    bool inQuote = false, escape = false;
    for (int i = 0; i < cleanPayload.size(); ++i) {
        char c = cleanPayload.at(i);
        if (escape) { escape = false; continue; }
        if (c == '\\') { if (inQuote) escape = true; continue; }
        if (c == '"') { inQuote = !inQuote; continue; }
        if (!inQuote) {
            if (c == '(') cleanPayload[i] = '{';
            else if (c == ')') cleanPayload[i] = '}';
        }
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(cleanPayload, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return;
    if (!topic.contains("info")) return;

    QJsonObject d = doc.object();
    QString msgCarId;
    if (d.contains("car_id")) msgCarId = d.value("car_id").toVariant().toString().trimmed();
    else if (d.contains("carId")) msgCarId = d.value("carId").toVariant().toString().trimmed();
    else if (topic.startsWith("/rob/")) {
        QStringList parts = topic.split('/', Qt::SkipEmptyParts);
        if (parts.size() >= 2) msgCarId = parts[1].trimmed();
    }
    QString curCarId = m_robot.carId.trimmed().isEmpty() ? AppConfig::instance().agvId().trimmed() : m_robot.carId.trimmed();
    if (curCarId.isEmpty()) curCarId = "1";
    if (!msgCarId.isEmpty() && msgCarId != curCarId) return;

    RobotInfoManager::instance().updateFromPayload(topic, cleanPayload);
}

void MapViewTelemetryController::onRobotInfo(const RobotInfo &info)
{
    if (info.hasPose) {
        m_robot.wx = info.car_x;
        m_robot.wy = info.car_y;
        m_robot.yawRad = info.car_yaw;
        m_robot.posValid = true;
    }
    if (info.battery >= 0.0)      m_robot.battery = info.batteryPercent();
    if (info.actual_vel.valid)    m_robot.speed = info.actualLinearSpeed();
    if (info.covScore >= 0.0)     m_robot.covScore = info.covScore;
    if (!info.carName.isEmpty())  m_robot.carName = info.carName;
    if (info.latency >= 0)        m_robot.latency = info.latency;

    emit telemetryUpdated(m_robot, info);
}
