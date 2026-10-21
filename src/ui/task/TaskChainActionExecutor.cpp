/**
 * @file    TaskChainActionExecutor.cpp
 * @layer   GUI
 * @brief   Implementation of single action execution and batch task chain execution requests.
 * @deps    TaskChainActionExecutor.h, TaskChainWidget.h, ApiService.h, TaskChainManager.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "TaskChainActionExecutor.h"
#include "TaskChainWidget.h"
#include "core/TaskChainManager.h"
#include "core/MotionConfigManager.h"
#include "network/ApiService.h"
#include "app/AppConfig.h"
#include "ui/common/Toast.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QMessageBox>
#include <QCheckBox>
#include <QDateTime>
#include <cmath>

static constexpr double kMaxLiftLinearSpeed = TaskChainManager::kMaxLiftLinearSpeed;
static constexpr double kMaxLiftHeight      = TaskChainManager::kMaxLiftHeight;

void TaskChainActionExecutor::executeSingleAction(TaskChainWidget *owner, const QJsonObject &action)
{
    if (!owner) return;
    const QString type = action.value("action_type").toString();
    const int actionId = static_cast<int>(QDateTime::currentMSecsSinceEpoch() % 1000000 + 1);

    if (type == "99") {
        const QString targetStation = action.value("target_station").toString();
        if (targetStation.isEmpty()) {
            owner->updateStatus(QStringLiteral("路径导航缺少目标站点, 已跳过该动作"));
            Toast::error(owner, QStringLiteral("路径导航未设置目标站点, 请先选择站点"));
            return;
        }

        QJsonObject fixPath;
        QJsonObject d;
        d["action_id"] = actionId;
        d["mapcode"]   = AppConfig::instance().currentDevice().mapName;

        QJsonArray paths;
        QJsonObject p;
        p["start_node"] = 1;
        bool ok = false;
        int goalNodeInt = targetStation.toInt(&ok);
        p["goal_node"]  = ok ? QJsonValue(goalNodeInt) : QJsonValue(targetStation);
        p["index"]      = 1;
        double navSpd   = action.value("speed").toDouble(0.5);
        p["path_speed"] = MotionConfigManager::instance().clampLinearSpeed(navSpd <= 0.01 ? MotionConfigManager::instance().maxSpeed() : navSpd);
        paths.append(p);
        d["robot_paths"] = paths;
        fixPath["data"]  = d;

        ApiService::instance().setRobotFixPath(fixPath, [targetStation](int code, const QJsonObject &resp) {
            qDebug() << "[TaskChain] setRobotFixPath to" << targetStation << "code:" << code << resp;
        });
        owner->updateStatus(QStringLiteral("正在向控制器下发路径导航: 目标站点 %1").arg(targetStation));
    } else if (type == "6") {
        const auto &mc = MotionConfigManager::instance();
        double vx   = mc.clampSignedLinearSpeed(action.value("vx").toDouble(0.2));
        double vy   = mc.clampSignedLinearSpeed(action.value("vy").toDouble(0.0));
        double dist = std::max(0.0, action.value("distance").toDouble(1.0));

        ApiService::instance().setLinearMotion(vx, vy, dist, actionId, [vx, vy, dist](int code, const QJsonObject &resp) {
            qDebug() << "[TaskChain] setLinearMotion (vx" << vx << "vy" << vy << dist << "m) code:" << code << resp;
        });
        owner->updateStatus(QStringLiteral("正在向控制器下发平动动作: vx %1 m/s, vy %2 m/s, 距离 %3 m").arg(vx).arg(vy).arg(dist));
    } else if (type == "5") {
        const auto &mc  = MotionConfigManager::instance();
        double deg      = std::max(0.0, action.value("rotate_angle").toDouble(90.0));
        double spdDeg   = mc.clampSignedAngularSpeedDeg(action.value("rotate_speed").toDouble(mc.maxAngularSpeedDeg()));
        double spd      = spdDeg * M_PI / 180.0;
        double angleRad = deg * M_PI / 180.0;

        ApiService::instance().setRotateMotion(angleRad, spd, actionId, [angleRad, deg](int code, const QJsonObject &resp) {
            qDebug() << "[TaskChain] setRotateMotion (" << deg << "° =" << angleRad << "rad) code:" << code << resp;
        });
        owner->updateStatus(QStringLiteral("正在向控制器下发转动动作: %1° (角速度 %2°/s)").arg(deg).arg(spdDeg));
    } else if (type == "3") {
        double h = qBound(0.0, action.value("lift_height").toDouble(0.05), kMaxLiftHeight);
        double v = qBound(0.0, action.value("lift_linear").toDouble(0.05), kMaxLiftLinearSpeed);

        ApiService::instance().setJackControl(h, v, actionId, [h, v](int code, const QJsonObject &resp) {
            qDebug() << "[TaskChain] setJackControl (lift_height" << h << "lift_linear" << v << ") code:" << code << resp;
        });
        owner->updateStatus(QStringLiteral("正在向控制器下发顶升动作: 目标高度 %1m, 线速度 %2m/s").arg(h).arg(v));
    }
}

void TaskChainActionExecutor::sendTaskChain(TaskChainWidget *owner, bool onlyChecked)
{
    if (!owner) return;
    const int idx = owner->currentChainIndex();
    if (idx < 0 || idx >= owner->m_chains.size()) {
        QMessageBox::warning(owner, QStringLiteral("提示"), QStringLiteral("请先选择任务链"));
        return;
    }

    const QJsonObject chainJson = owner->exportChainToJson(owner->m_chains[idx], onlyChecked, true);
    const QJsonArray subTasks   = chainJson.value("sub_task_info").toArray();
    if (subTasks.isEmpty()) {
        QMessageBox::warning(owner, QStringLiteral("提示"),
            onlyChecked ? QStringLiteral("未勾选有效任务或任务链数据为空") : QStringLiteral("任务链数据为空"));
        return;
    }

    const QString chainString = QString::fromUtf8(QJsonDocument(chainJson).toJson(QJsonDocument::Compact));
    const bool looped = (owner->m_loopCheck && owner->m_loopCheck->isChecked());

    owner->m_execStatus = TaskChainWidget::Running;
    owner->setListsLocked(true);
    TaskChainManager::instance().setLocalExecutionActive(true);
    owner->updateBottomBar();
    owner->updateStatus(QStringLiteral("正在下发任务链 (%1, %2): %3")
        .arg(onlyChecked ? QStringLiteral("勾选子任务") : QStringLiteral("全部子任务"))
        .arg(looped ? QStringLiteral("循环模式") : QStringLiteral("单次模式"))
        .arg(owner->m_chains[idx].name));

    ApiService::instance().sendTaskChain(chainString, true, looped, [owner, onlyChecked](int code, const QJsonObject &resp) {
        if (!owner) return;
        if (code == 200) {
            Toast::success(owner, onlyChecked ? QStringLiteral("任务链数据已成功下发至设备") : QStringLiteral("全部任务链数据已成功下发至设备"));
        } else {
            owner->m_execStatus = TaskChainWidget::NotStarted;
            owner->setListsLocked(false);
            TaskChainManager::instance().resetExecutionState();
            owner->updateBottomBar();
            const QString err = resp.value("msg").toString(QStringLiteral("请求失败"));
            owner->updateStatus(QStringLiteral("任务链下发失败: %1").arg(err));
            Toast::error(owner, QStringLiteral("任务链下发失败(%1): %2").arg(code).arg(err));
        }
    });
}
