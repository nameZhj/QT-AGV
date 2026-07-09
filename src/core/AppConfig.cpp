#include "AppConfig.h"
#include <QVariant>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

AppConfig &AppConfig::instance()
{
    static AppConfig inst;
    return inst;
}

AppConfig::AppConfig()
    : m_settings("AGV", "QT-AGV")
{
}

// ========== Multi-device profile management ==========

QList<DeviceProfile> AppConfig::deviceList() const
{
    QList<DeviceProfile> list;
    int count = m_settings.beginReadArray("devices");
    for (int i = 0; i < count; ++i) {
        m_settings.setArrayIndex(i);
        DeviceProfile p;
        p.name          = m_settings.value("name").toString();
        p.ip            = m_settings.value("ip").toString();
        p.carId         = m_settings.value("carId").toString();
        p.token         = m_settings.value("token").toString();
        p.lastConnected = m_settings.value("lastConnected").toDateTime();
        list.append(p);
    }
    m_settings.endArray();
    return list;
}

void AppConfig::saveDeviceList(const QList<DeviceProfile> &list)
{
    m_settings.beginWriteArray("devices", list.size());
    for (int i = 0; i < list.size(); ++i) {
        m_settings.setArrayIndex(i);
        const DeviceProfile &p = list[i];
        m_settings.setValue("name",          p.name);
        m_settings.setValue("ip",            p.ip);
        m_settings.setValue("carId",         p.carId);
        m_settings.setValue("token",         p.token);
        m_settings.setValue("lastConnected", p.lastConnected);
    }
    m_settings.endArray();
    m_settings.sync();
}

void AppConfig::saveDevice(const DeviceProfile &dev)
{
    QList<DeviceProfile> list = deviceList();
    // Update existing entry by IP
    for (DeviceProfile &p : list) {
        if (p.ip == dev.ip) {
            p = dev;
            saveDeviceList(list);
            return;
        }
    }
    list.append(dev);
    saveDeviceList(list);
}

void AppConfig::removeDevice(int index)
{
    QList<DeviceProfile> list = deviceList();
    if (index >= 0 && index < list.size()) {
        list.removeAt(index);
        saveDeviceList(list);

        if (list.isEmpty()) {
            setCurrentDeviceIndex(-1);
        } else {
            int cur = currentDeviceIndex();
            if (cur >= list.size())
                setCurrentDeviceIndex(qMax(0, list.size() - 1));
        }
    }
}

void AppConfig::clearDevices()
{
    saveDeviceList({});
    m_settings.remove("currentDeviceIndex");
    m_settings.sync();
}

int AppConfig::currentDeviceIndex() const
{
    return m_settings.value("currentDeviceIndex", 0).toInt();
}

void AppConfig::setCurrentDeviceIndex(int idx)
{
    m_settings.setValue("currentDeviceIndex", idx);
    m_settings.sync();
}

DeviceProfile AppConfig::currentDevice() const
{
    QList<DeviceProfile> list = deviceList();
    int idx = currentDeviceIndex();
    if (idx >= 0 && idx < list.size())
        return list[idx];
    return {};
}

// ========== Active connection settings ==========

QString AppConfig::serverIp() const
{
    return m_settings.value("serverIp", "").toString();
}

void AppConfig::setServerIp(const QString &ip)
{
    m_settings.setValue("serverIp", ip);
    m_settings.sync();
}

QString AppConfig::mqttHost() const
{
    return m_settings.value("mqttHost", serverIp() + ":1883").toString();
}

void AppConfig::setMqttHost(const QString &host)
{
    m_settings.setValue("mqttHost", host);
    m_settings.sync();
}

QString AppConfig::mapHost() const
{
    return "http://" + serverIp() + ":" + QString::number(mapPort(8001)) + "/RobotResource/map/";
}

QString AppConfig::pcdHost() const
{
    return "http://" + serverIp() + ":" + QString::number(mapPort(8001)) + "/";
}

QString AppConfig::agvMapResourceHost() const
{
    // 与 mapHost() 同一资源根(RobotResource)下的原始 pgm/yaml 存放目录
    // (后端 RuoYiConfig.getAgvMapResourcePath() = {profile}/agvMapResource/)
    return "http://" + serverIp() + ":" + QString::number(mapPort(8001)) + "/RobotResource/agvMapResource/";
}

QString AppConfig::agvId() const
{
    return m_settings.value("agvId", "").toString();
}

void AppConfig::setAgvId(const QString &id)
{
    m_settings.setValue("agvId", id);
    m_settings.sync();
}

QString AppConfig::token() const
{
    return m_settings.value("token", "").toString();
}

void AppConfig::setToken(const QString &token)
{
    m_settings.setValue("token", token);
    m_settings.sync();
}

int AppConfig::deptId() const
{
    return m_settings.value("deptId", 100).toInt();
}

void AppConfig::setDeptId(int id)
{
    m_settings.setValue("deptId", id);
    m_settings.sync();
}

QString AppConfig::baseApiUrl() const
{
    return "http://" + serverIp() + ":8048";
}

QString AppConfig::robotApiUrl(int port) const
{
    return "http://" + serverIp() + ":" + QString::number(mapPort(port));
}

int AppConfig::mapPort(int internalPort)
{
    // 已取消端口映射，直接使用原始端口连接服务器
    return internalPort;
}
