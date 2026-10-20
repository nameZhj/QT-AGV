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
    // 刚开机默认初始化状态为“就绪” (status: "0")，但未接收到有效数据时 receivedData 为 false
    RobotInfo def;
    def.carIdStr = QStringLiteral("1");
    def.car_id = 1;
    def.status = QStringLiteral("0");
    def.receivedData = false;
    def.latency = -1;
    m_robots[QStringLiteral("1")] = def;
}

void RobotInfoManager::init()
{
    auto &mqtt = MqttService::instance();
    connect(&mqtt, &MqttService::connected, this, &RobotInfoManager::onMqttConnected, Qt::UniqueConnection);
    connect(&mqtt, &MqttService::disconnected, this, [this]() {
        QWriteLocker locker(&m_lock);
        for (auto &r : m_robots) {
            r.receivedData = false;
            r.latency = -1;
        }
    }, Qt::UniqueConnection);

    m_initialized = true;

    // 无论当前 MQTT 是否已连接完成，立刻全局注册核心订阅
    subscribeGlobalTopics();
}

void RobotInfoManager::subscribeGlobalTopics()
{
    auto &mqtt = MqttService::instance();
    auto handlePayload = [this](const QString &topic, const QByteArray &payload) {
        updateFromPayload(topic, payload);
    };

    const QString curCarId = AppConfig::instance().agvId().trimmed().isEmpty()
                             ? QStringLiteral("1")
                             : AppConfig::instance().agvId().trimmed();

    mqtt.setTopicSubscription(QStringLiteral("/rob/1/info"), 0, handlePayload);
    if (curCarId != QStringLiteral("1")) {
        mqtt.setTopicSubscription(QStringLiteral("/rob/%1/info").arg(curCarId), 0, handlePayload);
    }
    mqtt.setTopicSubscription(QStringLiteral("/rob/+/info"), 0, handlePayload);
    mqtt.setTopicSubscription(QStringLiteral("/rob/info"), 0, handlePayload);
    mqtt.setTopicSubscription(QStringLiteral("/info"), 0, handlePayload);
    mqtt.setTopicSubscription(QStringLiteral("info"), 0, handlePayload);

    // 订阅车辆状态MQTT消息：/rob/navigate/status
    mqtt.setTopicSubscription(QStringLiteral("/rob/navigate/status"), 0, [this](const QString &, const QByteArray &payload) {
        updateNavigateStatus(payload);
    });
    mqtt.setTopicSubscription(QStringLiteral("/rob/+/navigate/status"), 0, [this](const QString &, const QByteArray &payload) {
        updateNavigateStatus(payload);
    });
}

void RobotInfoManager::onMqttConnected()
{
    subscribeGlobalTopics();
}

void RobotInfoManager::resetForCar(const QString &newCarId)
{
    {
        QWriteLocker locker(&m_lock);
        m_robots.clear(); // 清理旧车全部数据与缓存
        RobotInfo def;
        def.carIdStr = QStringLiteral("1");
        def.car_id = 1;
        def.status = QStringLiteral("0"); // 重置初始化状态为就绪
        def.receivedData = false;
        def.latency = -1;
        m_robots[QStringLiteral("1")] = def;
        if (!newCarId.isEmpty() && newCarId != QStringLiteral("1")) {
            def.carIdStr = newCarId;
            def.car_id = newCarId.toInt();
            m_robots[newCarId] = def;
        }
    }

    subscribeGlobalTopics();
}

RobotInfo RobotInfoManager::currentInfo() const
{
    QReadLocker locker(&m_lock);
    // 用户明确规定：/rob/{car_id}/info 中 car_id 永远是常数 1，优先返回 ID 为 1 的车辆信息
    if (m_robots.contains(QStringLiteral("1"))) {
        return m_robots.value(QStringLiteral("1"));
    }
    QString curId = AppConfig::instance().agvId().trimmed();
    if (curId.isEmpty()) {
        curId = AppConfig::instance().currentDevice().carId.trimmed();
    }
    if (!curId.isEmpty() && m_robots.contains(curId)) {
        return m_robots.value(curId);
    }
    if (!m_robots.isEmpty()) {
        return m_robots.constBegin().value();
    }
    // 返回初始化空对象（默认 car_id 为 1）
    RobotInfo def;
    def.carIdStr = QStringLiteral("1");
    def.car_id = 1;
    return def;
}

