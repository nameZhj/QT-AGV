#pragma once

#include <QObject>
#include <QString>
#include <QSettings>
#include <QList>
#include <QDateTime>
#include <QReadWriteLock>
#include <QJsonObject>

/**
 * @brief 机器人/AGV 设备配置档案
 */
struct DeviceProfile {
    QString   name;                   ///< 显示名称, 如 "AGV @ 192.168.1.100"
    QString   ip;                     ///< 服务器 IP
    QString   carId = "1";            ///< 车辆 ID (默认 "1")
    QString   token;                  ///< 授权令牌
    QDateTime lastConnected;          ///< 最近连接时间
    QString   linkStatus;             ///< 链接状态: "0"=已链接, "1"=未链接
    QString   mapName;                ///< 绑定的地图名称
    QString   version;                ///< 系统/固件版本号
    QString   sshUser = "tuju";       ///< SSH 用户名
    QString   sshPwd  = "vsis1234";   ///< SSH 密码
    int       sshPort = 22;           ///< SSH 端口 (默认 22)

    bool isValid() const {
        return !ip.trimmed().isEmpty();
    }

    bool operator==(const DeviceProfile &other) const {
        return ip == other.ip && carId == other.carId && name == other.name;
    }

    bool operator!=(const DeviceProfile &other) const {
        return !(*this == other);
    }
};

/**
 * @brief 全局配置与设备档案管理中心 (Thread-Safe Singleton)
 * 
 * 架构规范与特性：
 * 1. 继承自 QObject，提供响应式配置变更信号 (deviceListChanged, currentDeviceChanged, serverIpChanged 等)；
 * 2. 线程安全读写：内部由 QReadWriteLock 护航，支持跨线程安全并发读取与持久化；
 * 3. 集中收拢服务默认端口常量，消除各模块散落的硬编码魔数；
 * 4. 健壮的 URL 拼接与 IP 前缀自愈机制 (自动纠正携带 http:// 或结尾斜杠的脏数据)；
 * 5. 100% 向下兼容既有业务 API 签名。
 */
class AppConfig final : public QObject
{
    Q_OBJECT

public:
    // ── 系统标准服务端口常量 ──
    static constexpr int kDefaultSshPort         = 22;    ///< SSH 运维端口
    static constexpr int kDefaultHttpPort        = 8048;  ///< HTTP / WebSocket API
    static constexpr int kDefaultMqttPort        = 1883;  ///< MQTT 消息总线
    static constexpr int kDefaultResourcePort    = 8001;  ///< 地图与静态文件资源服务
    static constexpr int kDefaultAgvControl1Port = 8061;  ///< AGV 底盘控制通道 1
    static constexpr int kDefaultAgvControl2Port = 8062;  ///< AGV 底盘控制通道 2

    static AppConfig &instance();

    // ========== 多设备档案管理 ==========
    QList<DeviceProfile> deviceList() const;
    void saveDevice(const DeviceProfile &dev);          ///< 新增或按 IP 更新设备
    void updateDevice(int index, const DeviceProfile &dev);
    void removeDevice(int index);
    void clearDevices();

    // ========== 当前活动设备档案 ==========
    int  currentDeviceIndex() const;
    void setCurrentDeviceIndex(int idx);
    DeviceProfile currentDevice() const;

    // ========== 当前活跃连接参数 ==========
    QString serverIp() const;
    void    setServerIp(const QString &ip);

    QString mqttHost() const;
    void    setMqttHost(const QString &host);

    QString mapHost() const;
    QString pcdHost() const;
    QString agvMapResourceHost() const;

    QString agvId() const;
    void    setAgvId(const QString &id);

    QString token() const;
    void    setToken(const QString &token);

    int     deptId() const;
    void    setDeptId(int id);

    // ========== URL 格式化与构建 ==========
    QString baseApiUrl() const;
    QString robotApiUrl(int port) const;

    // ========== 工具方法 ==========
    static QString cleanHostIp(const QString &rawIp);

signals:
    void deviceListChanged();
    void currentDeviceChanged(int index, const DeviceProfile &profile);
    void serverIpChanged(const QString &newIp);
    void tokenChanged(const QString &newToken);
    void agvIdChanged(const QString &newId);

private:
    explicit AppConfig(QObject *parent = nullptr);
    ~AppConfig() override = default;
    AppConfig(const AppConfig &) = delete;
    AppConfig &operator=(const AppConfig &) = delete;

    void saveDeviceListInternal(const QList<DeviceProfile> &list);

    mutable QReadWriteLock m_lock;
    mutable QSettings      m_settings;
    int                    m_currentDeviceIndex = -1;
};

