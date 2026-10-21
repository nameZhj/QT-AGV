/**
 * @file    AppConfigProfileSerializer.cpp
 * @layer   AppLayer
 * @brief   Implementation of DeviceProfile serialization to/from QSettings.
 * @deps    AppConfigProfileSerializer.h, AppConfig.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "AppConfigProfileSerializer.h"
#include "AppConfig.h"

#include <QSettings>

QString AppConfigProfileSerializer::cleanHostIp(const QString &rawIp)
{
    QString ip = rawIp.trimmed();
    if (ip.startsWith("http://", Qt::CaseInsensitive)) {
        ip = ip.mid(7);
    } else if (ip.startsWith("https://", Qt::CaseInsensitive)) {
        ip = ip.mid(8);
    }
    while (ip.endsWith('/')) {
        ip.chop(1);
    }
    return ip.trimmed();
}

QList<DeviceProfile> AppConfigProfileSerializer::readDeviceProfiles(QSettings &settings)
{
    QList<DeviceProfile> list;
    const int count = settings.beginReadArray("devices");
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        DeviceProfile p;
        p.name          = settings.value("name").toString();
        p.ip            = cleanHostIp(settings.value("ip").toString());
        p.carId         = settings.value("carId", "1").toString().trimmed();
        if (p.carId.isEmpty()) p.carId = "1";
        p.token         = settings.value("token").toString();
        p.lastConnected = settings.value("lastConnected").toDateTime();
        p.linkStatus    = settings.value("linkStatus").toString();
        p.mapName       = settings.value("mapName").toString();
        p.version       = settings.value("version").toString();
        p.sshUser       = settings.value("sshUser", "tuju").toString();
        p.sshPwd        = settings.value("sshPwd", "vsis1234").toString();
        p.sshPort       = settings.value("sshPort", AppConfig::kDefaultSshPort).toInt();
        if (p.sshPort <= 0) p.sshPort = AppConfig::kDefaultSshPort;
        list.append(p);
    }
    settings.endArray();
    return list;
}

void AppConfigProfileSerializer::writeDeviceProfiles(QSettings &settings, const QList<DeviceProfile> &list)
{
    settings.beginWriteArray("devices", list.size());
    for (int i = 0; i < list.size(); ++i) {
        settings.setArrayIndex(i);
        const DeviceProfile &p = list[i];
        settings.setValue("name",          p.name);
        settings.setValue("ip",            cleanHostIp(p.ip));
        settings.setValue("carId",         p.carId.trimmed().isEmpty() ? "1" : p.carId.trimmed());
        settings.setValue("token",         p.token);
        settings.setValue("lastConnected", p.lastConnected);
        settings.setValue("linkStatus",    p.linkStatus);
        settings.setValue("mapName",       p.mapName);
        settings.setValue("version",       p.version);
        settings.setValue("sshUser",       p.sshUser);
        settings.setValue("sshPwd",        p.sshPwd);
        settings.setValue("sshPort",       p.sshPort > 0 ? p.sshPort : AppConfig::kDefaultSshPort);
    }
    settings.endArray();
    settings.sync();
}

void AppConfigProfileSerializer::mergeDeviceProfile(DeviceProfile &existing, const DeviceProfile &targetDev)
{
    QString keepMap     = targetDev.mapName.isEmpty() ? existing.mapName : targetDev.mapName;
    QString keepVer     = targetDev.version.isEmpty() ? existing.version : targetDev.version;
    QString keepName    = targetDev.name.isEmpty() ? existing.name : targetDev.name;
    QString keepSshUser = targetDev.sshUser.isEmpty() ? existing.sshUser : targetDev.sshUser;
    QString keepSshPwd  = targetDev.sshPwd.isEmpty() ? existing.sshPwd : targetDev.sshPwd;
    int keepSshPort     = targetDev.sshPort > 0 ? targetDev.sshPort : existing.sshPort;

    existing = targetDev;
    existing.mapName = keepMap;
    existing.version = keepVer;
    existing.name    = keepName;
    existing.sshUser = keepSshUser.isEmpty() ? "tuju" : keepSshUser;
    existing.sshPwd  = keepSshPwd.isEmpty() ? "vsis1234" : keepSshPwd;
    existing.sshPort = keepSshPort > 0 ? keepSshPort : AppConfig::kDefaultSshPort;
}
