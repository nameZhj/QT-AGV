/**
 * @file    MapViewMarkHelper.cpp
 * @layer   GUI
 * @brief   Robot right-click marking and EPC acquisition implementation.
 * @deps    MapViewMarkHelper.h, MqttService.h, RobotInfoManager.h, Toast.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapViewMarkHelper.h"
#include "network/MqttService.h"
#include "core/RobotInfoManager.h"
#include "ui/common/Toast.h"
#include <QMenu>
#include <QAction>
#include <QProgressDialog>
#include <QTimer>
#include <QJsonDocument>
#include <cmath>

static int nextIdNum(const QJsonArray &arr, const QString &prefix)
{
    int maxN = 0;
    for (const auto &v : arr) {
        QString id = v.toObject().value("id").toString();
        if (id.startsWith(prefix)) {
            bool ok = false;
            int n = id.mid(prefix.length()).toInt(&ok);
            if (ok && n > maxN) maxN = n;
        }
    }
    return maxN + 1;
}

static void populateCommonDeviceProps(QJsonObject &obj, double wx, double wy, double carYaw, double epcX, double epcY, double epcTheta)
{
    double carTheta = carYaw * 180.0 / M_PI;
    obj["x"]          = wx;
    obj["y"]          = wy;
    obj["epc_x"]      = epcX;
    obj["epc_y"]      = epcY;
    obj["epc_theta"]  = epcTheta;
    obj["epcX"]       = epcX;
    obj["epcY"]       = epcY;
    obj["epcTheta"]   = epcTheta;
    obj["epc_Yaw"]    = epcTheta;
    obj["epc_yaw"]    = epcTheta;
    obj["yaw"]        = carYaw;
    obj["theta"]      = carTheta;
    obj["roll"]       = 0.0;
    obj["pitch"]      = 0.0;
    obj["offset"]     = 0.0;
    obj["isyawfix"]   = false;
    obj["robotYaw"]   = 0.0;
    obj["rdsDockable"]= true;
    obj["allowRotate"]= true;
    obj["useDownPGV"] = false;
}

MapViewMarkHelper::MapViewMarkHelper(QWidget *parentWidget, QObject *parent)
    : QObject(parent), m_parentWidget(parentWidget)
{
}

void MapViewMarkHelper::showRobotContextMenu(const QPoint &globalPos,
                                             const QString &mapId,
                                             const QString &carId,
                                             bool posValid,
                                             double wx, double wy, double yawRad,
                                             QJsonArray &points,
                                             QJsonArray &charges,
                                             QJsonArray &highPrecisionPoints,
                                             std::function<void(std::function<void(bool)>)> saveRouteToServer)
{
    QMenu menu(m_parentWidget);
    menu.setStyleSheet(
        "QMenu { background-color: #22262e; border: 1px solid #3a3f4a; padding: 4px; border-radius: 4px; }"
        "QMenu::item { color: #c8d0d8; padding: 6px 20px; font-size: 12px; border-radius: 3px; }"
        "QMenu::item:selected { background-color: #1e4080; color: #ffffff; }"
    );

    QAction *actStation = menu.addAction("标记站点");
    QAction *actWorkStation = menu.addAction("标记工作站");
    QAction *actSpecialWorkStation = menu.addAction("标记特殊工作站");

    QAction *sel = menu.exec(globalPos);
    if (sel == actStation) {
        markStation(mapId, posValid, wx, wy, yawRad, points, saveRouteToServer);
    } else if (sel == actWorkStation) {
        markDevicePoint("highPrecision", mapId, carId, posValid, wx, wy, yawRad, charges, highPrecisionPoints, saveRouteToServer);
    } else if (sel == actSpecialWorkStation) {
        markDevicePoint("charge", mapId, carId, posValid, wx, wy, yawRad, charges, highPrecisionPoints, saveRouteToServer);
    }
}

void MapViewMarkHelper::markStation(const QString &mapId, bool posValid,
                                   double wx, double wy, double yawRad,
                                   QJsonArray &points,
                                   std::function<void(std::function<void(bool)>)> saveRouteToServer)
{
    if (mapId.isEmpty()) { Toast::info(m_parentWidget, "当前未加载地图，无法打点"); return; }
    if (!posValid) { Toast::info(m_parentWidget, "未收到设备位置信息，无法打点"); return; }

    double carTheta = yawRad * 180.0 / M_PI;
    int n = nextIdNum(points, "p_");
    QJsonObject pt;
    pt["id"] = QString("p_%1").arg(n);
    pt["name"] = QString("LM%1").arg(n);
    pt["typeName"] = "LandMark";
    pt["x"] = wx; pt["y"] = wy; pt["yaw"] = yawRad; pt["theta"] = carTheta;
    pt["robotYaw"] = 0.0; pt["roll"] = 0.0; pt["pitch"] = 0.0; pt["offset"] = 0.0;
    pt["isyawfix"] = false; pt["rdsDockable"] = true; pt["allowRotate"] = true; pt["useDownPGV"] = false;
    points.append(pt);

    QWidget *pw = m_parentWidget;
    saveRouteToServer([pw, pt, carTheta](bool ok) {
        if (ok) {
            Toast::success(pw, QString("已在设备位置(%1, %2)添加普通站点 %3\n朝向角: %4° 并已保存路网")
                .arg(pt.value("x").toDouble(), 0, 'f', 2).arg(pt.value("y").toDouble(), 0, 'f', 2)
                .arg(pt.value("name").toString()).arg(carTheta, 0, 'f', 1));
        }
    });
}

void MapViewMarkHelper::markDevicePoint(const QString &type,
                                       const QString &mapId,
                                       const QString &carId,
                                       bool posValid,
                                       double wx, double wy, double yawRad,
                                       QJsonArray &charges,
                                       QJsonArray &highPrecisionPoints,
                                       std::function<void(std::function<void(bool)>)> saveRouteToServer)
{
    if (mapId.isEmpty()) { Toast::info(m_parentWidget, "当前未加载地图，无法打点"); return; }
    if (m_devicePointPending) { Toast::info(m_parentWidget, "上一次设备打点尚未完成，请稍候"); return; }
    if (!posValid) { Toast::info(m_parentWidget, "未收到设备位置信息，无法打点"); return; }
    if (carId.isEmpty()) { Toast::info(m_parentWidget, "未选择设备，无法打点"); return; }

    m_devicePointPending = true;

    QJsonObject data;
    data["point_type"] = (type == "charge") ? "charge" : "highPrecision";
    data["sender"]     = "Web";
    MqttService::instance().publish(QString("/rob/%1/pointType").arg(carId), QJsonDocument(data).toJson(QJsonDocument::Compact));

    const int kWaitMs = 3000;
    const int kStepMs = 100;
    auto *prog = new QProgressDialog(QString("正在获取设备相对定位 (epc)...\n剩余 %1 秒").arg(kWaitMs / 1000), QString(), 0, kWaitMs / kStepMs, m_parentWidget);
    prog->setWindowTitle("设备打点");
    prog->setWindowModality(Qt::WindowModal);
    prog->setMinimumDuration(0);
    prog->setAutoClose(false);
    prog->setAutoReset(false);
    prog->setValue(0);
    prog->setCancelButton(nullptr);

    auto *tick = new QTimer(prog);
    tick->setInterval(kStepMs);
    int elapsed = 0;
    QWidget *pw = m_parentWidget;

    connect(tick, &QTimer::timeout, this, [this, prog, tick, kStepMs, kWaitMs, elapsed, type, wx, wy, yawRad, &charges, &highPrecisionPoints, saveRouteToServer, pw]() mutable {
        elapsed += kStepMs;
        int remainSec = (kWaitMs - elapsed + 999) / 1000;
        prog->setLabelText(QString("正在获取设备相对定位 (epc)...\n剩余 %1 秒").arg(remainSec));
        prog->setValue(elapsed / kStepMs);

        if (elapsed >= kWaitMs) {
            tick->stop();
            prog->accept();
            m_devicePointPending = false;

            const RobotInfo &info = RobotInfoManager::instance().currentInfo();
            double epcX = info.hasEpc ? info.epc_x : 0.0;
            double epcY = info.hasEpc ? info.epc_y : 0.0;
            double epcTheta = info.hasEpc ? info.epc_theta : 0.0;

            if (type == "charge") {
                int n = nextIdNum(charges, "c_");
                QJsonObject ch;
                ch["id"] = QString("c_%1").arg(n);
                ch["name"] = QString("CP_%1").arg(n);
                ch["typeName"] = "ChargePoint";
                populateCommonDeviceProps(ch, wx, wy, yawRad, epcX, epcY, epcTheta);
                charges.append(ch);

                saveRouteToServer([pw, wx, wy, epcX, epcY, ch](bool ok) {
                    if (ok) Toast::success(pw, QString("已在设备位置(%1, %2)添加特殊工作站 %3\nepc(%4, %5) 并已保存路网").arg(wx, 0, 'f', 2).arg(wy, 0, 'f', 2).arg(ch.value("name").toString()).arg(epcX, 0, 'f', 2).arg(epcY, 0, 'f', 2));
                });
            } else {
                int n = nextIdNum(highPrecisionPoints, "hp_");
                QJsonObject hp;
                hp["id"] = QString("hp_%1").arg(n);
                hp["name"] = QString("AP%1").arg(n);
                hp["typeName"] = "ActionPoint";
                populateCommonDeviceProps(hp, wx, wy, yawRad, epcX, epcY, epcTheta);
                highPrecisionPoints.append(hp);

                saveRouteToServer([pw, wx, wy, epcX, epcY, hp](bool ok) {
                    if (ok) Toast::success(pw, QString("已在设备位置(%1, %2)添加工作站 %3\nepc(%4, %5) 并已保存路网").arg(wx, 0, 'f', 2).arg(wy, 0, 'f', 2).arg(hp.value("name").toString()).arg(epcX, 0, 'f', 2).arg(epcY, 0, 'f', 2));
                });
            }
        }
    });

    tick->start();
    prog->exec();
    prog->deleteLater();
}
