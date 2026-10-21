/**
 * @file    MapEditTelemetryController.cpp
 * @layer   GUI
 * @brief   Implementation of MQTT telemetry, robot pose marker, and teleop control.
 * @deps    MapEditTelemetryController.h, MapEditTypes.h, ApiService.h, MqttService.h, AppConfig.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapEditTelemetryController.h"
#include "MapEditTypes.h"
#include "network/ApiService.h"
#include "network/MqttService.h"
#include "app/AppConfig.h"
#include "core/RobotInfoManager.h"
#include "core/TeleopController.h"
#include "ui/common/Toast.h"

#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QPushButton>
#include <QProgressDialog>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>

using namespace MapEdit;

MapEditTelemetryController::MapEditTelemetryController(QObject *parent)
    : QObject(parent)
{
}

MapEditTelemetryController::~MapEditTelemetryController()
{
    disconnectMqtt();
}

void MapEditTelemetryController::init(QGraphicsScene *scene, CoordToSceneFn w2s)
{
    m_scene = scene;
    m_w2s = w2s;
    m_robotPixmap.load(":/car.png");
    if (m_scene && !m_robotPixmap.isNull()) {
        m_robotItem = m_scene->addPixmap(m_robotPixmap);
        m_robotItem->setZValue(200);
        m_robotItem->setVisible(false);
    }
}

void MapEditTelemetryController::setMapDimensions(double wL, double wR, double wB, double wT, int imgW, int imgH)
{
    m_worldL = wL; m_worldR = wR; m_worldB = wB; m_worldT = wT;
    m_imgW = imgW; m_imgH = imgH;
    updateRobotMarker();
}

void MapEditTelemetryController::setRobotVisible(bool visible)
{
    m_showRobotPosition = visible;
    updateRobotMarker();
}

void MapEditTelemetryController::loadCarList()
{
    ApiService::instance().listCarAll([this](int code, const QJsonObject &data) {
        if (code != 200) {
            initMqtt();
            return;
        }
        initMqtt();
    });
}

void MapEditTelemetryController::initMqtt()
{
    QString serverIp = AppConfig::instance().serverIp();
    if (serverIp.isEmpty()) return;
    if (!MqttService::instance().isConnected()) {
        MqttService::instance().connectToHost(serverIp, 1883);
    }

    auto handleInfoPayload = [this](const QString &topic, const QByteArray &payload) {
        QJsonParseError err;
        auto doc = QJsonDocument::fromJson(payload, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) return;
        QJsonObject d = doc.object();
        QJsonObject dataObj = d.value("data").toObject();
        QJsonObject poseObj = d.value("pose").toObject();

        RobotInfoManager::instance().updateFromPayload(topic, payload);

        auto dv = [&](const QStringList &keys, double def = 0) -> double {
            for (const QString &k : keys) {
                if (d.contains(k) && !d.value(k).isNull()) return d.value(k).toDouble(def);
                if (!dataObj.isEmpty() && dataObj.contains(k) && !dataObj.value(k).isNull()) return dataObj.value(k).toDouble(def);
                if (!poseObj.isEmpty() && poseObj.contains(k) && !poseObj.value(k).isNull()) return poseObj.value(k).toDouble(def);
            }
            return def;
        };

        QJsonValue mapIdV = d.contains("map_id") ? d.value("map_id") : d.value("mapId");
        if (mapIdV.isUndefined() && !dataObj.isEmpty()) {
            mapIdV = dataObj.contains("map_id") ? dataObj.value("map_id") : dataObj.value("mapId");
        }
        int msgMapId = mapIdV.toVariant().toInt();
        int curMapId = m_mapId.toInt();
        if (msgMapId > 0 && curMapId > 0 && msgMapId != curMapId) return;

        m_carX = dv({"carX", "car_x", "x", "current_x", "posX"});
        m_carY = dv({"carY", "car_y", "y", "current_y", "posY"});
        m_carYaw = dv({"carYaw", "car_yaw", "yaw", "carTheta", "theta", "angle"});
        m_carTheta = dv({"theta", "car_theta", "carTheta", "angle"}, m_carYaw * kRadToDeg);
        m_hasCarPose = true;

        if (d.contains("epc_x") || d.contains("epc_y") || (!dataObj.isEmpty() && (dataObj.contains("epc_x") || dataObj.contains("epc_y")))) {
            m_epcX = dv({"epc_x", "epcX"}, 0);
            m_epcY = dv({"epc_y", "epcY"}, 0);
            m_epcTheta = dv({"epc_theta", "epc_Theta", "epcTheta"}, 0);
            m_hasEpcPose = true;
        }
        updateRobotMarker();
    };

    m_currentCarId = QStringLiteral("1");
    MqttService::instance().subscribeTopic(QStringLiteral("/rob/1/info"), 0, handleInfoPayload);
}

void MapEditTelemetryController::disconnectMqtt()
{
    MqttService::instance().unsubscribeTopic(QStringLiteral("/rob/1/info"));
    m_hasCarPose = false;
    m_hasEpcPose = false;
    m_devicePointPending = false;
    updateRobotMarker();
}

void MapEditTelemetryController::updateRobotMarker()
{
    if (!m_robotItem) return;
    if (!m_showRobotPosition || !m_hasCarPose || !m_w2s) {
        m_robotItem->setVisible(false);
        return;
    }
    QPointF sp = m_w2s(m_carX, m_carY);

    double mapW = m_worldR - m_worldL, mapH = m_worldT - m_worldB;
    double ppm = 20.0;
    if (m_imgW > 0 && mapW > 0 && m_imgH > 0 && mapH > 0) ppm = ((double)m_imgW / mapW + (double)m_imgH / mapH) / 2.0;
    else if (m_imgW > 0 && mapW > 0) ppm = (double)m_imgW / mapW;

    const double kRobotLength = 1.0, kRobotWidth = 0.5;
    double sceneW = kRobotWidth * ppm;
    double srcW = m_robotPixmap.width(), srcH = m_robotPixmap.height();
    double pivotX = srcW / 2.0, pivotY = srcH / 2.0;
    double scale = (srcW > 0) ? (sceneW / srcW) : 1.0;

    m_robotItem->setScale(scale);
    m_robotItem->setTransformOriginPoint(pivotX, pivotY);
    m_robotItem->setPos(sp.x() - pivotX, sp.y() - pivotY);

    double yawRad = (std::abs(m_carYaw) > kPi * 2.0) ? (m_carYaw * kDegToRad) : m_carYaw;
    m_robotItem->setRotation(90.0 - (yawRad * kRadToDeg));
    m_robotItem->setVisible(true);
    emit robotMarkerUpdated();
}

void MapEditTelemetryController::toggleTeleop(QPushButton *teleopBtn)
{
    Q_UNUSED(teleopBtn);
    TeleopController::instance().setActive(!TeleopController::instance().isActive());
}

void MapEditTelemetryController::onTeleopActiveChanged(bool active, QPushButton *teleopBtn)
{
    if (!teleopBtn) return;
    if (active) {
        teleopBtn->setStyleSheet(
            "QPushButton{color:white;border:none;border-radius:4px;"
            "padding:4px 12px;font-size:12px;background:#c0392b;}"
            "QPushButton:hover{background:#e74c3c;}");
    } else {
        teleopBtn->setStyleSheet(
            "QPushButton{color:white;border:none;border-radius:4px;"
            "padding:4px 12px;font-size:12px;background:#1e6fd4;}"
            "QPushButton:hover{background:#2980e8;}");
    }
}

void MapEditTelemetryController::addDevicePoint(const QString &type, QWidget *parent,
                                               std::function<void(const QPointF &sp, double epcX, double epcY, double epcTheta, double yaw, double theta)> onAddPoint)
{
    if (m_devicePointPending) { Toast::info(parent, "上一次设备打点尚未完成，请稍候"); return; }
    if (!m_hasCarPose) { Toast::info(parent, "未收到设备位置信息，无法打点"); return; }

    m_devicePointPending = true;
    QJsonObject data;
    data["point_type"] = (type == "charge") ? "charge" : "highPrecision";
    data["sender"] = "Web";
    MqttService::instance().publish(QString("/rob/%1/pointType").arg(m_currentCarId),
                                    QJsonDocument(data).toJson(QJsonDocument::Compact));

    const int kWaitMs = 3000, kStepMs = 100;
    auto *prog = new QProgressDialog("正在获取设备相对定位 (epc)...\n剩余 3 秒", QString(), 0, kWaitMs / kStepMs, parent);
    prog->setWindowTitle("设备打点");
    prog->setWindowModality(Qt::WindowModal);
    prog->setCancelButton(nullptr);

    auto *tick = new QTimer(prog);
    tick->setInterval(kStepMs);
    int elapsed = 0;
    QObject::connect(tick, &QTimer::timeout, parent, [prog, tick, kStepMs, kWaitMs, &elapsed, type, parent, onAddPoint, this]() {
        elapsed += kStepMs;
        int remainSec = (kWaitMs - elapsed + 999) / 1000;
        prog->setLabelText(QString("正在获取设备相对定位 (epc)...\n剩余 %1 秒").arg(remainSec));
        prog->setValue(elapsed / kStepMs);

        if (elapsed >= kWaitMs) {
            tick->stop();
            prog->accept();
            m_devicePointPending = false;
            if (!m_hasCarPose) {
                Toast::error(parent, "设备位置信息丢失，打点失败");
                return;
            }
            if (m_w2s && onAddPoint) {
                QPointF sp = m_w2s(m_carX, m_carY);
                onAddPoint(sp, m_epcX, m_epcY, m_epcTheta, m_carYaw, m_carTheta);
                Toast::success(parent, QString("已在设备位置(%1, %2)添加点\nepc(%3, %4)")
                    .arg(m_carX, 0, 'f', 2).arg(m_carY, 0, 'f', 2).arg(m_epcX, 0, 'f', 2).arg(m_epcY, 0, 'f', 2));
            }
        }
    });
    tick->start();
    prog->exec();
    prog->deleteLater();
}
