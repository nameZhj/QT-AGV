/**
 * @file    AppConfig.cpp
 * @layer   AppLayer
 * @brief   Global configuration and device profile management center.
 * @deps    AppConfig.h, AppConfigProfileSerializer.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "AppConfig.h"
#include "AppConfigProfileSerializer.h"

#include <QVariant>
#include <QReadLocker>
#include <QWriteLocker>

AppConfig &AppConfig::instance()
{
    static AppConfig inst;
    return inst;
}

AppConfig::AppConfig(QObject *parent)
    : QObject(parent), m_settings("AGV", "QT-AGV"), m_currentDeviceIndex(-1)
{
    m_settings.remove("currentDeviceIndex");
}

QString AppConfig::cleanHostIp(const QString &rawIp)
{
    return AppConfigProfileSerializer::cleanHostIp(rawIp);
}

// ========== 多设备档案管理 ==========

QList<DeviceProfile> AppConfig::deviceList() const
{
    QReadLocker locker(&m_lock);
    return AppConfigProfileSerializer::readDeviceProfiles(m_settings);
}

void AppConfig::saveDeviceListInternal(const QList<DeviceProfile> &list)
{
    AppConfigProfileSerializer::writeDeviceProfiles(m_settings, list);
}

void AppConfig::saveDevice(const DeviceProfile &dev)
{
    DeviceProfile targetDev = dev;
    targetDev.ip = cleanHostIp(targetDev.ip);

    {
        QWriteLocker locker(&m_lock);
        QList<DeviceProfile> list = AppConfigProfileSerializer::readDeviceProfiles(m_settings);
        bool found = false;
        for (DeviceProfile &p : list) {
            if (p.ip == targetDev.ip) {
                AppConfigProfileSerializer::mergeDeviceProfile(p, targetDev);
                found = true;
                break;
            }
        }
        if (!found) list.append(targetDev);
        saveDeviceListInternal(list);
    }
    emit deviceListChanged();
}

void AppConfig::updateDevice(int index, const DeviceProfile &dev)
{
    QString oldIp;
    bool updated = false;
    {
        QWriteLocker locker(&m_lock);
        QList<DeviceProfile> list = AppConfigProfileSerializer::readDeviceProfiles(m_settings);
        if (index >= 0 && index < list.size()) {
            oldIp = list[index].ip;
            list[index] = dev;
            list[index].ip = cleanHostIp(list[index].ip);
            saveDeviceListInternal(list);
            updated = true;
        }
    }
    if (updated) {
        if (!oldIp.isEmpty() && serverIp() == oldIp) setServerIp(dev.ip);
        emit deviceListChanged();
    }
}

void AppConfig::removeDevice(int index)
{
    bool removed = false;
    {
        QWriteLocker locker(&m_lock);
        QList<DeviceProfile> list = AppConfigProfileSerializer::readDeviceProfiles(m_settings);
        if (index >= 0 && index < list.size()) {
            list.removeAt(index);
            saveDeviceListInternal(list);
            removed = true;
            if (list.isEmpty() || m_currentDeviceIndex == index) m_currentDeviceIndex = -1;
            else if (m_currentDeviceIndex > index) m_currentDeviceIndex--;
        }
    }
    if (removed) emit deviceListChanged();
}

void AppConfig::clearDevices()
{
    {
        QWriteLocker locker(&m_lock);
        saveDeviceListInternal({});
        m_currentDeviceIndex = -1;
        m_settings.remove("currentDeviceIndex");
        m_settings.sync();
    }
    emit deviceListChanged();
}

// ========== 当前活动设备档案 ==========

int AppConfig::currentDeviceIndex() const
{
    QReadLocker locker(&m_lock);
    return m_currentDeviceIndex;
}

void AppConfig::setCurrentDeviceIndex(int idx)
{
    bool changed = false;
    {
        QWriteLocker locker(&m_lock);
        if (m_currentDeviceIndex != idx) {
            m_currentDeviceIndex = idx;
            changed = true;
        }
    }
    if (changed) emit currentDeviceChanged(idx, currentDevice());
}

DeviceProfile AppConfig::currentDevice() const
{
    const QList<DeviceProfile> list = deviceList();
    const int idx = currentDeviceIndex();
    if (idx >= 0 && idx < list.size()) {
        DeviceProfile dev = list[idx];
        if (dev.carId.trimmed().isEmpty()) dev.carId = "1";
        return dev;
    }
    DeviceProfile dev;
    dev.carId = "1";
    return dev;
}

// ========== 当前活跃连接参数 ==========

QString AppConfig::serverIp() const
{
    QReadLocker locker(&m_lock);
    return cleanHostIp(m_settings.value("serverIp", "").toString());
}

void AppConfig::setServerIp(const QString &ip)
{
    const QString cleaned = cleanHostIp(ip);
    bool changed = false;
    {
        QWriteLocker locker(&m_lock);
        const QString oldIp = cleanHostIp(m_settings.value("serverIp", "").toString());
        if (oldIp != cleaned) {
            m_settings.setValue("serverIp", cleaned);
            m_settings.sync();
            changed = true;
        }
    }
    if (changed) emit serverIpChanged(cleaned);
}

QString AppConfig::mqttHost() const
{
    QReadLocker locker(&m_lock);
    return m_settings.value("mqttHost", serverIp() + ":" + QString::number(kDefaultMqttPort)).toString();
}

void AppConfig::setMqttHost(const QString &host)
{
    QWriteLocker locker(&m_lock);
    m_settings.setValue("mqttHost", host.trimmed());
    m_settings.sync();
}

QString AppConfig::mapHost() const
{
    return QStringLiteral("http://") + serverIp() + QStringLiteral(":") + QString::number(kDefaultResourcePort) + QStringLiteral("/RobotResource/map/");
}

QString AppConfig::pcdHost() const
{
    return QStringLiteral("http://") + serverIp() + QStringLiteral(":") + QString::number(kDefaultResourcePort) + QStringLiteral("/");
}

QString AppConfig::agvMapResourceHost() const
{
    return QStringLiteral("http://") + serverIp() + QStringLiteral(":") + QString::number(kDefaultResourcePort) + QStringLiteral("/RobotResource/agvMapResource/");
}

QString AppConfig::agvId() const
{
    QReadLocker locker(&m_lock);
    QString id = m_settings.value("agvId", "1").toString().trimmed();
    return id.isEmpty() ? QStringLiteral("1") : id;
}

void AppConfig::setAgvId(const QString &id)
{
    const QString finalId = id.trimmed().isEmpty() ? QStringLiteral("1") : id.trimmed();
    bool changed = false;
    {
        QWriteLocker locker(&m_lock);
        if (m_settings.value("agvId", "1").toString().trimmed() != finalId) {
            m_settings.setValue("agvId", finalId);
            m_settings.sync();
            changed = true;
        }
    }
    if (changed) emit agvIdChanged(finalId);
}

QString AppConfig::token() const
{
    QReadLocker locker(&m_lock);
    return m_settings.value("token", "").toString();
}

void AppConfig::setToken(const QString &token)
{
    bool changed = false;
    {
        QWriteLocker locker(&m_lock);
        if (m_settings.value("token", "").toString() != token) {
            m_settings.setValue("token", token);
            m_settings.sync();
            changed = true;
        }
    }
    if (changed) emit tokenChanged(token);
}

int AppConfig::deptId() const
{
    QReadLocker locker(&m_lock);
    return m_settings.value("deptId", 100).toInt();
}

void AppConfig::setDeptId(int id)
{
    QWriteLocker locker(&m_lock);
    m_settings.setValue("deptId", id);
    m_settings.sync();
}

QString AppConfig::baseApiUrl() const
{
    return QStringLiteral("http://") + serverIp() + QStringLiteral(":") + QString::number(kDefaultHttpPort);
}

QString AppConfig::robotApiUrl(int port) const
{
    const int targetPort = (port > 0) ? port : kDefaultHttpPort;
    return QStringLiteral("http://") + serverIp() + QStringLiteral(":") + QString::number(targetPort);
}
