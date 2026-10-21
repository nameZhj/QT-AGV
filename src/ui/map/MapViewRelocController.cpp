/**
 * @file    MapViewRelocController.cpp
 * @layer   GUI
 * @brief   Relocalization controller implementation.
 * @deps    MapViewRelocController.h, ApiService.h, LoadingDialog.h, Toast.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapViewRelocController.h"
#include "network/ApiService.h"
#include "ui/common/LoadingDialog.h"
#include "ui/common/Toast.h"
#include <QGraphicsScene>
#include <QGraphicsLineItem>
#include <QGraphicsEllipseItem>
#include <QJsonObject>
#include <QJsonArray>
#include <cmath>

static QJsonValue numOrStr(const QString &s)
{
    bool ok = false;
    qint64 v = s.toLongLong(&ok);
    return ok ? QJsonValue(v) : QJsonValue(s);
}

MapViewRelocController::MapViewRelocController(QObject *parent)
    : QObject(parent)
{
}

MapViewRelocController::~MapViewRelocController()
{
}

void MapViewRelocController::enterRelocMode()
{
    m_relocMode = true;
    m_relocDragging = false;
    emit relocModeChanged(true);
}

void MapViewRelocController::exitRelocMode(QGraphicsScene *scene)
{
    m_relocMode = false;
    m_relocDragging = false;
    clearPreview(scene);
    emit relocModeChanged(false);
}

void MapViewRelocController::clearPreview(QGraphicsScene *scene)
{
    if (m_poseDotItem) {
        if (scene && m_poseDotItem->scene() == scene) scene->removeItem(m_poseDotItem);
        delete m_poseDotItem;
        m_poseDotItem = nullptr;
    }
    if (m_poseArrowItem) {
        if (scene && m_poseArrowItem->scene() == scene) scene->removeItem(m_poseArrowItem);
        delete m_poseArrowItem;
        m_poseArrowItem = nullptr;
    }
}

bool MapViewRelocController::handlePress(const QPointF &worldPos, const QPointF &scenePos, QGraphicsScene *scene,
                                        const QString &carId, std::function<void(const QString &, const QString &)> setStatus)
{
    if (!m_relocMode) return false;
    if (carId.isEmpty()) {
        if (setStatus) setStatus("无设备ID，无法重定位", "#e05555");
        return true;
    }

    m_relocDragging   = true;
    m_relocStartWorld = worldPos;

    clearPreview(scene);

    if (scene) {
        auto *dot = new QGraphicsEllipseItem(scenePos.x() - 6, scenePos.y() - 6, 12, 12);
        dot->setBrush(QColor(255, 165, 0));
        dot->setPen(Qt::NoPen);
        dot->setZValue(30);
        m_poseDotItem = dot;
        scene->addItem(m_poseDotItem);
    }

    if (setStatus) setStatus("拖拽以设定朝向，松开发送重定位", "#4a9eff");
    return true;
}

void MapViewRelocController::handleMove(const QPointF &scenePos, QGraphicsScene *scene,
                                       std::function<QPointF(double, double)> worldToScene)
{
    if (!m_relocDragging || !scene || !worldToScene) return;

    QPointF startScene = worldToScene(m_relocStartWorld.x(), m_relocStartWorld.y());
    if (m_poseArrowItem) {
        if (m_poseArrowItem->scene() == scene) scene->removeItem(m_poseArrowItem);
        delete m_poseArrowItem;
        m_poseArrowItem = nullptr;
    }

    auto *line = new QGraphicsLineItem(startScene.x(), startScene.y(), scenePos.x(), scenePos.y());
    line->setPen(QPen(QColor(255, 165, 0), 2, Qt::SolidLine, Qt::RoundCap));
    line->setZValue(30);
    m_poseArrowItem = line;
    scene->addItem(m_poseArrowItem);
}

void MapViewRelocController::handleRelease(const QPointF &worldPos, const QPointF &, QGraphicsScene *scene,
                                          const QString &carId, const QString &mapId, double fallbackYaw,
                                          std::function<void(const QString &, const QString &)> setStatus,
                                          std::function<void()> onFinished, QWidget *parent)
{
    if (!m_relocDragging) return;
    m_relocDragging = false;

    clearPreview(scene);

    double dx = worldPos.x() - m_relocStartWorld.x();
    double dy = worldPos.y() - m_relocStartWorld.y();
    double yaw = (dx * dx + dy * dy > 0.01) ? std::atan2(dy, dx) : fallbackYaw;

    QString cid = carId.isEmpty() ? "1" : carId;
    QJsonObject poseObj;
    QJsonArray coords = { m_relocStartWorld.x(), m_relocStartWorld.y(), 0.0 };
    poseObj["coordinates"] = coords;
    poseObj["yaw"]         = yaw;

    QJsonObject msg;
    msg["action"] = "initialpose";
    msg["car_id"] = numOrStr(cid);
    msg["map_id"] = numOrStr(mapId);
    msg["pose"]   = poseObj;

    if (setStatus) setStatus("正在发送重定位...", "#4a9eff");
    auto *loading = new LoadingDialog("正在发送重定位...", parent);
    loading->show();

    ApiService::instance().sendMqtt(msg, [this, loading, setStatus, onFinished, parent, scene](int code, const QJsonObject &) {
        loading->close();
        loading->deleteLater();
        if (code == 200) {
            Toast::success(parent, "重定位消息已发送，请稍等...");
            if (setStatus) setStatus("重定位消息已发送，请稍等...", "#4a9eff");
        } else {
            if (setStatus) setStatus("重定位失败", "#e05555");
        }
        exitRelocMode(scene);
        if (onFinished) onFinished();
    });
}
