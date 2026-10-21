/**
 * @file    MapCanvasRobotTracker.h
 * @layer   GUI
 * @brief   Real-time AGV pose tracking and SVG marker rendering on MapCanvas.
 * @deps    QObject, QGraphicsScene, QGraphicsItem
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QObject>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QJsonObject>
#include <QMap>
#include <QPixmap>

struct CanvasRobotMarker {
    QString carId;
    QString carName;
    double x = 0;
    double y = 0;
    double angle = 0;
    QGraphicsItem *marker = nullptr;
    QGraphicsTextItem *nameLabel = nullptr;
};

class MapCanvasRobotTracker : public QObject
{
    Q_OBJECT
public:
    explicit MapCanvasRobotTracker(QGraphicsScene *scene, QObject *parent = nullptr);
    ~MapCanvasRobotTracker() override;

    void setScene(QGraphicsScene *scene);
    void setWorldInfo(double left, double bottom, double right, double top, double width, double height);
    void setCarId(const QString &carId);

private slots:
    void onMqttMessage(const QString &topic, const QByteArray &payload);

private:
    void initMqtt();
    void disconnectMqtt();
    void drawRobot(const QString &carId);
    QPointF worldToScene(double wx, double wy) const;

    QGraphicsScene *m_scene = nullptr;
    double m_worldL = 0, m_worldB = 0, m_worldR = 0, m_worldT = 0;
    double m_imgW   = 0, m_imgH   = 0;
    bool   m_worldInfoSet = false;

    QString m_trackedCarId = "1";
    QMap<QString, CanvasRobotMarker> m_carInfos;
    QPixmap m_robotPixmap;
};
