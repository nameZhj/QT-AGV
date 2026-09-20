#include "RobotInfoManager.h"
#include "MqttService.h"
#include "AppConfig.h"
#include <QJsonDocument>
#include <QRegularExpression>
#include <QDebug>

RobotInfoManager &RobotInfoManager::instance()
{
    static RobotInfoManager s_instance;
    return s_instance;
}

RobotInfoManager::RobotInfoManager(QObject *parent)
    : QObject(parent)
{
}

void RobotInfoManager::init()
{
    if (m_initialized) return;
    m_initialized = true;

    auto &mqtt = MqttService::instance();
    connect(&mqtt, &MqttService::connected, this, &RobotInfoManager::onMqttConnected);

    if (mqtt.isConnected()) {
        onMqttConnected();
    }
}

void RobotInfoManager::onMqttConnected()
{
    auto &mqtt = MqttService::instance();
    auto handlePayload = [this](const QString &topic, const QByteArray &payload) {
        updateFromPayload(topic, payload);
    };

    // 订阅通用与多机通配符 info 话题
    mqtt.subscribeTopic(QStringLiteral("/info"), 0, handlePayload);
    mqtt.subscribeTopic(QStringLiteral("info"), 0, handlePayload);
    mqtt.subscribeTopic(QStringLiteral("/rob/+/info"), 0, handlePayload);
    mqtt.subscribeTopic(QStringLiteral("/rob/info"), 0, handlePayload);
}

RobotInfo RobotInfoManager::currentInfo() const
{
    QString curId = AppConfig::instance().agvId().trimmed();
    if (curId.isEmpty()) {
        curId = AppConfig::instance().currentDevice().carId.trimmed();
    }
    if (curId.isEmpty()) curId = QStringLiteral("1");

    return infoForCar(curId);
}

RobotInfo RobotInfoManager::infoForCar(const QString &carId) const
{
    QReadLocker locker(&m_lock);
    QString key = carId.trimmed();
    if (m_robots.contains(key)) {
        return m_robots.value(key);
    }
    // 返回初始化空对象
    RobotInfo def;
    def.carIdStr = key;
    bool ok = false;
    int cid = key.toInt(&ok);
    if (ok) def.car_id = cid;
    return def;
}

QMap<QString, RobotInfo> RobotInfoManager::allRobotInfos() const
{
    QReadLocker locker(&m_lock);
    return m_robots;
}

bool RobotInfoManager::hasInfoForCar(const QString &carId) const
{
    QReadLocker locker(&m_lock);
    return m_robots.contains(carId.trimmed());
}

bool RobotInfoManager::updateFromPayload(const QString &topic, const QByteArray &payload, QString *outCarId)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    QJsonObject d = doc.object();

    // 提取目标车辆 ID
    QString carId;
    if (d.contains(QStringLiteral("car_id")) && !d.value(QStringLiteral("car_id")).isNull()) {
        QJsonValue cv = d.value(QStringLiteral("car_id"));
        if (cv.isDouble()) carId = QString::number(cv.toInt());
        else if (cv.isString()) carId = cv.toString().trimmed();
    } else if (d.contains(QStringLiteral("carId")) && !d.value(QStringLiteral("carId")).isNull()) {
        QJsonValue cv = d.value(QStringLiteral("carId"));
        if (cv.isDouble()) carId = QString::number(cv.toInt());
        else if (cv.isString()) carId = cv.toString().trimmed();
    }

    // 从 topic 中提取: /rob/{carId}/info
    if (carId.isEmpty()) {
        static const QRegularExpression topicRe(QStringLiteral("/rob/([^/]+)/info"));
        QRegularExpressionMatch match = topicRe.match(topic);
        if (match.hasMatch()) {
            carId = match.captured(1).trimmed();
        }
    }

    // 兜底至当前默认车辆
    if (carId.isEmpty()) {
        carId = AppConfig::instance().agvId().trimmed();
        if (carId.isEmpty()) carId = AppConfig::instance().currentDevice().carId.trimmed();
        if (carId.isEmpty()) carId = QStringLiteral("1");
    }

    if (outCarId) {
        *outCarId = carId;
    }

    RobotInfo snapshot;
    {
        QWriteLocker locker(&m_lock);
        RobotInfo &target = m_robots[carId];
        target.carIdStr = carId;
        bool ok = false;
        int cid = carId.toInt(&ok);
        if (ok) target.car_id = cid;

        parseInfoJson(d, &target);
        snapshot = target;
    }

    // 派发信号
    emit robotInfoUpdated(carId, snapshot);

    QString curId = AppConfig::instance().agvId().trimmed();
    if (curId.isEmpty()) curId = AppConfig::instance().currentDevice().carId.trimmed();
    if (curId.isEmpty()) curId = QStringLiteral("1");

    if (carId == curId) {
        emit currentRobotInfoUpdated(snapshot);
    }

    return true;
}

