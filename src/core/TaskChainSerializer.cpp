/**
 * @file    TaskChainSerializer.cpp
 * @layer   CoreTopology
 * @brief   Implementation of TaskChainData import/export and action type normalizing.
 * @deps    TaskChainSerializer.h, TaskChainManager.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "TaskChainSerializer.h"
#include "TaskChainManager.h"

#include <cmath>
#include <QVariant>

QString TaskChainSerializer::actionTypeFromName(const QString &name)
{
    if (name == "路径导航") return "99";
    if (name == "顶升") return "3";
    if (name == "转动") return "5";
    if (name == "平动") return "6";
    if (name == "播放音频") return "10";
    if (name == "停止播放音频") return "11";
    return "-1";
}

QString TaskChainSerializer::actionDescFromType(const QString &type)
{
    if (type == "99") return "路径导航";
    if (type == "3" ) return "顶升";     
    if (type == "5" || type == "12") return "转动";     
    if (type == "6" ) return "平动";  
    if (type == "10") return "播放音频";
    if (type == "11") return "停止播放音频";
    return "路径导航";
}

QString TaskChainSerializer::normalizeActionType(const QString &type, const QString &name)
{
    if (type == "0") return "99";
    if (type == "1") return "6";
    if (type == "2" || type == "12") return "5";
    if (type == "4") return "3";
    if (type == "3" && name != "顶升") return "10";
    if (type == "5" && name == "停止播放音频") return "11";
    return type;
}

QJsonObject TaskChainSerializer::exportChainToJson(const TaskChainData &chain,
                                                   bool onlyChecked,
                                                   bool forExecution,
                                                   const QList<QPair<QString, QString>> &stations)
{
    QJsonObject root;
    root["task_name"] = chain.name;

    QJsonArray subTaskInfo;
    for (int i = 0; i < chain.tasks.size(); ++i) {
        QJsonObject taskObj = chain.tasks[i].toObject();
        bool enable = taskObj.contains("enable") ? taskObj.value("enable").toBool() : taskObj.value("checked").toBool();
        if (onlyChecked && !enable) continue;

        QJsonObject subTask;
        QString subTaskName = taskObj.value("sub_task_name").toString();
        if (subTaskName.isEmpty()) subTaskName = taskObj.value("name").toString();
        if (subTaskName.isEmpty()) subTaskName = QString("sub_task_%1").arg(i + 1);

        subTask["sub_task_name"] = subTaskName;
        subTask["enable"]        = forExecution ? true : enable;

        QJsonArray groupInfo;
        QJsonArray rawGroups = taskObj.value("groups").isArray()
            ? taskObj.value("groups").toArray()
            : taskObj.value("group_info").toArray();

        for (int g = 0; g < rawGroups.size(); ++g) {
            QJsonObject rawGroup = rawGroups[g].toObject();
            bool groupEnable = rawGroup.contains("enable") ? rawGroup.value("enable").toBool() : rawGroup.value("checked").toBool();
            if (onlyChecked && !groupEnable) continue;

            QJsonObject group;
            QString groupName = rawGroup.value("group_name").toString();
            if (groupName.isEmpty()) groupName = rawGroup.value("name").toString();
            if (groupName.isEmpty()) groupName = QString("group_%1").arg(g + 1);

            group["group_name"] = groupName;
            group["enable"]     = forExecution ? true : groupEnable;

            QJsonArray actionsOut;
            QJsonArray rawActions = rawGroup.value("actions").toArray();
            for (int a = 0; a < rawActions.size(); ++a) {
                QJsonObject rawAction = rawActions[a].toObject();
                QJsonObject action;

                QString name = rawAction.value("name").toString();
                if (name.isEmpty()) name = rawAction.value("action_desc").toString();

                QString actionType = rawAction.value("action_type").toString();
                if (actionType.isEmpty()) actionType = actionTypeFromName(name);
                actionType = normalizeActionType(actionType, name);

                action["action_type"] = actionType;
                action["action_desc"] = name.isEmpty() ? actionDescFromType(actionType) : name;

                QJsonObject params = rawAction.value("params").toObject();

                if (actionType == "99") {
                    QString station = params.value("target_station").toString();
                    if (station.isEmpty()) station = rawAction.value("target_station").toString();
                    if (station.isEmpty()) station = params.value("target").toString();
                    if (station.isEmpty()) station = rawAction.value("target").toString();
                    for (const auto &st : stations) {
                        if (st.first == station) { station = st.second; break; }
                    }
                    if (station == QStringLiteral("未知")) station.clear();
                    action["target_station"] = station;
                } else if (actionType == "6") {
                    action["distance"] = params.contains("distance") ? params.value("distance").toDouble() : rawAction.value("distance").toDouble(1.0);
                    action["vx"]       = params.contains("vx") ? params.value("vx").toDouble() : rawAction.value("vx").toDouble(0.2);
                    action["vy"]       = params.contains("vy") ? params.value("vy").toDouble() : rawAction.value("vy").toDouble(0.0);
                } else if (actionType == "5") {
                    action["rotate_angle"] = params.contains("angle") ? params.value("angle").toDouble() : rawAction.value("angle").toDouble(-90.0);
                    action["rotate_speed"] = params.contains("speed") ? params.value("speed").toDouble() : rawAction.value("speed").toDouble(30.0);
                } else if (actionType == "10") {
                    QString audioName = params.value("audio_name").toString();
                    if (audioName.isEmpty()) audioName = params.value("audioName").toString();
                    if (audioName.isEmpty()) audioName = rawAction.value("audio_name").toString();
                    if (audioName.isEmpty()) audioName = rawAction.value("audioName").toString();
                    action["audio_name"] = audioName;
                    QString loopVal = params.contains("loop") ? params.value("loop").toVariant().toString()
                                                              : rawAction.value("loop").toVariant().toString();
                    if (loopVal.isEmpty()) loopVal = "no";
                    if (loopVal == "是" || loopVal.compare("true", Qt::CaseInsensitive) == 0) loopVal = "yes";
                    else if (loopVal == "否" || loopVal.compare("false", Qt::CaseInsensitive) == 0) loopVal = "no";
                    action["loop"] = loopVal;
                } else if (actionType == "3") {
                    double h = params.contains("lift_height") ? params.value("lift_height").toDouble()
                                                              : rawAction.value("lift_height").toDouble(0.05);
                    double v = params.contains("lift_linear") ? params.value("lift_linear").toDouble()
                                                              : rawAction.value("lift_linear").toDouble(0.05);
                    action["lift_height"] = qBound(0.0, h, kMaxLiftHeight);
                    action["lift_linear"] = qBound(0.0, v, kMaxLiftLinearSpeed);
                } else if (actionType == "11") {
                    // 停止播放音频直接传 action_type
                } else {
                    for (auto it = params.begin(); it != params.end(); ++it) {
                        if (it.key() != "checked" && it.key() != "target" && it.key() != "id") {
                            action[it.key()] = it.value();
                        }
                    }
                }
                actionsOut.append(action);
            }
            group["actions"] = actionsOut;
            groupInfo.append(group);
        }

        if (forExecution && onlyChecked && groupInfo.isEmpty()) continue;
        subTask["group_info"] = groupInfo;
        subTaskInfo.append(subTask);
    }

    root["sub_task_info"] = subTaskInfo;
    return root;
}

TaskChainData TaskChainSerializer::importChainFromJson(const QJsonObject &json)
{
    TaskChainData chain;
    chain.name = json.value("task_name").toString();
    if (chain.name.isEmpty()) chain.name = json.value("name").toString();

    QJsonArray subTaskInfo = json.value("sub_task_info").isArray()
        ? json.value("sub_task_info").toArray()
        : json.value("tasks").toArray();

    for (int i = 0; i < subTaskInfo.size(); ++i) {
        QJsonObject rawSub = subTaskInfo[i].toObject();
        QJsonObject task;

        QString subTaskName = rawSub.value("sub_task_name").toString();
        if (subTaskName.isEmpty()) subTaskName = rawSub.value("name").toString();
        if (subTaskName.isEmpty()) subTaskName = QString("sub_task_%1").arg(i + 1);

        bool enable = rawSub.contains("enable") ? rawSub.value("enable").toBool() : rawSub.value("checked").toBool();

        task["sub_task_name"] = subTaskName;
        task["name"]          = subTaskName;
        task["enable"]        = enable;
        task["checked"]       = enable;
        task["note"]          = rawSub.value("note").toString();

        QJsonArray groupInfo = rawSub.value("group_info").isArray()
            ? rawSub.value("group_info").toArray()
            : rawSub.value("groups").toArray();

        QJsonArray outGroups;
        for (int g = 0; g < groupInfo.size(); ++g) {
            QJsonObject rawGroup = groupInfo[g].toObject();
            QJsonObject group;

            QString groupName = rawGroup.value("group_name").toString();
            if (groupName.isEmpty()) groupName = rawGroup.value("name").toString();
            if (groupName.isEmpty()) groupName = QString("group_%1").arg(g + 1);

            bool groupEnable = rawGroup.contains("enable") ? rawGroup.value("enable").toBool() : rawGroup.value("checked").toBool();

            group["group_name"] = groupName;
            group["name"]       = groupName;
            group["enable"]     = groupEnable;
            group["checked"]    = groupEnable;

            QJsonArray actionsIn = rawGroup.value("actions").toArray();
            QJsonArray outActions;
            for (int a = 0; a < actionsIn.size(); ++a) {
                QJsonObject rawAction = actionsIn[a].toObject();
                QJsonObject action;

                QString actionDesc = rawAction.value("action_desc").toString();
                if (actionDesc.isEmpty()) actionDesc = rawAction.value("name").toString();

                QString actionType = rawAction.value("action_type").toString();
                if (actionType.isEmpty()) actionType = actionTypeFromName(actionDesc);
                actionType = normalizeActionType(actionType, actionDesc);
                if (actionDesc.isEmpty()) actionDesc = actionDescFromType(actionType);

                action["name"]        = actionDesc;
                action["action_desc"] = actionDesc;
                action["action_type"] = actionType;

                QJsonObject params;
                if (actionType != "11") {
                    for (auto it = rawAction.begin(); it != rawAction.end(); ++it) {
                        if (it.key() != "name" && it.key() != "action_desc" && it.key() != "action_type") {
                            params[it.key()] = it.value();
                            if (it.key() == "target_station") params["target"] = it.value();
                            if (it.key() == "audio_name") params["audioName"] = it.value();
                            if (it.key() == "rotate_angle") params["angle"] = it.value();
                            if (it.key() == "rotate_speed") params["speed"] = it.value();
                        }
                    }
                }
                if (actionType == "6" && !params.contains("vx") && params.contains("direction")) {
                    const QString dir = params.value("direction").toString();
                    const double  spd = params.value("speed").toDouble(0.2);
                    double dist = params.value("distance").toDouble(1.0);
                    params.remove("direction");
                    params.remove("speed");
                    if (dir == "左移") {
                        params["vx"] = 0.0; params["vy"] = spd;
                    } else if (dir == "右移") {
                        params["vx"] = 0.0; params["vy"] = -spd;
                    } else if (dir == "后退") {
                        params["vx"] = -spd; params["vy"] = 0.0;
                    } else {
                        params["vx"] = spd; params["vy"] = 0.0;
                    }
                    params["distance"] = std::abs(dist);
                }
                params.remove("rotate_angle");
                params.remove("rotate_speed");
                if (actionType == "3" && !params.contains("lift_height") && params.contains("height")) {
                    params["lift_height"] = params.value("height").toDouble(0.05);
                    params["lift_linear"] = kMaxLiftLinearSpeed;
                }
                if (actionType == "3") {
                    params.remove("operation");
                    params.remove("height");
                    params["lift_height"] = qBound(0.0, params.value("lift_height").toDouble(0.05), kMaxLiftHeight);
                    params["lift_linear"] = qBound(0.0, params.value("lift_linear").toDouble(0.05), kMaxLiftLinearSpeed);
                }
                action["params"] = params;
                outActions.append(action);
            }
            group["actions"] = outActions;
            if (!outActions.isEmpty()) {
                group["actionType"] = outActions[0].toObject().value("name").toString();
            }
            outGroups.append(group);
        }

        task["group_info"] = outGroups;
        task["groups"]     = outGroups;
        chain.tasks.append(task);
    }

    return chain;
}
