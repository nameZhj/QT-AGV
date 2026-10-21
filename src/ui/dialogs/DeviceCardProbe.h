/**
 * @file    DeviceCardProbe.h
 * @layer   GUILayer
 * @brief   Handles network latency probing and telemetry status polling for DeviceCard.
 * @deps    QString
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QString>

class DeviceCard;

class DeviceCardProbe
{
public:
    static void testLatency(DeviceCard *card, const QString &ip);
    static void probeDeviceStatus(DeviceCard *card, const QString &ip);
};
