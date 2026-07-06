#ifndef MAPCANVAS_H
#define MAPCANVAS_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QJsonObject>
#include <QMap>
#include <QPixmap>

// Info for rendering robots via MQTT
struct RobotInfo {
    QString carId;
    QString carName;
    double x = 0;
    double y = 0;
    double angle = 0;
    QGraphicsItem *marker = nullptr;
    QGraphicsTextItem *nameLabel = nullptr;
};

class MapCanvas : public QGraphicsView
{
    Q_OBJECT
public:
    explicit MapCanvas(QGraphicsScene *scene, QWidget *parent = nullptr);
    ~MapCanvas() override;

    void setPanWithLeftButton(bool enabled);
    
    // Coordinate transformation setup
    void setWorldInfo(double left, double bottom, double right, double top, double width, double height);
    
    // MQTT Robot Tracking
    void setCarId(const QString &carId); // Single tracking
    
signals:
    void transformChanged();
    void sceneClicked(const QPointF &scenePos, Qt::MouseButton btn);
    void sceneMoved(const QPointF &scenePos);
    void sceneReleased(const QPointF &scenePos, Qt::MouseButton btn);
    void canvasResized(const QSize &sz);

protected:
    void wheelEvent(QWheelEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;

private slots:
    void onMqttMessage(const QString &topic, const QByteArray &payload);

private:
    void drawRobot(const QString &carId);
    QPointF worldToScene(double wx, double wy) const;
    void initMqtt();
    void disconnectMqtt();

private:
    bool   m_panning = false;
    bool   m_panWithLeftButton = true;
    QPoint m_lastMousePos;

    // Center-based rotation state
    bool   m_rotating       = false;
    double m_rotAngle       = 0.0;
    double m_startViewAngle = 0.0;
    QPointF m_rotStartMousePos;  // 按下时鼠标位置(用于计算相对中心的极角)
    
    // World coordinates mapping
    double m_worldL = 0, m_worldB = 0, m_worldR = 0, m_worldT = 0;
    double m_imgW   = 0, m_imgH   = 0;
    bool m_worldInfoSet = false;

    // Robot Tracking
    QString m_trackedCarId;
    QMap<QString, RobotInfo> m_carInfos;
    QPixmap m_robotPixmap;
};

#endif // MAPCANVAS_H
