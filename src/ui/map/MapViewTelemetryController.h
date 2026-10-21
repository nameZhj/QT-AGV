/**
 * @file    MapViewTelemetryController.h
 * @layer   GUI
 * @brief   Telemetry, MQTT subscription, and robot runtime metrics synchronization controller.
 * @deps    QObject, RobotInfoManager.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QObject>
#include <QString>
#include "core/RobotInfoManager.h"

struct MapViewRobotState {
    QString carId = "1", carName;
    double wx = 0, wy = 0, yawRad = 0;
    double speed = 0, battery = -1, covScore = -1;
    int latency = -1;
    bool posValid = false;
};

class MapViewTelemetryController : public QObject
{
    Q_OBJECT
public:
    explicit MapViewTelemetryController(QObject *parent = nullptr);
    ~MapViewTelemetryController() override;

    void initMqtt(const QString &carId);
    void disconnectMqtt(const QString &carId);

    const MapViewRobotState& robotState() const { return m_robot; }
    void setCarName(const QString &name) { m_robot.carName = name; }
    void setCarId(const QString &id) { m_robot.carId = id; }

public slots:
    void onRobotInfo(const RobotInfo &info);

signals:
    void telemetryUpdated(const MapViewRobotState &state, const RobotInfo &info);

private slots:
    void onMqttPayload(const QString &topic, const QByteArray &payload);

private:
    MapViewRobotState m_robot;
    QString           m_activeCarId;
};
