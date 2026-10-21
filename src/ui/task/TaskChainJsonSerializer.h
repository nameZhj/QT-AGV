/**
 * @file    TaskChainJsonSerializer.h
 * @layer   GUI
 * @brief   Persistence, cache management, and JSON serialization for TaskChain.
 * @deps    core/TaskChainManager.h, TaskActionParamDialog.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QString>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include "core/TaskChainManager.h"
#include "TaskActionParamDialog.h"

namespace TaskChainJsonSerializer {

QString cacheFilePath();
QString taskChainLocalDir();

QList<TaskChainData> readLocalCache(const QString &filePath = QString());
bool writeLocalCache(const QList<TaskChainData> &chains,
                     const QList<TaskChainStationItem> &stations,
                     const QString &filePath = QString());

bool saveChainsToLocalDir(const QList<TaskChainData> &chains,
                          const QList<TaskChainStationItem> &stations,
                          int *savedCount = nullptr);

bool loadChainFromFile(const QString &filePath,
                       TaskChainData *outChain,
                       QString *outError = nullptr);

bool loadChainsFromFiles(const QStringList &filePaths,
                         QList<TaskChainData> *outChains,
                         int *outSuccessFileCount = nullptr);

bool exportChainToFile(const TaskChainData &chain,
                       const QString &filePath,
                       const QList<TaskChainStationItem> &stations,
                       QString *outError = nullptr);

struct ServerParseResult {
    QList<TaskChainData> chains;
    int added = 0;
    int updated = 0;
};
ServerParseResult mergeServerChains(const QJsonObject &resp, QList<TaskChainData> &existingChains);

QJsonArray buildBatchSavePayload(const QList<TaskChainData> &chains,
                                 const QList<TaskChainStationItem> &stations);

QJsonObject exportChainToJson(const TaskChainData &chain,
                              bool onlyChecked,
                              bool forExecution,
                              const QList<TaskChainStationItem> &stations);

TaskChainData importChainFromJson(const QJsonObject &json);

} // namespace TaskChainJsonSerializer
