#ifndef TESTPORTMAPPING_H
#define TESTPORTMAPPING_H

#include <QMap>
#include <QSettings>
#include <QString>

/**
 * @brief 测试环境外网端口映射管理类
 * 专门用于测试环境中将内网服务的原始端口映射为外网映射端口。
 *
 * 映射对应关系 (默认配置):
 *  - 内网 1883 (MQTT 服务)              -> 外网 6020
 *  - 内网 8061 (AGV 控制服务 1)         -> 外网 6017
 *  - 内网 8062 (AGV 控制服务 2)         -> 外网 6018
 *  - 内网 8048 (HTTP 后台 API & WS)     -> 外网 6015
 *  - 内网 8001 (地图及 PCD 资源服务)    -> 外网 6019
 */
class TestPortMapping
{
public:
    static TestPortMapping &instance();

    // 端口映射功能全局开关
    bool isEnabled() const;
    void setEnabled(bool enabled);

    // 转换端口。若开关开启且存在映射关系，返回对应的外网端口；否则返回原始内网端口
    int mapPort(int internalPort) const;

    // 自定义修改/新增映射规则 (内网端口 -> 外网端口)
    void setMappingRule(int internalPort, int outerPort);
    void removeMappingRule(int internalPort);

    // 获取当前所有映射规则 (key: 内网端口, value: 外网映射端口)
    QMap<int, int> getAllRules() const;

    // 重置恢复为默认测试映射配置
    void resetToDefaultRules();

private:
    TestPortMapping();
    ~TestPortMapping() = default;
    TestPortMapping(const TestPortMapping &) = delete;
    TestPortMapping &operator=(const TestPortMapping &) = delete;

    mutable QSettings m_settings;
    QMap<int, int> m_rules; // internalPort -> outerPort
};

#endif // TESTPORTMAPPING_H
