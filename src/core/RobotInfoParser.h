/**
 * @file    RobotInfoParser.h
 * @layer   CoreScheduling
 * @brief   JSON parsing & duration formatting utilities for RobotInfo telemetry.
 * @deps    RobotInfoManager.h, QJsonObject
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */
#pragma once

#include <QString>
#include <QJsonObject>

struct RobotInfo;

class RobotInfoParser
{
public:
    static bool parseInfoJson(const QJsonObject &d, RobotInfo *outInfo);
    static QString formatDuration(qint64 totalSeconds);
};
