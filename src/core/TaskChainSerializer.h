/**
 * @file    TaskChainSerializer.h
 * @layer   CoreTopology
 * @brief   JSON serialization and action protocol conversion for TaskChainData.
 * @deps    TaskChainManager.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QList>
#include <QPair>

struct TaskChainData;

class TaskChainSerializer
{
public:
    static constexpr double kMaxLiftLinearSpeed = 0.1;
    static constexpr double kMaxLiftHeight = 0.15;

    static QString actionTypeFromName(const QString &name);
    static QString actionDescFromType(const QString &type);
    static QString normalizeActionType(const QString &type, const QString &name);

    static QJsonObject exportChainToJson(const TaskChainData &chain,
                                         bool onlyChecked = false,
                                         bool forExecution = false,
                                         const QList<QPair<QString, QString>> &stations = {});
    static TaskChainData importChainFromJson(const QJsonObject &json);
};
