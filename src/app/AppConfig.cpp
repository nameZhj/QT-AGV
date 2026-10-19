#include "AppConfig.h"

#include <QVariant>
#include <QReadLocker>
#include <QWriteLocker>

AppConfig &AppConfig::instance()
{
    static AppConfig inst;
    return inst;
}

AppConfig::AppConfig(QObject *parent)
    : QObject(parent)
    , m_settings("AGV", "QT-AGV")
{
}

QString AppConfig::cleanHostIp(const QString &rawIp)
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

// ========== 多设备档案管理 ==========

QList<DeviceProfile> AppConfig::deviceList() const
{
    QReadLocker locker(&m_lock);
    QList<DeviceProfile> list;
    const int count = m_settings.beginReadArray("devices");
    for (int i = 0; i < count; ++i) {
        m_settings.setArrayIndex(i);
        DeviceProfile p;
        p.name          = m_settings.value("name").toString();
        p.ip            = cleanHostIp(m_settings.value("ip").toString());
        p.carId         = m_settings.value("carId", "1").toString().trimmed();
        if (p.carId.isEmpty()) {
            p.carId = "1";
        }
        p.token         = m_settings.value("token").toString();
        p.lastConnected = m_settings.value("lastConnected").toDateTime();
        p.linkStatus    = m_settings.value("linkStatus").toString();
        p.mapName       = m_settings.value("mapName").toString();
        p.version       = m_settings.value("version").toString();
        p.sshUser       = m_settings.value("sshUser", "tuju").toString();
        p.sshPwd        = m_settings.value("sshPwd", "vsis1234").toString();
        p.sshPort       = m_settings.value("sshPort", kDefaultSshPort).toInt();
        if (p.sshPort <= 0) {
            p.sshPort = kDefaultSshPort;
        }
        list.append(p);
    }
    m_settings.endArray();
    return list;
}

void AppConfig::saveDeviceListInternal(const QList<DeviceProfile> &list)
{
    m_settings.beginWriteArray("devices", list.size());
    for (int i = 0; i < list.size(); ++i) {
        m_settings.setArrayIndex(i);
        const DeviceProfile &p = list[i];
        m_settings.setValue("name",          p.name);
        m_settings.setValue("ip",            cleanHostIp(p.ip));
        m_settings.setValue("carId",         p.carId.trimmed().isEmpty() ? "1" : p.carId.trimmed());
        m_settings.setValue("token",         p.token);
        m_settings.setValue("lastConnected", p.lastConnected);
        m_settings.setValue("linkStatus",    p.linkStatus);
        m_settings.setValue("mapName",       p.mapName);
        m_settings.setValue("version",       p.version);
        m_settings.setValue("sshUser",       p.sshUser);
        m_settings.setValue("sshPwd",        p.sshPwd);
        m_settings.setValue("sshPort",       p.sshPort > 0 ? p.sshPort : kDefaultSshPort);
    }
    m_settings.endArray();
    m_settings.sync();
}

void AppConfig::saveDevice(const DeviceProfile &dev)
{
    DeviceProfile targetDev = dev;
    targetDev.ip = cleanHostIp(targetDev.ip);

    {
        QWriteLocker locker(&m_lock);
        QList<DeviceProfile> list;
        const int count = m_settings.beginReadArray("devices");
        for (int i = 0; i < count; ++i) {
            m_settings.setArrayIndex(i);
            DeviceProfile p;
            p.name          = m_settings.value("name").toString();
            p.ip            = cleanHostIp(m_settings.value("ip").toString());
            p.carId         = m_settings.value("carId", "1").toString().trimmed();
            if (p.carId.isEmpty()) p.carId = "1";
            p.token         = m_settings.value("token").toString();
            p.lastConnected = m_settings.value("lastConnected").toDateTime();
            p.linkStatus    = m_settings.value("linkStatus").toString();
            p.mapName       = m_settings.value("mapName").toString();
            p.version       = m_settings.value("version").toString();
            p.sshUser       = m_settings.value("sshUser", "tuju").toString();
            p.sshPwd        = m_settings.value("sshPwd", "vsis1234").toString();
            p.sshPort       = m_settings.value("sshPort", kDefaultSshPort).toInt();
            if (p.sshPort <= 0) p.sshPort = kDefaultSshPort;
            list.append(p);
        }
        m_settings.endArray();

        bool found = false;
        for (DeviceProfile &p : list) {
            if (p.ip == targetDev.ip) {
                QString keepMap     = targetDev.mapName.isEmpty() ? p.mapName : targetDev.mapName;
                QString keepVer     = targetDev.version.isEmpty() ? p.version : targetDev.version;
                QString keepName    = targetDev.name.isEmpty() ? p.name : targetDev.name;
                QString keepSshUser = targetDev.sshUser.isEmpty() ? p.sshUser : targetDev.sshUser;
                QString keepSshPwd  = targetDev.sshPwd.isEmpty() ? p.sshPwd : targetDev.sshPwd;
                int keepSshPort     = targetDev.sshPort > 0 ? targetDev.sshPort : p.sshPort;

                p = targetDev;
                p.mapName = keepMap;
                p.version = keepVer;
                p.name    = keepName;
                p.sshUser = keepSshUser.isEmpty() ? "tuju" : keepSshUser;
                p.sshPwd  = keepSshPwd.isEmpty() ? "vsis1234" : keepSshPwd;
                p.sshPort = keepSshPort > 0 ? keepSshPort : kDefaultSshPort;
                found = true;
                break;
            }
        }

        if (!found) {
            list.append(targetDev);
        }

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
        QList<DeviceProfile> list;
        const int count = m_settings.beginReadArray("devices");
        for (int i = 0; i < count; ++i) {
            m_settings.setArrayIndex(i);
            DeviceProfile p;
            p.name          = m_settings.value("name").toString();
            p.ip            = cleanHostIp(m_settings.value("ip").toString());
            p.carId         = m_settings.value("carId", "1").toString().trimmed();
            if (p.carId.isEmpty()) p.carId = "1";
            p.token         = m_settings.value("token").toString();
            p.lastConnected = m_settings.value("lastConnected").toDateTime();
            p.linkStatus    = m_settings.value("linkStatus").toString();
            p.mapName       = m_settings.value("mapName").toString();
            p.version       = m_settings.value("version").toString();
            p.sshUser       = m_settings.value("sshUser", "tuju").toString();
            p.sshPwd        = m_settings.value("sshPwd", "vsis1234").toString();
            p.sshPort       = m_settings.value("sshPort", kDefaultSshPort).toInt();
            if (p.sshPort <= 0) p.sshPort = kDefaultSshPort;
            list.append(p);
        }
        m_settings.endArray();

        if (index >= 0 && index < list.size()) {
            oldIp = list[index].ip;
            list[index] = dev;
            list[index].ip = cleanHostIp(list[index].ip);
            saveDeviceListInternal(list);
            updated = true;
        }
    }

    if (updated) {
        if (!oldIp.isEmpty() && serverIp() == oldIp) {
            setServerIp(dev.ip);
        }
        emit deviceListChanged();
    }
}

