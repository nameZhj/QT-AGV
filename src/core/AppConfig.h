#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <QString>
#include <QSettings>
#include <QList>
#include <QDateTime>

// Device profile saved locally
struct DeviceProfile {
    QString name;           // display name, e.g. "AGV @ 192.168.1.100"
    QString ip;             // server IP
    QString carId;          // car ID returned from API
    QString token;          // last auth token
    QDateTime lastConnected;
    QString linkStatus;     // 设备链接状态: "0"=已链接, "1"=未链接 (参考 Vue: linkStatus==='0' 时链接按钮禁用)
    QString mapName;        // 绑定的地图名称 (持久化缓存)
    QString version;        // 系统/固件版本号 (持久化缓存)
};

// 全局应用配置
// 已取消端口映射，直接使用各服务的原始端口连接远程服务器
// (SSH 22, HTTP后台 400, 地图资源 4000, PCD资源 4001, API 8048,
//  MQTT 1883, AGV控制1 8061, AGV控制2 8062, 地图资源服务 8001)
class AppConfig
{
public:
    static AppConfig &instance();

    // ========== Multi-device profile management ==========
    QList<DeviceProfile> deviceList() const;
    void saveDevice(const DeviceProfile &dev);   // add or update by IP
    void updateDevice(int index, const DeviceProfile &dev);
    void removeDevice(int index);
    void clearDevices();

    // Currently active device
    int  currentDeviceIndex() const;
    void setCurrentDeviceIndex(int idx);
    DeviceProfile currentDevice() const;

    // ========== Active connection settings (from current device) ==========
    QString serverIp() const;
    void setServerIp(const QString &ip);

    QString mqttHost() const;
    void setMqttHost(const QString &host);

    QString mapHost() const;
    QString pcdHost() const;
    QString agvMapResourceHost() const;  // raw carto_map.pgm/.yaml resources

    QString agvId() const;
    void setAgvId(const QString &id);

    QString token() const;
    void setToken(const QString &token);

    int deptId() const;
    void setDeptId(int id);

    // Build URLs
    QString baseApiUrl() const;
    QString robotApiUrl(int port) const;

    // 根据 TestPortMapping 测试环境规则对端口进行映射转换（带映射使能开关）
    static int mapPort(int internalPort);

    // ========== Test mode ==========
    bool testMode() const;
    void setTestMode(bool enabled);
    // 添加一个模拟设备用于测试
    void ensureTestDevice();

private:
    AppConfig();
    ~AppConfig() = default;
    AppConfig(const AppConfig &) = delete;
    AppConfig &operator=(const AppConfig &) = delete;

    void saveDeviceList(const QList<DeviceProfile> &list);

    mutable QSettings m_settings;
};

#endif // APPCONFIG_H
