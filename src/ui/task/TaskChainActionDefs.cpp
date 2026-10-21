/**
 * @file    TaskChainActionDefs.cpp
 * @layer   GUILayer
 * @brief   Implementation of task chain action definitions and converters.
 * @deps    TaskChainActionDefs.h, TaskChainManager.h, MotionConfigManager.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "TaskChainActionDefs.h"
#include "core/TaskChainManager.h"
#include "core/MotionConfigManager.h"
#include "network/ApiService.h"
#include <QFileInfo>
#include <QEventLoop>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>

namespace TaskChainActionDefs {

QList<ActionDef> createDefaultDefs()
{
    QList<ActionDef> list;
    ActionDef def;

    // 1. 路径导航 (仅包含目标站点设置，选中后 target_station 赋值为点位 id)
    def = {"路径导航", "nav"};
    def.params = {
        {"target", "目标站点:", "station_combo", {}, "未选择时显示“未知”", 0.0},
    };
    list.append(def);

    // 2. 平动 (参数与“地图与控制-移动-平动”一致: 距离 / vx / vy)
    const auto &mc = MotionConfigManager::instance();
    def = {"平动", "motion"};
    def.params = {
        {"distance", "设置距离：", "double", {}, "m", 1.0},
        {"vx", "设置 vx 速度：", "double", {}, "m/s", mc.maxSpeed()},
        {"vy", "设置 vy 速度：", "double", {}, "m/s", 0.0},
    };
    list.append(def);

    // 3. 转动 (旋转角度非负, 旋转方向由角速度正负决定)
    def = {"转动", "motion"};
    def.params = {
        {"angle", "设置旋转角度：", "double", {}, "°", 90.0},
        {"speed", "设置角速度：", "double", {}, "°/s", mc.maxAngularSpeedDeg()},
    };
    list.append(def);

    // 4. 播放音频
    def = {"播放音频", "audio"};
    def.params = {
        {"audioName", "音频名称:", "audio_combo", {}, "选择音频文件", 0.0},
        {"loop", "循环播放:", "combo", {"否", "是"}, "", 0.0},
    };
    list.append(def);

    // 5. 顶升 (传目标高度 lift_height 与升降线速度 lift_linear, 高度 0-0.15 m, 速度上限 0.1 m/s)
    def = {"顶升", "jack"};
    def.params = {
        {"lift_height", "目标顶升高度:", "double", {}, "m", 0.05},
        {"lift_linear", "顶升线速度:", "double", {}, "m/s", 0.05},
    };
    list.append(def);

    // 6. 停止播放音频 (无需选择音频，直接传 action_type)
    def = {"停止播放音频", "audio"};
    def.params = {};
    list.append(def);

    return list;
}

ActionDef* findDef(QList<ActionDef> &defs, const QString &name)
{
    for (auto &def : defs) {
        if (def.name == name) return &def;
    }
    return nullptr;
}

const ActionDef* findDef(const QList<ActionDef> &defs, const QString &name)
{
    for (const auto &def : defs) {
        if (def.name == name) return &def;
    }
    return nullptr;
}

QString actionTypeFromName(const QString &name)
{
    return TaskChainManager::actionTypeFromName(name);
}

QString actionDescFromType(const QString &type)
{
    return TaskChainManager::actionDescFromType(type);
}

QString normalizeActionType(const QString &type, const QString &name)
{
    return TaskChainManager::normalizeActionType(type, name);
}

QString audioDisplayName(const QString &fullName)
{
    const QString base = QFileInfo(fullName).completeBaseName();
    const int idx = base.lastIndexOf('-');
    return idx > 0 ? base.left(idx) : base;
}

QStringList fetchAudioFiles(int timeoutMs)
{
    QStringList files;
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    ApiService::instance().listAudio([&](int code, const QJsonObject &resp) {
        if (code == 200) {
            for (const QJsonValue &v : resp.value("data").toArray()) {
                const QString name = QFileInfo(v.toString().trimmed()).fileName();
                if (!name.isEmpty()) files << name;
            }
        }
        loop.quit();
    });

    timer.start(timeoutMs);
    loop.exec();
    return files;
}

bool isActionNoParam(const ActionDef *def, const QString &actionName)
{
    if (actionName == QStringLiteral("停止播放音频")) return true;
    return def && def->params.isEmpty();
}

QString formatParamSummary(const QString &actionName,
                           const QJsonObject &params,
                           const std::function<QString(const QString&)> &stationNameResolver)
{
    QStringList parts;
    bool audioShown = false;
    for (auto it = params.begin(); it != params.end(); ++it) {
        const QString &key = it.key();
        if (key == "target" || key == "checked" || key == "id") continue;
        if (key == "audioName" || key == "audio_name") {
            if (audioShown) continue;
            const QString audio = it.value().toString();
            if (audio.isEmpty()) continue;
            audioShown = true;
            parts << QStringLiteral("音频名称: %1").arg(audioDisplayName(audio));
        } else if (key == "loop") {
            parts << QStringLiteral("循环播放: %1").arg(it.value().toString().compare("yes", Qt::CaseInsensitive) == 0 ? "是" : "否");
        } else if (key == "target_station") {
            const QString st = stationNameResolver ? stationNameResolver(it.value().toString()) : it.value().toString();
            parts << QStringLiteral("目标站点: %1").arg(st);
        } else {
            parts << QStringLiteral("%1: %2").arg(key, it.value().toVariant().toString());
        }
    }
    return parts.isEmpty() ? QStringLiteral("未设置 — 点击编辑") : parts.join(", ");
}

} // namespace TaskChainActionDefs