void AppConfig::removeDevice(int index)
{
    bool removed = false;

    {
        QWriteLocker locker(&m_lock);
        QList<DeviceProfile> list;
        const int count = m_settings.beginReadArray("devices");
        for (int i = 0; i < count; ++i) {
            m_settings.setArrayIndex(i);
            DeviceProfile p;
            p.name          = m_settings.value("name").toString();
            p.ip            = cleanHostIp(m_settings.value("ip").toString());
            p.carId         = m_settings.value("carId", "1").toString().trimmed();
            if (p.carId.isEmpty()) p.carId = "1";
            p.token         = m_settings.value("token").toString();
            p.lastConnected = m_settings.value("lastConnected").toDateTime();
            p.linkStatus    = m_settings.value("linkStatus").toString();
            p.mapName       = m_settings.value("mapName").toString();
            p.version       = m_settings.value("version").toString();
            p.sshUser       = m_settings.value("sshUser", "tuju").toString();
            p.sshPwd        = m_settings.value("sshPwd", "vsis1234").toString();
            p.sshPort       = m_settings.value("sshPort", kDefaultSshPort).toInt();
            if (p.sshPort <= 0) p.sshPort = kDefaultSshPort;
            list.append(p);
        }
        m_settings.endArray();

        if (index >= 0 && index < list.size()) {
            list.removeAt(index);
            saveDeviceListInternal(list);
            removed = true;

            if (list.isEmpty()) {
                m_settings.setValue("currentDeviceIndex", -1);
            } else {
                int cur = m_settings.value("currentDeviceIndex", 0).toInt();
                if (cur >= list.size()) {
                    m_settings.setValue("currentDeviceIndex", qMax(0, list.size() - 1));
                }
            }
            m_settings.sync();
        }
    }

    if (removed) {
        emit deviceListChanged();
    }
}

void AppConfig::clearDevices()
{
    {
        QWriteLocker locker(&m_lock);
        saveDeviceListInternal({});
        m_settings.remove("currentDeviceIndex");
        m_settings.sync();
    }
    emit deviceListChanged();
}

// ========== 当前活动设备档案 ==========

int AppConfig::currentDeviceIndex() const
{
    QReadLocker locker(&m_lock);
    return m_settings.value("currentDeviceIndex", -1).toInt();
}

void AppConfig::setCurrentDeviceIndex(int idx)
{
    bool changed = false;
    DeviceProfile activeDev;
    {
        QWriteLocker locker(&m_lock);
        const int oldIdx = m_settings.value("currentDeviceIndex", -1).toInt();
        if (oldIdx != idx) {
            m_settings.setValue("currentDeviceIndex", idx);
            m_settings.sync();
            changed = true;
        }
    }

    if (changed) {
        activeDev = currentDevice();
        emit currentDeviceChanged(idx, activeDev);
    }
}

DeviceProfile AppConfig::currentDevice() const
{
    const QList<DeviceProfile> list = deviceList();
    const int idx = currentDeviceIndex();
    if (idx >= 0 && idx < list.size()) {
        DeviceProfile dev = list[idx];
        if (dev.carId.trimmed().isEmpty()) {
            dev.carId = "1";
        }
        return dev;
    }

    DeviceProfile dev;
    dev.carId = "1";
    dev.ip = serverIp();
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

    if (changed) {
        emit serverIpChanged(cleaned);
    }
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
        const QString oldId = m_settings.value("agvId", "1").toString().trimmed();
        if (oldId != finalId) {
            m_settings.setValue("agvId", finalId);
            m_settings.sync();
            changed = true;
        }
    }

    if (changed) {
        emit agvIdChanged(finalId);
    }
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
        const QString oldToken = m_settings.value("token", "").toString();
        if (oldToken != token) {
            m_settings.setValue("token", token);
            m_settings.sync();
            changed = true;
        }
    }

    if (changed) {
        emit tokenChanged(token);
    }
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

// ========== URL 格式化与构建 ==========

QString AppConfig::baseApiUrl() const
{
    return QStringLiteral("http://") + serverIp() + QStringLiteral(":") + QString::number(kDefaultHttpPort);
}

QString AppConfig::robotApiUrl(int port) const
{
    const int targetPort = (port > 0) ? port : kDefaultHttpPort;
    return QStringLiteral("http://") + serverIp() + QStringLiteral(":") + QString::number(targetPort);
}
