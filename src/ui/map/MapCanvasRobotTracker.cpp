/**
 * @file    MapCanvasRobotTracker.cpp
 * @layer   GUI
 * @brief   Real-time AGV pose tracking and SVG marker rendering implementation.
 * @deps    MapCanvasRobotTracker.h, MqttService.h, RobotInfoManager.h, AppConfig.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapCanvasRobotTracker.h"
#include "network/MqttService.h"
#include "app/AppConfig.h"
#include "core/RobotInfoManager.h"
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSvgRenderer>
#include <QPainter>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

MapCanvasRobotTracker::MapCanvasRobotTracker(QGraphicsScene *scene, QObject *parent)
    : QObject(parent), m_scene(scene)
{
    QSvgRenderer renderer(QString(":/ic-robot1.svg"));
    if (renderer.isValid()) {
        m_robotPixmap = QPixmap(180, 330);
        m_robotPixmap.fill(Qt::transparent);
        QPainter rp(&m_robotPixmap);
        rp.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        renderer.render(&rp);
    }
}

MapCanvasRobotTracker::~MapCanvasRobotTracker()
{
    disconnectMqtt();
}

void MapCanvasRobotTracker::setScene(QGraphicsScene *scene)
{
    m_scene = scene;
}

void MapCanvasRobotTracker::setWorldInfo(double left, double bottom, double right, double top, double width, double height)
{
    m_worldL = left;
    m_worldB = bottom;
    m_worldR = right;
    m_worldT = top;
    m_imgW = width;
    m_imgH = height;
    m_worldInfoSet = true;
}

void MapCanvasRobotTracker::setCarId(const QString &carId)
{
    if (m_trackedCarId == carId) return;
    disconnectMqtt();
    m_trackedCarId = carId;
    if (!m_trackedCarId.isEmpty()) {
        initMqtt();
    }
}

QPointF MapCanvasRobotTracker::worldToScene(double wx, double wy) const
{
    if (!m_worldInfoSet) return QPointF(0, 0);
    double resX = (m_worldR - m_worldL) / m_imgW;
    double resY = (m_worldT - m_worldB) / m_imgH;
    if (resX == 0 || resY == 0) return QPointF(0, 0);

    double sx = (wx - m_worldL) / resX;
    double sy = m_imgH - (wy - m_worldB) / resY;
    return QPointF(sx, sy);
}

void MapCanvasRobotTracker::initMqtt()
{
    if (!MqttService::instance().isConnected()) {
        MqttService::instance().connectToHost(AppConfig::instance().serverIp(), 1883);
    }

    m_trackedCarId = QStringLiteral("1");
    MqttService::instance().subscribeTopic(QStringLiteral("/rob/1/info"), 0, [this](const QString &t, const QByteArray &p) {
        onMqttMessage(t, p);
    });
}

void MapCanvasRobotTracker::disconnectMqtt()
{
    if (MqttService::instance().isConnected()) {
        MqttService::instance().unsubscribeTopic(QStringLiteral("/rob/1/info"));
    }
}

void MapCanvasRobotTracker::onMqttMessage(const QString &topic, const QByteArray &payload)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return;

    QJsonObject d = doc.object();

    auto dv = [&](const QStringList &keys, double def = 0) -> double {
        for (const QString &k : keys) {
            QJsonValue v = d.value(k);
            if (!v.isUndefined() && !v.isNull()) return v.toDouble(def);
        }
        return def;
    };

    RobotInfoManager::instance().updateFromPayload(topic, payload);

    QString carId;
    if (d.contains("carId")) carId = d.value("carId").toString();
    else if (d.contains("carId")) carId = QString::number(d.value("carId").toInt());

    if (carId.isEmpty()) carId = m_trackedCarId;
    if (carId.isEmpty()) carId = "1";

    CanvasRobotMarker &info = m_carInfos[carId];
    info.carId = carId;
    info.x = dv({"car_x", "carX", "x"});
    info.y = dv({"car_y", "carY", "y"});
    info.angle = dv({"car_yaw", "carYaw", "yaw"});

    if (d.contains("carName")) info.carName = d.value("carName").toString();

    drawRobot(carId);
}

void MapCanvasRobotTracker::drawRobot(const QString &carId)
{
    if (!m_worldInfoSet || !m_scene || !m_carInfos.contains(carId)) return;

    CanvasRobotMarker &info = m_carInfos[carId];
    QPointF sp = worldToScene(info.x, info.y);

    double mapW = m_worldR - m_worldL;
    double mapH = m_worldT - m_worldB;
    double ppm = 20.0;
    if (m_imgW > 0 && mapW > 0 && m_imgH > 0 && mapH > 0) {
        ppm = ((double)m_imgW / mapW + (double)m_imgH / mapH) / 2.0;
    } else if (m_imgW > 0 && mapW > 0) {
        ppm = (double)m_imgW / mapW;
    }

    const double kRobotLength = 1.0;
    const double kRobotWidth  = 0.5;
    double sceneW = kRobotWidth * ppm;
    double sceneH = kRobotLength * ppm;

    double srcW   = m_robotPixmap.width();
    double srcH   = m_robotPixmap.height();
    double pivotX = srcW / 2.0;
    double pivotY = srcH / 2.0;
    double scale  = (srcW > 0) ? (sceneW / srcW) : 1.0;

    double yawRad = (qAbs(info.angle) > M_PI * 2) ? info.angle * M_PI / 180.0 : info.angle;
    double screenAngle = 90.0 - yawRad * 180.0 / M_PI;

    if (!info.marker) {
        QGraphicsPixmapItem *robotItem = m_scene->addPixmap(m_robotPixmap);
        robotItem->setScale(scale);
        robotItem->setTransformOriginPoint(pivotX, pivotY);
        robotItem->setPos(sp.x() - pivotX, sp.y() - pivotY);
        robotItem->setRotation(screenAngle);
        robotItem->setZValue(100);
        info.marker = robotItem;
    }

    if (!info.nameLabel && !info.carName.isEmpty()) {
        info.nameLabel = m_scene->addText(info.carName);
        info.nameLabel->setDefaultTextColor(Qt::white);
        info.nameLabel->setZValue(101);
    }

    if (info.marker) {
        info.marker->setScale(scale);
        info.marker->setTransformOriginPoint(pivotX, pivotY);
        info.marker->setPos(sp.x() - pivotX, sp.y() - pivotY);
        info.marker->setRotation(screenAngle);
    }

    if (info.nameLabel) {
        info.nameLabel->setPos(sp.x() + sceneW * 0.5 + 4, sp.y() - sceneH * 0.5 - 12);
    }
}
