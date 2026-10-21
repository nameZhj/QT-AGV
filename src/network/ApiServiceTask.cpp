/**
 * @file    ApiServiceTask.cpp
 * @layer   ProtocolComm
 * @brief   Task lifecycle RESTful APIs including task queue, task chain management, audio files, and logs.
 * @deps    ApiService.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "ApiService.h"

// ========== 任务基础与队列接口 (端口 8048) ==========

void ApiService::listTask(const QJsonObject &query, const Callback &cb)
{
    get(appendQuery("/robot/task/list", query), cb);
}

void ApiService::getTask(const QString &taskId, const Callback &cb)
{
    get("/robot/task/" + taskId, cb);
}

void ApiService::getTaskInfo(const QString &taskId, const Callback &cb)
{
    get("/robot/task/taskInfo/" + taskId, cb);
}

void ApiService::addTask(const QJsonObject &data, const Callback &cb)
{
    post("/robot/task", data, cb);
}

void ApiService::updateTask(const QJsonObject &data, const Callback &cb)
{
    put("/robot/task", data, cb);
}

void ApiService::delTask(const QString &taskId, const Callback &cb)
{
    del("/robot/task/" + taskId, cb);
}

void ApiService::listTaskAllStatus(const QString &carId, const Callback &cb)
{
    get("/robot/navigation/listAllStatus/" + carId, cb);
}

void ApiService::getCurrentTask(const QJsonObject &query, const Callback &cb)
{
    get(appendQuery("/robot/navigation/currentTask", query), cb);
}

void ApiService::loadTask(const QJsonObject &data, const Callback &cb)
{
    put("/robot/navigation/loadTaskQueue", data, cb);
}

void ApiService::startTask(const QJsonObject &data, const Callback &cb)
{
    post("/robot/navigation/start", data, cb);
}

void ApiService::startTask(const QString &taskId, const Callback &cb)
{
    startTask(QJsonObject{{"taskId", taskId}}, cb);
}

void ApiService::removeTask(const QJsonObject &data, const Callback &cb)
{
    put("/robot/navigation/quitTaskQueue", data, cb);
}

void ApiService::commandTask(const QJsonObject &data, const Callback &cb)
{
    post("/robot/navigation/command", data, cb);
}

// ========== 任务链接口 (端口 8048) ==========

void ApiService::saveTaskChain(const QString &filePath, const Callback &cb)
{
    uploadFile("/task/chain/save", filePath, "taskFile", cb);
}

void ApiService::batchSaveTaskChain(const QJsonArray &chains, const Callback &cb)
{
    postArray("/task/chain/batchSave", chains, cb);
}

void ApiService::deleteTaskChain(const QString &fileName, const Callback &cb)
{
    del("/task/chain/" + fileName, cb);
}

void ApiService::listTaskChain(const Callback &cb)
{
    get("/task/chain/list", cb);
}

void ApiService::sendTaskChain(const QString &taskChainString, bool enabled, bool looped, const Callback &cb)
{
    QJsonObject body;
    body["enabled"]         = enabled;
    body["looped"]          = looped;
    body["taskChainString"] = taskChainString;
    post("/task/chain/sendTask", body, cb);
}

void ApiService::sendTaskChain(const QString &taskChainString, bool enabled, const Callback &cb)
{
    sendTaskChain(taskChainString, enabled, false, cb);
}

void ApiService::controlTaskChain(const QString &command, const Callback &cb)
{
    put(appendQuery("/task/chain/control", QJsonObject{{"command", command}}), QJsonObject(), cb);
}

void ApiService::controlNavigation(const QString &command, const Callback &cb)
{
    put(appendQuery("/robot/navigation/control", QJsonObject{{"command", command}}), QJsonObject(), cb);
}

void ApiService::sendNextTask(const Callback &cb)
{
    sendNextTask(QJsonObject(), cb);
}

void ApiService::sendNextTask(const QJsonObject &data, const Callback &cb)
{
    post("/task/chain/sendNextTask", data, cb, 8048);
}

void ApiService::getTaskChainLog(const Callback &cb)
{
    get("/task/chain/taskLog", cb);
}

// ========== 音频接口 (端口 8048) ==========

void ApiService::listAudio(const Callback &cb)
{
    get("/robot/audio/list", cb);
}

void ApiService::uploadAudio(const QString &filePath, const Callback &cb)
{
    uploadFile("/robot/audio/upload", filePath, "file", cb);
}

void ApiService::downloadAudio(const QString &fileName, const FileCallback &cb)
{
    downloadFile("/robot/audio/download/" + fileName, cb);
}

void ApiService::deleteAudio(const QString &fileName, const Callback &cb)
{
    del("/robot/audio/" + fileName, cb);
}

// ========== 任务日志接口 ==========

void ApiService::listTasklog(const QJsonObject &query, const Callback &cb)
{
    get(appendQuery("/robot/tasklog/list", query), cb);
}

void ApiService::getTasklog(const QString &tasklogId, const Callback &cb)
{
    get("/robot/tasklog/" + tasklogId, cb);
}
