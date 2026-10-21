/**
 * @file    EditConfigRouteSync.cpp
 * @layer   GUILayer
 * @brief   Implementation of obstacle avoidance parameter synchronization with route network.
 * @deps    EditConfigRouteSync.h, EditConfigWidget.h, ApiService.h, MotionConfigManager.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "EditConfigRouteSync.h"
#include "EditConfigWidget.h"
#include "network/ApiService.h"
#include "core/MotionConfigManager.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QDebug>

void EditConfigRouteSync::syncObstacleParamsToRouteNetwork(EditConfigWidget *w,
                                                         double stopDx, double stopDy,
                                                         double decelDx, double decelDy,
                                                         std::function<void(bool ok)> onDone)
{
    if (!w) return;
    ApiService::instance().loadEditedMap([w, stopDx, stopDy, decelDx, decelDy, onDone](int code, const QJsonObject &resp) {
        if (code != 200) {
            qWarning() << "[EditConfigWidget] Failed to load route network for obstacle sync, code:" << code;
            if (onDone) onDone(false);
            return;
        }

        QString jsonStr = resp.value("json").toString();
        if (jsonStr.isEmpty()) {
            if (onDone) onDone(false);
            return;
        }

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            if (onDone) onDone(false);
            return;
        }

        QJsonObject geojson = doc.object();
        QString mapId = geojson.value("mapId").toString();
        QString mapName = geojson.value("mapName").toString();

        auto isSpecialPoint = [](const QJsonObject &props) -> bool {
            QString id = props.value("id").toString();
            QString type = props.value("type").toString();
            QString typeName = props.value("typeName").toString();
            QString name = props.value("name").toString();

            if (id.startsWith("c_", Qt::CaseInsensitive) || id.startsWith("hp_", Qt::CaseInsensitive) ||
                id.startsWith("cp_", Qt::CaseInsensitive) || id.startsWith("ap_", Qt::CaseInsensitive)) {
                return true;
            }
            if (type.compare("charge", Qt::CaseInsensitive) == 0 ||
                type.compare("highPrecision", Qt::CaseInsensitive) == 0 ||
                type.compare("chargeEpc", Qt::CaseInsensitive) == 0 ||
                type.compare("highPrecisionEpc", Qt::CaseInsensitive) == 0) {
                return true;
            }
            if (typeName.compare("ChargePoint", Qt::CaseInsensitive) == 0 ||
                typeName.compare("ActionPoint", Qt::CaseInsensitive) == 0) {
                return true;
            }
            if (name.startsWith("CP", Qt::CaseInsensitive) || name.startsWith("AP", Qt::CaseInsensitive) ||
                name.contains("充电") || name.contains("工作站")) {
                return true;
            }
            return false;
        };

        if (geojson.contains("features") && geojson.value("features").isArray()) {
            QJsonArray features = geojson.value("features").toArray();
            for (int i = 0; i < features.size(); ++i) {
                QJsonObject feat = features[i].toObject();
                QJsonObject props = feat.value("properties").toObject();
                QJsonObject geom = feat.value("geometry").toObject();
                QString gtype = geom.value("type").toString();

                if (gtype == "Point") {
                    if (isSpecialPoint(props)) {
                        props["intrusion_stop_length"]         = 0.0;
                        props["intrusion_stop_width"]          = 0.0;
                        props["intrusion_deceleration_length"] = 0.0;
                        props["intrusion_deceleration_width"]  = 0.0;
                        props["is_custom_obstacle"]            = false;
                        if (props.contains("actions") && props.value("actions").isArray()) {
                            QJsonArray acts = props.value("actions").toArray();
                            for (int a = 0; a < acts.size(); ++a) {
                                QJsonObject act = acts[a].toObject();
                                if (act.value("action_type").toInt() == 21 || act.value("name").toString() == "实时避障范围") {
                                    act["name"] = "实时避障范围";
                                    act["action_type"] = 21;
                                    act["intrusion_stop_length"]         = 0.0;
                                    act["intrusion_stop_width"]          = 0.0;
                                    act["intrusion_deceleration_length"] = 0.0;
                                    act["intrusion_deceleration_width"]  = 0.0;
                                    act["is_custom_obstacle"]            = false;
                                    acts[a] = act;
                                }
                            }
                            props["actions"] = acts;
                        }
                    } else {
                        bool isCustom = props.value("is_custom_obstacle").toBool(props.value("custom_obstacle").toBool(false));
                        if (!isCustom) {
                            props["intrusion_stop_length"]         = stopDx;
                            props["intrusion_stop_width"]          = stopDy;
                            props["intrusion_deceleration_length"] = decelDx;
                            props["intrusion_deceleration_width"]  = decelDy;
                            props["is_custom_obstacle"]            = false;

                            QJsonArray acts = props.value("actions").toArray();
                            bool foundAct = false;
                            for (int a = 0; a < acts.size(); ++a) {
                                QJsonObject act = acts[a].toObject();
                                if (act.value("action_type").toInt() == 21 || act.value("name").toString() == "实时避障范围") {
                                    act["name"] = "实时避障范围";
                                    act["action_type"] = 21;
                                    act["intrusion_stop_length"]         = stopDx;
                                    act["intrusion_stop_width"]          = stopDy;
                                    act["intrusion_deceleration_length"] = decelDx;
                                    act["intrusion_deceleration_width"]  = decelDy;
                                    act["is_custom_obstacle"]            = false;
                                    acts[a] = act;
                                    foundAct = true;
                                    break;
                                }
                            }
                            if (!foundAct && (stopDx > 0.0 || stopDy > 0.0 || decelDx > 0.0 || decelDy > 0.0)) {
                                QJsonObject act;
                                act["name"] = "实时避障范围";
                                act["action_type"] = 21;
                                act["intrusion_stop_length"]         = stopDx;
                                act["intrusion_stop_width"]          = stopDy;
                                act["intrusion_deceleration_length"] = decelDx;
                                act["intrusion_deceleration_width"]  = decelDy;
                                act["is_custom_obstacle"]            = false;
                                acts.append(act);
                            }
                            props["actions"] = acts;
                        }
                    }
                } else if (gtype == "LineString" || gtype == "MultiPoint") {
                    bool isCustom = props.value("is_custom_obstacle").toBool(props.value("custom_obstacle").toBool(false));
                    if (!isCustom) {
                        props["intrusion_stop_length"]         = stopDx;
                        props["intrusion_stop_width"]          = stopDy;
                        props["intrusion_deceleration_length"] = decelDx;
                        props["intrusion_deceleration_width"]  = decelDy;
                        props["is_custom_obstacle"]            = false;

                        const auto &mc = MotionConfigManager::instance();
                        props["maxspeed"]                 = mc.maxSpeed();
                        props["max_acceleration"]         = mc.maxAcceleration();
                        props["maxacc"]                   = mc.maxAcceleration();
                        props["max_deceleration"]         = mc.maxDeceleration();
                        props["maxdec"]                   = mc.maxDeceleration();
                        props["max_angular_speed"]        = mc.maxAngularSpeedDeg();
                        props["maxrot"]                   = mc.maxAngularSpeedDeg();
                        props["max_angular_acceleration"] = mc.maxAngularAccelerationDeg();
                        props["maxrotacc"]                = mc.maxAngularAccelerationDeg();
                        props["reachdist"]                = mc.arrivalDistanceTolerance();
                        props["reachangle"]               = mc.arrivalAngleToleranceDeg();
                        props["width"]                    = mc.routeWidth();
                    }
                }

                feat["properties"] = props;
                features[i] = feat;
            }
            geojson["features"] = features;
        }

        if (!geojson.contains("features") && (geojson.contains("points") || geojson.contains("lines") || geojson.contains("curves"))) {
            auto updatePoints = [&](QJsonArray &arr, bool isSpecialArr) {
                for (int i = 0; i < arr.size(); ++i) {
                    QJsonObject pt = arr[i].toObject();
                    if (isSpecialArr || isSpecialPoint(pt)) {
                        pt["intrusion_stop_length"]         = 0.0;
                        pt["intrusion_stop_width"]          = 0.0;
                        pt["intrusion_deceleration_length"] = 0.0;
                        pt["intrusion_deceleration_width"]  = 0.0;
                        pt["is_custom_obstacle"]            = false;
                    } else {
                        bool isCustom = pt.value("is_custom_obstacle").toBool(pt.value("custom_obstacle").toBool(false));
                        if (!isCustom) {
                            pt["intrusion_stop_length"]         = stopDx;
                            pt["intrusion_stop_width"]          = stopDy;
                            pt["intrusion_deceleration_length"] = decelDx;
                            pt["intrusion_deceleration_width"]  = decelDy;
                            pt["is_custom_obstacle"]            = false;
                        }
                    }
                    arr[i] = pt;
                }
            };
            auto updateLines = [&](QJsonArray &arr) {
                for (int i = 0; i < arr.size(); ++i) {
                    QJsonObject ln = arr[i].toObject();
                    bool isCustom = ln.value("is_custom_obstacle").toBool(ln.value("custom_obstacle").toBool(false));
                    if (!isCustom) {
                        ln["intrusion_stop_length"]         = stopDx;
                        ln["intrusion_stop_width"]          = stopDy;
                        ln["intrusion_deceleration_length"] = decelDx;
                        ln["intrusion_deceleration_width"]  = decelDy;
                        ln["is_custom_obstacle"]            = false;

                        const auto &mc = MotionConfigManager::instance();
                        ln["maxspeed"]                 = mc.maxSpeed();
                        ln["max_acceleration"]         = mc.maxAcceleration();
                        ln["maxacc"]                   = mc.maxAcceleration();
                        ln["max_deceleration"]         = mc.maxDeceleration();
                        ln["maxdec"]                   = mc.maxDeceleration();
                        ln["max_angular_speed"]        = mc.maxAngularSpeedDeg();
                        ln["maxrot"]                   = mc.maxAngularSpeedDeg();
                        ln["max_angular_acceleration"] = mc.maxAngularAccelerationDeg();
                        ln["maxrotacc"]                = mc.maxAngularAccelerationDeg();
                        ln["reachdist"]                = mc.arrivalDistanceTolerance();
                        ln["reachangle"]               = mc.arrivalAngleToleranceDeg();
                        ln["width"]                    = mc.routeWidth();
                    }
                    arr[i] = ln;
                }
            };
            if (geojson.contains("points")) {
                QJsonArray pts = geojson.value("points").toArray();
                updatePoints(pts, false);
                geojson["points"] = pts;
            }
            if (geojson.contains("charges")) {
                QJsonArray chs = geojson.value("charges").toArray();
                updatePoints(chs, true);
                geojson["charges"] = chs;
            }
            if (geojson.contains("highPrecisionPoints")) {
                QJsonArray hps = geojson.value("highPrecisionPoints").toArray();
                updatePoints(hps, true);
                geojson["highPrecisionPoints"] = hps;
            }
            if (geojson.contains("lines")) {
                QJsonArray lns = geojson.value("lines").toArray();
                updateLines(lns);
                geojson["lines"] = lns;
            }
            if (geojson.contains("curves")) {
                QJsonArray cvs = geojson.value("curves").toArray();
                updateLines(cvs);
                geojson["curves"] = cvs;
            }
        }

        ApiService::instance().saveEditedMap(geojson, [w, geojson, mapName, mapId, onDone](int saveCode, const QJsonObject &saveResp) {
            if (saveCode == 200) {
                if (!mapId.isEmpty() && !mapName.isEmpty()) {
                    QString localDir = QCoreApplication::applicationDirPath() + "/maps/" + mapName + "_" + mapId;
                    QDir().mkpath(localDir);
                    QFile rf(localDir + "/route.json");
                    if (rf.open(QIODevice::WriteOnly)) {
                        rf.write(QJsonDocument(geojson).toJson());
                        rf.close();
                    }
                }
                qDebug() << "[EditConfigWidget] Route network obstacle parameters synchronized and saved successfully!";
                emit w->routeNetworkUpdated();
                if (onDone) onDone(true);
            } else {
                QString err = saveResp.value("msg").toString(saveResp.value("message").toString());
                qWarning() << "[EditConfigWidget] Failed to save route network after obstacle sync, code:" << saveCode << "err:" << err;
                if (onDone) onDone(false);
            }
        });
    });
}
