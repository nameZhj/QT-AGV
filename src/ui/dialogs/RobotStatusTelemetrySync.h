/**
 * @file    RobotStatusTelemetrySync.h
 * @layer   GUILayer
 * @brief   Telemetry, MQTT payload parsing, and RobotInfo mapper for RobotStatusDialog.
 * @deps    RobotStatusDialog.h, RobotInfoManager.h
 * @note    Refactored for AI readability; runtime behavior & telemetry mappings invariant.
 */

#pragma once

#include <QString>
#include <QByteArray>

class RobotStatusDialog;
struct RobotInfo;

class RobotStatusTelemetrySync
{
public:
    static void updateFromRobotInfo(RobotStatusDialog *dlg, const RobotInfo &info);
    static void updateFromRobotState(RobotStatusDialog *dlg, const QString &carId,
                                    double x, double y, double yawRad,
                                    double speed, double battery, double covScore, bool posValid);
    static void onPeriodicRefresh(RobotStatusDialog *dlg);
    static QByteArray sanitizeJsonPayload(const QByteArray &payload);
    static void onMqttMessage(RobotStatusDialog *dlg, const QString &topic, const QByteArray &payload);
};
