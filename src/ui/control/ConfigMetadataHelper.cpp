/**
 * @file    ConfigMetadataHelper.cpp
 * @layer   GUILayer
 * @brief   Implementation of config metadata parser and tree item mapper.
 * @deps    ConfigMetadataHelper.h, ConfigFieldDictionary.h, ConfigDefaultCompleter.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "ConfigMetadataHelper.h"
#include "ConfigFieldDictionary.h"
#include "ConfigDefaultCompleter.h"
#include "core/MotionConfigManager.h"

#include <QJsonDocument>
#include <QMap>

// === [Section: Metadata & Field Dictionaries] ===
QPair<QString, QString> ConfigMetadataHelper::fieldMetaFor(const QString &key)
{
    return ConfigFieldDictionary::fieldMetaFor(key);
}

// === [Section: Profiles & Categorization] ===
QString ConfigMetadataHelper::determineProfile(const QJsonObject &entry)
{
    const QString name = entry.value("name").toString().toLower();
    const QString path = entry.value("file_path").toString().toLower();

    if (name == "motion_control" || name.contains("motion_control") ||
        path.contains("motion_control") || name == "运动控制") return "MOTION_CONTROL";
    if (path.contains("dispatch/config/config.yaml")) return "DISPATCH";
    if (path.contains("secondary_localization_reflector/config/config.yaml")) return "SECONDARY_LOC";
    if (name.contains("r2000_params")) return "C200_LIDAR";
    if (name.contains("pid")) return "PID";
    if (name.contains("bringup")) return "BRINGUP";
    if (name.contains("mapping")) return "MAPPING";
    if (name.contains("dispatch")) return "DISPATCH";
    if (name.contains("laser") || name.contains("lidar")) return "LASER_LINK";
    if (name.contains("imu")) return "IMU_LINK";
    if (name.contains("location") || name.contains("localization")) return "LOCATION";
    if (name.contains("secondary") || name.contains("二次定位")) return "SECONDARY_LOC";
    if (name.contains("olei_params")) return "olei_params";
    if (name.contains("system_component")) return "system_component";
    if (name.contains("motor_control") || path.contains("motor_control")) return "MOTOR_CONTROL";
    if (name.contains("config") || path.contains("config.yaml")) return "OBSTACLE_CONFIG";
    return "UNKNOWN";
}

QString ConfigMetadataHelper::getProfileName(const QString &key)
{
    static const QMap<QString, QString> names = {
        {"MOTION_CONTROL", "运动控制"},
        {"DISPATCH", "任务调度"},
        {"SECONDARY_LOC", "二次定位"},
        {"C200_LIDAR", "激光雷达驱动"},
        {"PID", "PID 控制"},
        {"BRINGUP", "Bringup"},
        {"MAPPING", "建图参数"},
        {"LASER_LINK", "雷达安装位姿"},
        {"IMU_LINK", "IMU 安装位姿"},
        {"LOCATION", "定位参数"},
        {"olei_params", "避障雷达"},
        {"system_component", "避障相关参数"},
        {"MOTOR_CONTROL", "电机驱动参数"},
        {"OBSTACLE_CONFIG", "避障配置"},
        {"UNKNOWN", "其他配置"},
    };
    return names.value(key, key);
}

QString ConfigMetadataHelper::getChildDisplayName(const QJsonObject &entry)
{
    const QString name = entry.value("name").toString();
    const QString path = entry.value("file_path").toString();
    const QString pLower = path.toLower();

    if (pLower.contains("laser_1") || pLower.contains("laser1") || pLower.contains("front_laser")) return "前雷达安装位姿";
    if (pLower.contains("laser_2") || pLower.contains("laser2") || pLower.contains("back_laser")) return "后雷达安装位姿";
    if (pLower.contains("olei_1") || pLower.contains("front_olei")) return "前避障雷达";
    if (pLower.contains("olei_2") || pLower.contains("back_olei")) return "后避障雷达";

    return name.isEmpty() ? basename(path) : name;
}

QString ConfigMetadataHelper::basename(const QString &path)
{
    const int idx = path.lastIndexOf('/');
    return idx >= 0 ? path.mid(idx + 1) : path;
}

// === [Section: Metadata Matching & Parsing] ===
QJsonObject ConfigMetadataHelper::findMetadataForConfig(const QJsonArray &metadataList, const QJsonObject &configEntry)
{
    const QString configName = configEntry.value("name").toString();
    const QString configPath = configEntry.value("file_path").toString();

    for (const auto &val : metadataList) {
        const QJsonObject meta = val.toObject();
        if (!configName.isEmpty() && meta.value("name").toString() == configName) return meta;
        if (!configPath.isEmpty() && meta.value("file_path").toString() == configPath) return meta;
    }
    return QJsonObject();
}

QString ConfigMetadataHelper::getDefaultValueString(const QJsonObject &metaObj, const QString &fullPath)
{
    if (metaObj.isEmpty() || fullPath.isEmpty()) return QStringLiteral("--");

    QJsonObject curObj = metaObj.value("params").toObject();
    const QStringList parts = fullPath.split('.');
    for (int i = 0; i < parts.size() - 1; ++i) curObj = curObj.value(parts[i]).toObject();

    const QJsonObject leafObj = curObj.value(parts.last()).toObject();
    if (leafObj.isEmpty() || !leafObj.contains("default")) return QStringLiteral("--");

    const QJsonValue def = leafObj.value("default");
    if (def.isString()) return def.toString();
    if (def.isDouble()) {
        const double d = def.toDouble();
        return (d == static_cast<long long>(d)) ? QString::number(static_cast<long long>(d))
                                                : QString::number(d, 'g', 10);
    }
    if (def.isBool()) return def.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (def.isArray()) {
        return QString::fromUtf8(QJsonDocument(def.toArray()).toJson(QJsonDocument::Compact));
    }
    return QStringLiteral("--");
}

bool ConfigMetadataHelper::getParamMinMax(const QJsonObject &metaObj,
                                          const QString &fullPath,
                                          double &outMin, double &outMax,
                                          bool &hasMin, bool &hasMax)
{
    hasMin = false;
    hasMax = false;
    if (fullPath.isEmpty()) return false;

    auto parseLimit = [](const QJsonValue &val, double &outVal) -> bool {
        if (val.isNull() || val.isUndefined()) return false;
        if (val.isDouble()) {
            outVal = val.toDouble();
            return true;
        }
        if (val.isString()) {
            const QString s = val.toString().trimmed();
            if (s.isEmpty() || s.compare("null", Qt::CaseInsensitive) == 0 || s == "--" || s == "none") return false;
            bool ok = false;
            const double d = s.toDouble(&ok);
            if (ok) {
                outVal = d;
                return true;
            }
        }
        return false;
    };

    if (!metaObj.isEmpty()) {
        QJsonObject curObj = metaObj.value("params").toObject();
        const QStringList parts = fullPath.split('.');
        for (int i = 0; i < parts.size() - 1; ++i) curObj = curObj.value(parts[i]).toObject();
        const QJsonObject leafObj = curObj.value(parts.last()).toObject();
        if (!leafObj.isEmpty()) {
            hasMin = parseLimit(leafObj.value("min"), outMin);
            hasMax = parseLimit(leafObj.value("max"), outMax);
        }
    }

    if (!hasMin && !hasMax) {
        const QString leafKey = fullPath.contains('.') ? fullPath.split('.').last() : fullPath;
        if (MotionConfigManager::instance().hasParamMeta(leafKey)) {
            const auto m = MotionConfigManager::instance().paramMeta(leafKey);
            if (m.hasMin) {
                hasMin = true;
                outMin = m.minVal;
            }
            if (m.hasMax) {
                hasMax = true;
                outMax = m.maxVal;
            }
        }
    }

    return hasMin || hasMax;
}

// === [Section: Default Param Completion Helpers] ===
void ConfigMetadataHelper::ensureMotionControlConfig(QJsonArray &configs)
{
    ConfigDefaultCompleter::ensureMotionControlConfig(configs);
}

void ConfigDefaultCompleter_ensureAlign(QJsonArray &configs)
{
    ConfigDefaultCompleter::ensureAlignConfig(configs);
}

void ConfigMetadataHelper::ensureAlignConfig(QJsonArray &configs)
{
    ConfigDefaultCompleter::ensureAlignConfig(configs);
}

void ConfigMetadataHelper::ensureObstacleConfig(QJsonArray &configs)
{
    ConfigDefaultCompleter::ensureObstacleConfig(configs);
}
