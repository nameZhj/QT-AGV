/**
 * @file    AppConfigProfileSerializer.h
 * @layer   AppLayer
 * @brief   Persistence serializer and IP normalization helper for DeviceProfile.
 * @deps    AppConfig.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QString>
#include <QList>

class QSettings;
struct DeviceProfile;

class AppConfigProfileSerializer
{
public:
    static QString cleanHostIp(const QString &rawIp);
    static QList<DeviceProfile> readDeviceProfiles(QSettings &settings);
    static void writeDeviceProfiles(QSettings &settings, const QList<DeviceProfile> &list);
    static void mergeDeviceProfile(DeviceProfile &existing, const DeviceProfile &targetDev);
};