bool RobotInfoManager::parseInfoJson(const QJsonObject &d, RobotInfo *outInfo)
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
    if (d.contains(QStringLiteral("car_x")) && !d.value(QStringLiteral("car_x")).isNull()) {
        outInfo->car_x = d.value(QStringLiteral("car_x")).toDouble();
        outInfo->hasPose = true;
    } else if (d.contains(QStringLiteral("x")) && !d.value(QStringLiteral("x")).isNull()) {
        outInfo->car_x = d.value(QStringLiteral("x")).toDouble();
        outInfo->hasPose = true;
    }

    if (d.contains(QStringLiteral("car_y")) && !d.value(QStringLiteral("car_y")).isNull()) {
        outInfo->car_y = d.value(QStringLiteral("car_y")).toDouble();
        outInfo->hasPose = true;
    } else if (d.contains(QStringLiteral("y")) && !d.value(QStringLiteral("y")).isNull()) {
        outInfo->car_y = d.value(QStringLiteral("y")).toDouble();
        outInfo->hasPose = true;
    }

    if (d.contains(QStringLiteral("car_z")) && !d.value(QStringLiteral("car_z")).isNull()) {
        outInfo->car_z = d.value(QStringLiteral("car_z")).toDouble();
    } else if (d.contains(QStringLiteral("z")) && !d.value(QStringLiteral("z")).isNull()) {
        outInfo->car_z = d.value(QStringLiteral("z")).toDouble();
    }

    // 4. car_yaw
    if (d.contains(QStringLiteral("car_yaw")) && !d.value(QStringLiteral("car_yaw")).isNull()) {
        outInfo->car_yaw = d.value(QStringLiteral("car_yaw")).toDouble();
        outInfo->hasPose = true;
    } else if (d.contains(QStringLiteral("yaw")) && !d.value(QStringLiteral("yaw")).isNull()) {
        outInfo->car_yaw = d.value(QStringLiteral("yaw")).toDouble();
        outInfo->hasPose = true;
    }

    // 5. status: "0"正常未开机 "2"正常开机 "3"正在导航 "4"停止导航作业中 "5"中止暂停 "6"故障
    if (d.contains(QStringLiteral("status")) && !d.value(QStringLiteral("status")).isNull()) {
        QJsonValue sv = d.value(QStringLiteral("status"));
        if (sv.isString()) outInfo->status = sv.toString().trimmed();
        else if (sv.isDouble()) outInfo->status = QString::number(sv.toInt());
    }

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
    if (d.contains(QStringLiteral("covariance_score")) && !d.value(QStringLiteral("covariance_score")).isNull()) {
        outInfo->covScore = d.value(QStringLiteral("covariance_score")).toDouble();
    } else if (d.contains(QStringLiteral("confidence")) && !d.value(QStringLiteral("confidence")).isNull()) {
        outInfo->covScore = d.value(QStringLiteral("confidence")).toDouble();
    }
    if (d.contains(QStringLiteral("firmware_version")) && !d.value(QStringLiteral("firmware_version")).isNull()) {
        outInfo->fwVersion = d.value(QStringLiteral("firmware_version")).toString().trimmed();
    }
    if (d.contains(QStringLiteral("remark")) && !d.value(QStringLiteral("remark")).isNull()) {
        outInfo->remark = d.value(QStringLiteral("remark")).toString().trimmed();
    }

    return true;
}

QString RobotInfoManager::formatDuration(qint64 totalSeconds)
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
