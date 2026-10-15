#pragma once

#include <QString>
#include <QPair>
#include <QJsonObject>
#include <QJsonArray>

/**
 * @brief 参数配置与元数据解析辅助类 (纯业务逻辑计算与字典，解耦巨型 Widget)
 */
class ConfigMetadataHelper
{
public:
    ConfigMetadataHelper() = delete;

    /**
     * @brief 获取参数字段对应的中文名称与备注单位
     */
    static QPair<QString, QString> fieldMetaFor(const QString &key);

    /**
     * @brief 根据配置文件的 name / file_path 计算归属的业务分组 ProfileKey
     */
    static QString determineProfile(const QJsonObject &entry);

    /**
     * @brief 根据 ProfileKey 获取展示的中文分组名称
     */
    static QString getProfileName(const QString &key);

    /**
     * @brief 获取子配置文件的中文展示名称 (如前雷达、后雷达)
     */
    static QString getChildDisplayName(const QJsonObject &entry);

    /**
     * @brief 提取路径的基文件名
     */
    static QString basename(const QString &path);

    /**
     * @brief 从元数据列表中查找指定配置文件的元数据条目
     */
    static QJsonObject findMetadataForConfig(const QJsonArray &metadataList, const QJsonObject &configEntry);

    /**
     * @brief 从元数据对象中解析参数默认值格式化文本
     */
    static QString getDefaultValueString(const QJsonObject &metaObj, const QString &fullPath);

    /**
     * @brief 从元数据对象中提取参数的 min 和 max 上下限范围
     */
    static bool getParamMinMax(const QJsonObject &metaObj,
                               const QString &fullPath,
                               double &outMin, double &outMax,
                               bool &hasMin, bool &hasMax);

    /**
     * @brief 补全运动控制、二次对位及避障扩展区间默认参数条目
     */
    static void ensureMotionControlConfig(QJsonArray &configs);
    static void ensureAlignConfig(QJsonArray &configs);
    static void ensureObstacleConfig(QJsonArray &configs);
};
