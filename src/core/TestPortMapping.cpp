#include "TestPortMapping.h"

TestPortMapping &TestPortMapping::instance()
{
    static TestPortMapping inst;
    return inst;
}

TestPortMapping::TestPortMapping()
    : m_settings("AGV", "QT-AGV-TestPortMapping")
{
    resetToDefaultRules();
}

bool TestPortMapping::isEnabled() const
{
    // 默认开启端口映射开关（可通过 setEnabled(false) 随时关闭）
    return m_settings.value("enabled", true).toBool();
}

void TestPortMapping::setEnabled(bool enabled)
{
    m_settings.setValue("enabled", enabled);
    m_settings.sync();
}

int TestPortMapping::mapPort(int internalPort) const
{
    if (!isEnabled()) {
        return internalPort;
    }
    return m_rules.value(internalPort, internalPort);
}

void TestPortMapping::setMappingRule(int internalPort, int outerPort)
{
    m_rules[internalPort] = outerPort;
}

void TestPortMapping::removeMappingRule(int internalPort)
{
    m_rules.remove(internalPort);
}

QMap<int, int> TestPortMapping::getAllRules() const
{
    return m_rules;
}

void TestPortMapping::resetToDefaultRules()
{
    m_rules.clear();
    // 端口映射规则表 (内网端口 -> 外网映射端口)
    m_rules[1883] = 6020; // MQTT 消息服务
    m_rules[8061] = 6017; // AGV 控制服务 1
    m_rules[8062] = 6018; // AGV 控制服务 2
    m_rules[8048] = 6015; // HTTP 后台 API & WebSocket
    m_rules[8001] = 6019; // 地图及 PCD 资源服务
}
