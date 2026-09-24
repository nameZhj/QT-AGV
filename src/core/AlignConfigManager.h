#pragma once

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>

/**
 * @brief 全局对齐与深度参数配置管理器 (单例)
 * 
 * 核心功能:
 * 1. 集中管理与持久化二次定位/正走入相关参数:
 *    - anti_align_depth: 正走入深度 (单位米, 默认 0.0)
 *    - cylinder_length: 前后货架腿中心点距离 (单位米, 默认 1.2)
 * 2. 动态运算全局变量 epc_x:
 *    epc_x = anti_align_depth - cylinder_length
 *    每次使用 epc_x 均单独进行一次运算。
 * 3. 支持 QSettings 启动自恢复与保存自动持久化。
 */
class AlignConfigManager : public QObject
{
    Q_OBJECT

public:
    static AlignConfigManager &instance();

    // 读取基础参数
    double antiAlignDepth() const { return m_antiAlignDepth; }
    double cylinderLength() const { return m_cylinderLength; }

    // 动态全局计算 epc_x: 每次使用均单独进行一次运算
    double epcX() const {
        return m_antiAlignDepth - m_cylinderLength;
    }

    // 写入与同步接口 (自动持久化到本地 QSettings 并发射信号)
    void setAntiAlignDepth(double val);
    void setCylinderLength(double val);

    // 从 params JSON 对象或 configs 数组中提取/更新
    void updateFromJsonObject(const QJsonObject &params);
    void updateFromConfigs(const QJsonArray &configs);

signals:
    void configChanged();

private:
    explicit AlignConfigManager(QObject *parent = nullptr);
    void loadSettings();
    void saveSettings();

    double m_antiAlignDepth = 0.0;
    double m_cylinderLength = 1.2;
};
