/**
 * @file    MapEditTelemetryController.h
 * @layer   GUI
 * @brief   Controller managing MQTT device telemetry, robot pose markers, and teleoperation override.
 * @deps    QObject, QGraphicsScene, QGraphicsPixmapItem, QPixmap, QPointF
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QObject>
#include <QPointF>
#include <QPixmap>
#include <QString>
#include <functional>

class QGraphicsScene;
class QGraphicsPixmapItem;
class QWidget;
class QPushButton;

class MapEditTelemetryController : public QObject
{
    Q_OBJECT

public:
    using CoordToSceneFn = std::function<QPointF(double wx, double wy)>;

    explicit MapEditTelemetryController(QObject *parent = nullptr);
    ~MapEditTelemetryController() override;

    void init(QGraphicsScene *scene, CoordToSceneFn w2s);
    void setMapId(const QString &mapId) { m_mapId = mapId; }
    void setMapDimensions(double wL, double wR, double wB, double wT, int imgW, int imgH);
    void setRobotVisible(bool visible);

    bool hasCarPose() const { return m_hasCarPose; }
    double carX() const { return m_carX; }
    double carY() const { return m_carY; }
    double carYaw() const { return m_carYaw; }
    double carTheta() const { return m_carTheta; }
    QString currentCarId() const { return m_currentCarId; }

    void loadCarList();
    void initMqtt();
    void disconnectMqtt();
    void updateRobotMarker();

    void toggleTeleop(QPushButton *teleopBtn);
    void onTeleopActiveChanged(bool active, QPushButton *teleopBtn);
    void addDevicePoint(const QString &type, QWidget *parent,
                        std::function<void(const QPointF &sp, double epcX, double epcY, double epcTheta, double yaw, double theta)> onAddPoint);

signals:
    void robotMarkerUpdated();

private:
    QGraphicsScene       *m_scene = nullptr;
    CoordToSceneFn        m_w2s;
    QGraphicsPixmapItem  *m_robotItem = nullptr;
    QPixmap               m_robotPixmap;

    QString               m_mapId;
    QString               m_currentCarId = QStringLiteral("1");
    bool                  m_showRobotPosition = false;
    bool                  m_hasCarPose = false;
    bool                  m_hasEpcPose = false;
    bool                  m_devicePointPending = false;

    double                m_carX = 0.0;
    double                m_carY = 0.0;
    double                m_carYaw = 0.0;
    double                m_carTheta = 0.0;
    double                m_epcX = 0.0;
    double                m_epcY = 0.0;
    double                m_epcTheta = 0.0;

    double                m_worldL = 0.0;
    double                m_worldR = 0.0;
    double                m_worldB = 0.0;
    double                m_worldT = 0.0;
    int                   m_imgW = 0;
    int                   m_imgH = 0;
};
