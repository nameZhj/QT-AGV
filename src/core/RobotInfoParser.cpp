/**
 * @file    RobotInfoParser.cpp
 * @layer   CoreScheduling
 * @brief   Implements RobotInfo telemetry JSON parsing and duration formatting.
 * @deps    RobotInfoParser.h, RobotInfoManager.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */
#include "RobotInfoParser.h"
#include "RobotInfoManager.h"
#include <QJsonValue>
#include <QStringList>
#include <cmath>

namespace {
double extractNum(const QJsonObject &obj, const QStringList &keys, double defVal, bool *found = nullptr)
{
    for (const QString &k : keys) {
        if (obj.contains(k) && !obj.value(k).isNull()) {
            QJsonValue v = obj.value(k);
            if (v.isDouble()) {
                if (found) *found = true;
                return v.toDouble();
            } else if (v.isString()) {
                bool ok = false;
                double dVal = v.toString().trimmed().toDouble(&ok);
                if (ok) {
                    if (found) *found = true;
                    return dVal;
                }
            }
        }
    }
    if (found) *found = false;
    return defVal;
}
} // namespace

bool RobotInfoParser::parseInfoJson(const QJsonObject &d, RobotInfo *outInfo)
{
    if (!outInfo) return false;

    // 1. car_id
    if (d.contains(QStringLiteral("car_id")) && !d.value(QStringLiteral("car_id")).isNull()) {
        QJsonValue cv = d.value(QStringLiteral("car_id"));
        if (cv.isDouble()) outInfo->car_id = cv.toInt();
        else if (cv.isString()) outInfo->car_id = cv.toString().toInt();
        outInfo->carIdStr = QString::number(outInfo->car_id);
    }

    // 2. map_id
    if (d.contains(QStringLiteral("map_id")) && !d.value(QStringLiteral("map_id")).isNull()) {
        QJsonValue mv = d.value(QStringLiteral("map_id"));
        if (mv.isDouble()) outInfo->map_id = mv.toInt();
        else if (mv.isString()) outInfo->map_id = mv.toString().toInt();
        outInfo->mapIdStr = QString::number(outInfo->map_id);
    } else if (d.contains(QStringLiteral("mapId")) && !d.value(QStringLiteral("mapId")).isNull()) {
        QJsonValue mv = d.value(QStringLiteral("mapId"));
        if (mv.isDouble()) outInfo->map_id = mv.toInt();
        else if (mv.isString()) outInfo->map_id = mv.toString().toInt();
        outInfo->mapIdStr = QString::number(outInfo->map_id);
    }

    // 3. car_x, car_y, car_z
    bool hasX = false, hasY = false;
    double nx = extractNum(d, {QStringLiteral("car_x"), QStringLiteral("carX"), QStringLiteral("x"), QStringLiteral("current_x"), QStringLiteral("posX")}, outInfo->car_x, &hasX);
    double ny = extractNum(d, {QStringLiteral("car_y"), QStringLiteral("carY"), QStringLiteral("y"), QStringLiteral("current_y"), QStringLiteral("posY")}, outInfo->car_y, &hasY);
    if (hasX || hasY) {
        outInfo->car_x = nx;
        outInfo->car_y = ny;
        outInfo->hasPose = true;
    }
    outInfo->car_z = extractNum(d, {QStringLiteral("car_z"), QStringLiteral("carZ"), QStringLiteral("z")}, outInfo->car_z);

    // 4. car_yaw
    bool hasYaw = false;
    double nyaw = extractNum(d, {QStringLiteral("car_yaw"), QStringLiteral("carYaw"), QStringLiteral("yaw"), QStringLiteral("carTheta"), QStringLiteral("theta"), QStringLiteral("angle")}, outInfo->car_yaw, &hasYaw);
    if (hasYaw) {
        outInfo->car_yaw = nyaw;
        outInfo->hasPose = true;
    }

    // 5. status: 状态权威真源为独立 MQTT 话题 /rob/navigate/status ({"status":"0"} 就绪, {"status":"1"} 导航中)
    // 刚开机默认初始化为 "0" (就绪)，不在此处被 /rob/1/info 中的旧 status 字段覆盖

    // 6. task_id: -1 或任务 ID
    if (d.contains(QStringLiteral("task_id")) && !d.value(QStringLiteral("task_id")).isNull()) {
        outInfo->task_id = d.value(QStringLiteral("task_id")).toInt(-1);
    } else if (d.contains(QStringLiteral("taskId")) && !d.value(QStringLiteral("taskId")).isNull()) {
        outInfo->task_id = d.value(QStringLiteral("taskId")).toInt(-1);
    }

    // 7. battery: 0.98 表示 98%
    if (d.contains(QStringLiteral("battery")) && !d.value(QStringLiteral("battery")).isNull()) {
        outInfo->battery = d.value(QStringLiteral("battery")).toDouble();
    } else if (d.contains(QStringLiteral("power")) && !d.value(QStringLiteral("power")).isNull()) {
        outInfo->battery = d.value(QStringLiteral("power")).toDouble();
    }

    // 8. theta: 机台入口对应的世界坐标角度，角度值
    if (d.contains(QStringLiteral("theta")) && !d.value(QStringLiteral("theta")).isNull()) {
        outInfo->theta = d.value(QStringLiteral("theta")).toDouble();
        outInfo->hasTheta = true;
    }

    // 9. epc_x, epc_y, epc_theta: 机台定位末端坐标
    if (d.contains(QStringLiteral("epc_x")) && !d.value(QStringLiteral("epc_x")).isNull()) {
        outInfo->epc_x = d.value(QStringLiteral("epc_x")).toDouble();
        outInfo->hasEpc = true;
    }
    if (d.contains(QStringLiteral("epc_y")) && !d.value(QStringLiteral("epc_y")).isNull()) {
        outInfo->epc_y = d.value(QStringLiteral("epc_y")).toDouble();
        outInfo->hasEpc = true;
    }
    if (d.contains(QStringLiteral("epc_theta")) && !d.value(QStringLiteral("epc_theta")).isNull()) {
        outInfo->epc_theta = d.value(QStringLiteral("epc_theta")).toDouble();
        outInfo->hasEpc = true;
    }

    // 10. mileage (对象可能为 null)
    if (d.contains(QStringLiteral("mileage"))) {
        QJsonValue mv = d.value(QStringLiteral("mileage"));
        if (mv.isObject()) {
            QJsonObject m = mv.toObject();
            outInfo->mileage.valid = true;
            if (m.contains(QStringLiteral("total_distance_m")) && !m.value(QStringLiteral("total_distance_m")).isNull()) {
                outInfo->mileage.total_distance_m = m.value(QStringLiteral("total_distance_m")).toDouble();
            }
            if (m.contains(QStringLiteral("today_distance_m")) && !m.value(QStringLiteral("today_distance_m")).isNull()) {
                outInfo->mileage.today_distance_m = m.value(QStringLiteral("today_distance_m")).toDouble();
            }
        } else if (mv.isNull()) {
            outInfo->mileage.valid = false;
        }
    }

    // 11. runtime (对象可能为 null)
    if (d.contains(QStringLiteral("runtime"))) {
        QJsonValue rv = d.value(QStringLiteral("runtime"));
        if (rv.isObject()) {
            QJsonObject r = rv.toObject();
            outInfo->runtime.valid = true;
            if (r.contains(QStringLiteral("current_runtime_sec")) && !r.value(QStringLiteral("current_runtime_sec")).isNull()) {
                outInfo->runtime.current_runtime_sec = r.value(QStringLiteral("current_runtime_sec")).toVariant().toLongLong();
            } else if (r.contains(QStringLiteral("current_runtime")) && !r.value(QStringLiteral("current_runtime")).isNull()) {
                outInfo->runtime.current_runtime_sec = r.value(QStringLiteral("current_runtime")).toVariant().toLongLong();
            }
            if (r.contains(QStringLiteral("total_runtime_sec")) && !r.value(QStringLiteral("total_runtime_sec")).isNull()) {
                outInfo->runtime.total_runtime_sec = r.value(QStringLiteral("total_runtime_sec")).toVariant().toLongLong();
            } else if (r.contains(QStringLiteral("total_runtime")) && !r.value(QStringLiteral("total_runtime")).isNull()) {
                outInfo->runtime.total_runtime_sec = r.value(QStringLiteral("total_runtime")).toVariant().toLongLong();
            }
        } else if (rv.isNull()) {
            outInfo->runtime.valid = false;
        }
    } else if (d.contains(QStringLiteral("current_runtime_sec"))) {
        outInfo->runtime.valid = true;
        outInfo->runtime.current_runtime_sec = d.value(QStringLiteral("current_runtime_sec")).toVariant().toLongLong();
    }

    // 12. obstacle (对象可能为 null)
    if (d.contains(QStringLiteral("obstacle"))) {
        QJsonValue ov = d.value(QStringLiteral("obstacle"));
        if (ov.isObject()) {
            QJsonObject o = ov.toObject();
            outInfo->obstacle.valid = true;
            if (o.contains(QStringLiteral("blocked")) && !o.value(QStringLiteral("blocked")).isNull()) {
                outInfo->obstacle.blocked = o.value(QStringLiteral("blocked")).toBool();
            }
            if (o.contains(QStringLiteral("point"))) {
                QJsonValue pv = o.value(QStringLiteral("point"));
                if (pv.isObject()) {
                    QJsonObject pt = pv.toObject();
                    outInfo->obstacle.hasPoint = true;
                    outInfo->obstacle.point.x = pt.value(QStringLiteral("x")).toDouble();
                    outInfo->obstacle.point.y = pt.value(QStringLiteral("y")).toDouble();
                } else if (pv.isNull()) {
                    outInfo->obstacle.hasPoint = false;
                }
            }
        } else if (ov.isNull()) {
            outInfo->obstacle.valid = false;
            outInfo->obstacle.hasPoint = false;
        }
    }

    // 13. actual_vel (对象可能为 null)
    if (d.contains(QStringLiteral("actual_vel"))) {
        QJsonValue vv = d.value(QStringLiteral("actual_vel"));
        if (vv.isObject()) {
            QJsonObject v = vv.toObject();
            outInfo->actual_vel.valid = true;
            if (v.contains(QStringLiteral("vx")) && !v.value(QStringLiteral("vx")).isNull()) {
                outInfo->actual_vel.vx = v.value(QStringLiteral("vx")).toDouble();
            }
            if (v.contains(QStringLiteral("vy")) && !v.value(QStringLiteral("vy")).isNull()) {
                outInfo->actual_vel.vy = v.value(QStringLiteral("vy")).toDouble();
            }
            if (v.contains(QStringLiteral("wz")) && !v.value(QStringLiteral("wz")).isNull()) {
                outInfo->actual_vel.wz = v.value(QStringLiteral("wz")).toDouble();
            }
        } else if (vv.isNull()) {
            outInfo->actual_vel.valid = false;
            outInfo->actual_vel.vx = 0.0;
            outInfo->actual_vel.vy = 0.0;
            outInfo->actual_vel.wz = 0.0;
        }
    }

    // 14. cmd_vel (对象可能为 null)
    if (d.contains(QStringLiteral("cmd_vel"))) {
        QJsonValue cv = d.value(QStringLiteral("cmd_vel"));
        if (cv.isObject()) {
            QJsonObject c = cv.toObject();
            outInfo->cmd_vel.valid = true;
            if (c.contains(QStringLiteral("vx")) && !c.value(QStringLiteral("vx")).isNull()) {
                outInfo->cmd_vel.vx = c.value(QStringLiteral("vx")).toDouble();
            }
            if (c.contains(QStringLiteral("vy")) && !c.value(QStringLiteral("vy")).isNull()) {
                outInfo->cmd_vel.vy = c.value(QStringLiteral("vy")).toDouble();
            }
            if (c.contains(QStringLiteral("wz")) && !c.value(QStringLiteral("wz")).isNull()) {
                outInfo->cmd_vel.wz = c.value(QStringLiteral("wz")).toDouble();
            }
        } else if (cv.isNull()) {
            outInfo->cmd_vel.valid = false;
            outInfo->cmd_vel.vx = 0.0;
            outInfo->cmd_vel.vy = 0.0;
            outInfo->cmd_vel.wz = 0.0;
        }
    }

    // 15. 辅助与兼容字段
    if (d.contains(QStringLiteral("carName")) && !d.value(QStringLiteral("carName")).isNull()) {
        outInfo->carName = d.value(QStringLiteral("carName")).toString().trimmed();
    }
    // 定位置信度 (支持浮点数及字符串形式，支持多种命名规范)
    bool hasCov = false;
    double parsedCov = extractNum(d, {
        QStringLiteral("covariance_score"), QStringLiteral("covarianceScore"),
        QStringLiteral("confidence"), QStringLiteral("confidence_score"),
        QStringLiteral("cov_score"), QStringLiteral("covScore"),
        QStringLiteral("position_confidence"), QStringLiteral("loc_confidence")
    }, -1.0, &hasCov);

    if (hasCov) {
        if (parsedCov > 1.0) {
            parsedCov = parsedCov / 100.0;
        }
        if (parsedCov <= 0.0 && outInfo->hasPose && (outInfo->status == "3" || outInfo->status == "4") && outInfo->covScore > 0.0) {
            // 保持原有效置信度
        } else if (parsedCov >= 0.0) {
            outInfo->covScore = parsedCov;
        }
    }
    if (d.contains(QStringLiteral("firmware_version")) && !d.value(QStringLiteral("firmware_version")).isNull()) {
        outInfo->fwVersion = d.value(QStringLiteral("firmware_version")).toString().trimmed();
    }
    if (d.contains(QStringLiteral("remark")) && !d.value(QStringLiteral("remark")).isNull()) {
        outInfo->remark = d.value(QStringLiteral("remark")).toString().trimmed();
    }

    outInfo->receivedData = true;
    return true;
}

QString RobotInfoParser::formatDuration(qint64 totalSeconds)
{
    if (totalSeconds < 0) totalSeconds = 0;
    qint64 days  = totalSeconds / 86400;
    qint64 hours = (totalSeconds % 86400) / 3600;
    qint64 mins  = (totalSeconds % 3600) / 60;
    qint64 secs  = totalSeconds % 60;

    return QString("%1天/%2时/%3分/%4秒")
        .arg(days, 2, 10, QChar('0'))
        .arg(hours, 2, 10, QChar('0'))
        .arg(mins, 2, 10, QChar('0'))
        .arg(secs, 2, 10, QChar('0'));
}
