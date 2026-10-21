/**
 * @file    TaskChainActionDefs.h
 * @layer   GUILayer
 * @brief   Action definitions, parameter schemas, and type normalizers for task chain.
 * @deps    QString, QList, QJsonObject
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <QJsonObject>
#include <functional>

// 动作标签定义: 标签名 + 类别 + 参数定义字段
struct ActionDef {
    QString name;       // 显示名, 如 "移动到目标点"
    QString category;   // 类别, 用于同组去重
    struct ParamDef {
        QString key;
        QString label;
        QString type;   // "text", "double", "int", "combo", "station_combo", "audio_combo"
        QStringList options;
        QString unit;
        double defaultValue = 0.0;
    };
    QList<ParamDef> params;
};

namespace TaskChainActionDefs {

QList<ActionDef> createDefaultDefs();
ActionDef* findDef(QList<ActionDef> &defs, const QString &name);
const ActionDef* findDef(const QList<ActionDef> &defs, const QString &name);

QString actionTypeFromName(const QString &name);
QString actionDescFromType(const QString &type);
QString normalizeActionType(const QString &type, const QString &name);

QString audioDisplayName(const QString &fullName);
QStringList fetchAudioFiles(int timeoutMs = 2500);

bool isActionNoParam(const ActionDef *def, const QString &actionName);
QString formatParamSummary(const QString &actionName,
                           const QJsonObject &params,
                           const std::function<QString(const QString&)> &stationNameResolver);

} // namespace TaskChainActionDefs