RobotInfo RobotInfoManager::infoForCar(const QString &carId) const
{
    QReadLocker locker(&m_lock);
    QString key = carId.trimmed();
    if (m_robots.contains(key)) {
        return m_robots.value(key);
    }
    // /rob/{car_id}/info 中 car_id 永远是常数 1，兜底回退到 1
    if (m_robots.contains(QStringLiteral("1"))) {
        return m_robots.value(QStringLiteral("1"));
    }
    RobotInfo def;
    def.carIdStr = key.isEmpty() ? QStringLiteral("1") : key;
    bool ok = false;
    int cid = def.carIdStr.toInt(&ok);
    def.car_id = ok ? cid : 1;
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

bool RobotInfoManager::updateNavigateStatus(const QByteArray &payload)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    QJsonObject obj = doc.object();
    if (!obj.contains(QStringLiteral("status"))) {
        return false;
    }

    QString statusVal;
    QJsonValue sv = obj.value(QStringLiteral("status"));
    if (sv.isString()) {
        statusVal = sv.toString().trimmed();
    } else if (sv.isDouble()) {
        statusVal = QString::number(sv.toInt());
    }

    if (statusVal.isEmpty()) {
        return false;
    }

    const QString carId = QStringLiteral("1");
    RobotInfo snapshot;
    {
        QWriteLocker locker(&m_lock);
        RobotInfo &target = m_robots[carId];
        target.carIdStr = carId;
        target.car_id = 1;
        target.status = statusVal; // "0" 就绪, "1" 导航中, "2" 暂停导航
        target.receivedData = true;
        snapshot = target;

        QString curId = AppConfig::instance().agvId().trimmed();
        if (curId.isEmpty()) curId = AppConfig::instance().currentDevice().carId.trimmed();
        if (!curId.isEmpty() && curId != carId) {
            m_robots[curId] = snapshot;
        }
    }

    qDebug().noquote() << QStringLiteral("[RobotInfoManager] 收到 /rob/navigate/status -> status:%1 (%2)")
                              .arg(statusVal, (statusVal == "0" ? "就绪" : (statusVal == "1" ? "导航中" : (statusVal == "2" ? "暂停导航" : statusVal))));

    emit robotInfoUpdated(carId, snapshot);
    emit currentRobotInfoUpdated(snapshot);
    return true;
}

bool RobotInfoManager::updateFromPayload(const QString &topic, const QByteArray &payload, QString *outCarId)
{
    if (topic.endsWith(QStringLiteral("/navigate/status"))) {
        if (outCarId) *outCarId = QStringLiteral("1");
        return updateNavigateStatus(payload);
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    QJsonObject d = doc.object();

    // 兼容外层包装 data 对象的结构: { code: 200, data: { ... } }
    QJsonObject source = d;
    if (d.contains(QStringLiteral("data")) && d.value(QStringLiteral("data")).isObject()) {
        QJsonObject dataObj = d.value(QStringLiteral("data")).toObject();
        for (auto it = dataObj.constBegin(); it != dataObj.constEnd(); ++it) {
            source.insert(it.key(), it.value());
        }
    }

    // 检查是否为合法的机器人设备状态报文（防止任务链/任务结构等非状态消息污染主车状态）
    bool isRobotInfoPayload = source.contains(QStringLiteral("car_id")) || source.contains(QStringLiteral("carId"))
                           || source.contains(QStringLiteral("car_x"))  || source.contains(QStringLiteral("x"))
                           || source.contains(QStringLiteral("status")) || source.contains(QStringLiteral("battery"))
                           || source.contains(QStringLiteral("power"))  || source.contains(QStringLiteral("map_id"));
    if (!isRobotInfoPayload) {
        return false;
    }

    // 用户明确规定：/rob/{car_id}/info 中 car_id 永远是常数 1
    const QString carId = QStringLiteral("1");

    if (outCarId) {
        *outCarId = carId;
    }

    RobotInfo snapshot;
    {
        QWriteLocker locker(&m_lock);
        RobotInfo &target = m_robots[carId];
        target.carIdStr = carId;
        target.car_id = 1;

        parseInfoJson(source, &target);
        target.car_id = 1; // 强制常数 1
        target.carIdStr = carId;
        snapshot = target;

        // 若当前配置有指定设备ID，同步写入镜像以兼容多处旧接口查询
        QString curId = AppConfig::instance().agvId().trimmed();
        if (curId.isEmpty()) curId = AppConfig::instance().currentDevice().carId.trimmed();
        if (!curId.isEmpty() && curId != carId) {
            m_robots[curId] = snapshot;
        }
    }

    // 派发信号
    emit robotInfoUpdated(carId, snapshot);
    emit currentRobotInfoUpdated(snapshot);

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
    auto extractNum = [](const QJsonObject &obj, const QStringList &keys, double defVal, bool *found = nullptr) -> double {
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
    };

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
        // 如果提取到的值为 0.0，但当前车辆位姿正常有效且正处于导航/作业运行中(status 3/4)，且此前已有有效置信度：
        // 说明此帧为过程过渡帧或未开启单独匹配计算，保留此前有效置信度，防止误置为 0% 触发假报警
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
