/**
 * @file    TaskChainJsonSerializer.cpp
 * @layer   GUI
 * @brief   Implementation of TaskChainJsonSerializer.
 * @deps    TaskChainJsonSerializer.h, core/TaskChainManager.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "TaskChainJsonSerializer.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QPair>

namespace TaskChainJsonSerializer {

QString cacheFilePath()
{
    return QCoreApplication::applicationDirPath() + "/taskChainCache.json";
}

QString taskChainLocalDir()
{
    QString dir = QCoreApplication::applicationDirPath() + "/task_chains";
    QDir().mkpath(dir);
    return dir;
}

QJsonObject exportChainToJson(const TaskChainData &chain,
                              bool onlyChecked,
                              bool forExecution,
                              const QList<TaskChainStationItem> &stations)
{
    QList<QPair<QString, QString>> stationPairs;
    stationPairs.reserve(stations.size());
    for (const auto &st : stations) {
        stationPairs.append({st.name, st.id});
    }
    return TaskChainManager::exportChainToJson(chain, onlyChecked, forExecution, stationPairs);
}

TaskChainData importChainFromJson(const QJsonObject &json)
{
    return TaskChainManager::importChainFromJson(json);
}

QList<TaskChainData> readLocalCache(const QString &filePath)
{
    QList<TaskChainData> result;
    const QString path = filePath.isEmpty() ? cacheFilePath() : filePath;
    QFile cache(path);
    if (!cache.open(QIODevice::ReadOnly)) return result;

    const QJsonDocument doc = QJsonDocument::fromJson(cache.readAll());
    cache.close();
    if (doc.isArray()) {
        for (const QJsonValue &v : doc.array()) {
            if (!v.isObject()) continue;
            TaskChainData chain = importChainFromJson(v.toObject());
            if (!chain.name.isEmpty()) result.append(chain);
        }
    }
    return result;
}

bool writeLocalCache(const QList<TaskChainData> &chains,
                     const QList<TaskChainStationItem> &stations,
                     const QString &filePath)
{
    QJsonArray arr;
    for (const auto &chain : chains) {
        arr.append(exportChainToJson(chain, false, false, stations));
    }
    const QString path = filePath.isEmpty() ? cacheFilePath() : filePath;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    file.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    file.close();
    return true;
}

bool saveChainsToLocalDir(const QList<TaskChainData> &chains,
                          const QList<TaskChainStationItem> &stations,
                          int *savedCount)
{
    QString localDir = taskChainLocalDir();
    int count = 0;
    for (const auto &chain : chains) {
        QString fileName = chain.name.trimmed();
        if (fileName.isEmpty()) continue;
        if (!fileName.endsWith(".json", Qt::CaseInsensitive)) fileName += ".json";

        QFile localFile(localDir + "/" + fileName);
        if (localFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QJsonDocument doc(exportChainToJson(chain, false, false, stations));
            localFile.write(doc.toJson(QJsonDocument::Indented));
            localFile.close();
            count++;
        }
    }
    if (savedCount) *savedCount = count;
    return count > 0 || chains.isEmpty();
}

bool loadChainFromFile(const QString &filePath,
                       TaskChainData *outChain,
                       QString *outError)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (outError) *outError = QStringLiteral("无法打开所选 JSON 文件");
        return false;
    }
    QByteArray data = file.readAll();
    file.close();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        if (outError) *outError = QStringLiteral("JSON 格式错误: %1").arg(err.errorString());
        return false;
    }

    if (outChain) {
        *outChain = importChainFromJson(doc.object());
        if (outChain->name.isEmpty()) {
            outChain->name = QFileInfo(filePath).baseName();
        }
    }
    return true;
}

bool loadChainsFromFiles(const QStringList &filePaths,
                         QList<TaskChainData> *outChains,
                         int *outSuccessFileCount)
{
    if (!outChains) return false;
    int successCount = 0;
    for (const QString &fp : filePaths) {
        QFile file(fp);
        if (!file.open(QIODevice::ReadOnly)) continue;
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
        file.close();
        if (err.error != QJsonParseError::NoError) continue;

        bool hasChain = false;
        auto addChain = [&](const QJsonObject &obj, int idx) {
            TaskChainData c = importChainFromJson(obj);
            if (c.name.isEmpty()) c.name = QString("%1_%2").arg(QFileInfo(fp).baseName()).arg(idx + 1);
            outChains->append(c);
            hasChain = true;
        };

        if (doc.isArray()) {
            QJsonArray arr = doc.array();
            for (int i = 0; i < arr.size(); ++i) if (arr[i].isObject()) addChain(arr[i].toObject(), i);
        } else if (doc.isObject()) {
            QJsonObject root = doc.object();
            if (root.contains("chains") && root["chains"].isArray()) {
                QJsonArray arr = root["chains"].toArray();
                for (int i = 0; i < arr.size(); ++i) if (arr[i].isObject()) addChain(arr[i].toObject(), i);
            } else if (root.contains("task_chains") && root["task_chains"].isArray()) {
                QJsonArray arr = root["task_chains"].toArray();
                for (int i = 0; i < arr.size(); ++i) if (arr[i].isObject()) addChain(arr[i].toObject(), i);
            } else {
                TaskChainData c = importChainFromJson(root);
                if (c.name.isEmpty()) c.name = QFileInfo(fp).baseName();
                outChains->append(c);
                hasChain = true;
            }
        }
        if (hasChain) successCount++;
    }
    if (outSuccessFileCount) *outSuccessFileCount = successCount;
    return !outChains->isEmpty();
}

bool exportChainToFile(const TaskChainData &chain,
                       const QString &filePath,
                       const QList<TaskChainStationItem> &stations,
                       QString *outError)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (outError) *outError = file.errorString();
        return false;
    }
    QJsonObject obj = exportChainToJson(chain, false, false, stations);
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

ServerParseResult mergeServerChains(const QJsonObject &resp, QList<TaskChainData> &existingChains)
{
    ServerParseResult res;
    for (const QJsonValue &row : resp.value("data").toArray()) {
        const QJsonObject obj = row.toObject();
        QString fileName = obj.value("fileName").toString();
        if (fileName.isEmpty()) continue;
        if (fileName.endsWith(".json", Qt::CaseInsensitive)) fileName.chop(5);

        QJsonObject chainJsonObj;
        if (obj.value("content").isObject()) {
            chainJsonObj = obj.value("content").toObject();
        } else if (obj.value("fileContent").isObject()) {
            chainJsonObj = obj.value("fileContent").toObject();
        } else {
            QString rawStr = obj.value("content").toString();
            if (rawStr.isEmpty()) rawStr = obj.value("fileContent").toString();
            if (!rawStr.isEmpty()) {
                QJsonDocument doc = QJsonDocument::fromJson(rawStr.toUtf8());
                if (doc.isObject()) chainJsonObj = doc.object();
            }
        }
        if (chainJsonObj.isEmpty()) continue;

        TaskChainData chain = importChainFromJson(chainJsonObj);
        if (chain.name.isEmpty()) chain.name = fileName;

        int existIdx = -1;
        for (int i = 0; i < existingChains.size(); ++i) {
            if (existingChains[i].name == chain.name || existingChains[i].name == fileName) {
                existIdx = i;
                break;
            }
        }

        if (existIdx >= 0) {
            existingChains[existIdx] = chain;
            ++res.updated;
        } else {
            existingChains.append(chain);
            ++res.added;
        }
    }
    res.chains = existingChains;
    return res;
}

QJsonArray buildBatchSavePayload(const QList<TaskChainData> &chains,
                                 const QList<TaskChainStationItem> &stations)
{
    QJsonArray array;
    for (const auto &chain : chains) {
        QJsonObject item;
        item["fileName"] = chain.name;
        item["fileContent"] = exportChainToJson(chain, false, false, stations);
        array.append(item);
    }
    return array;
}

} // namespace TaskChainJsonSerializer
