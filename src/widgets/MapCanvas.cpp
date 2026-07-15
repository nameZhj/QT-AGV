#include "MapCanvas.h"
#include "core/MqttService.h"
#include "core/AppConfig.h"
#include <QWheelEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <cmath>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QDebug>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

MapCanvas::MapCanvas(QGraphicsScene *scene, QWidget *parent)
    : QGraphicsView(scene, parent)
{
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    setDragMode(QGraphicsView::NoDrag);
    setBackgroundBrush(QColor(0x12, 0x15, 0x1a));
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setCursor(Qt::CrossCursor);

    m_robotPixmap.load(":/ic-robot.svg");
}

MapCanvas::~MapCanvas()
{
    disconnectMqtt();
}

void MapCanvas::setPanWithLeftButton(bool enabled)
{
    m_panWithLeftButton = enabled;
}

void MapCanvas::setWorldInfo(double left, double bottom, double right, double top, double width, double height)
{
    m_worldL = left;
    m_worldB = bottom;
    m_worldR = right;
    m_worldT = top;
    m_imgW = width;
    m_imgH = height;
    m_worldInfoSet = true;
}

void MapCanvas::setCarId(const QString &carId)
{
    if (m_trackedCarId == carId) return;
    
    disconnectMqtt();
    m_trackedCarId = carId;
    if (!m_trackedCarId.isEmpty()) {
        initMqtt();
    }
}

QPointF MapCanvas::worldToScene(double wx, double wy) const
{
    if (!m_worldInfoSet) return QPointF(0, 0);
    double resX = (m_worldR - m_worldL) / m_imgW;
    double resY = (m_worldT - m_worldB) / m_imgH;
    if (resX == 0 || resY == 0) return QPointF(0, 0);
    
    double sx = (wx - m_worldL) / resX;
    double sy = m_imgH - (wy - m_worldB) / resY;
    return QPointF(sx, sy);
}

void MapCanvas::wheelEvent(QWheelEvent *e)
{
    // 触摸板与鼠标兼容:
    // - Ctrl + 垂直滚动(鼠标滚轮/触控板双指) → 旋转
    // - Ctrl + 水平滚动(触控板双指水平) → 旋转
    // - 无 Ctrl + 垂直滚动 → 缩放
    // 触摸板的 pixelDelta() 提供更平滑的精度，优先使用
    if (e->modifiers() & Qt::ControlModifier) {
        // 旋转: 优先使用 angleDelta, 触控板用 pixelDelta 补充
        int dy = e->angleDelta().y();
        int dx = e->angleDelta().x();
        // 触控板水平滚动也用于旋转
        int delta = (qAbs(dy) >= qAbs(dx)) ? dy : dx;
        if (delta == 0 && !e->pixelDelta().isNull()) {
            delta = (qAbs(e->pixelDelta().y()) >= qAbs(e->pixelDelta().x()))
                    ? e->pixelDelta().y() : e->pixelDelta().x();
            delta = delta * 2;   // pixelDelta 数值小，放大
        }
        double deg = delta > 0 ? 5.0 : -5.0;
        m_rotAngle += deg;
        setTransformationAnchor(QGraphicsView::AnchorViewCenter);
        rotate(deg);
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        emit transformChanged();
        e->accept();
    } else {
        // 缩放: 鼠标滚轮用 angleDelta(离散15度单位)，触控板用 pixelDelta(连续像素)
        int angleY = e->angleDelta().y();
        if (angleY != 0) {
            double factor = angleY > 0 ? 1.15 : 1.0/1.15;
            scale(factor, factor);
            emit transformChanged();
        } else if (!e->pixelDelta().isNull()) {
            // 触控板双指缩放: pixelDelta 连续值，转换为缩放因子
            double py = e->pixelDelta().y();
            if (qAbs(py) > 0.5) {
                double factor = 1.0 + py * 0.01;
                factor = qBound(0.5, factor, 2.0);
                scale(factor, factor);
                emit transformChanged();
            }
        }
        e->accept();
    }
}

void MapCanvas::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && (e->modifiers() & Qt::ControlModifier)) {
        m_rotating = true;
        // 记录按下时的鼠标位置(用于计算相对中心的极角)
        m_rotStartMousePos = e->pos();
        m_startViewAngle = m_rotAngle;

        setCursor(Qt::SizeAllCursor);
        e->accept();
        return;
    }
    
    if (e->button() == Qt::MiddleButton ||
        (e->button() == Qt::LeftButton && (m_panWithLeftButton || (e->modifiers() & Qt::AltModifier)))) {
        m_panning = true;
        m_lastMousePos = e->pos();
        setCursor(Qt::ClosedHandCursor);
        e->accept();
        return;
    }
    
    QPointF sp = mapToScene(e->pos());
    emit sceneClicked(sp, e->button());
    QGraphicsView::mousePressEvent(e);
}

void MapCanvas::mouseMoveEvent(QMouseEvent *e)
{
    if (m_rotating) {
        // 以视图中心为圆心的拖拽旋转。
        // 使用鼠标相对中心的极角变化驱动旋转，任意方向移动都能响应：
        // 鼠标沿圆周方向移动 → 旋转；沿径向移动 → 角度变化小 → 几乎不旋转
        // (符合主流地图软件的操作直觉: 想象鼠标"抓住"地图边缘绕中心转动)
        QPoint center = viewport()->rect().center();
        QPointF v0 = m_rotStartMousePos - center;   // 按下时的向量
        QPointF v1 = e->pos() - center;             // 当前的向量

        double a0 = std::atan2(v0.y(), v0.x());
        double a1 = std::atan2(v1.y(), v1.x());
        // 角度差归一化到 [-π, π]，避免跨越 ±π 时的跳变
        double delta = a1 - a0;
        while (delta > M_PI)  delta -= 2 * M_PI;
        while (delta < -M_PI) delta += 2 * M_PI;
        double deltaDeg = delta * 180.0 / M_PI;

        double newAngle = m_startViewAngle + deltaDeg;
        double viewDelta = newAngle - m_rotAngle;
        m_rotAngle = newAngle;

        setTransformationAnchor(QGraphicsView::AnchorViewCenter);
        rotate(viewDelta);
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        emit transformChanged();
        e->accept();
        return;
    }

    if (!m_panning) {
        emit sceneMoved(mapToScene(e->pos()));
    }
    
    if (m_panning) {
        QPoint delta = e->pos() - m_lastMousePos;
        m_lastMousePos = e->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        emit transformChanged();
        e->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(e);
    emit transformChanged();
}

void MapCanvas::mouseReleaseEvent(QMouseEvent *e)
{
    if (m_rotating && e->button() == Qt::LeftButton) {
        m_rotating = false;
        setCursor(Qt::CrossCursor);
        e->accept();
        return;
    }
    
    if (m_panning && (e->button() == Qt::MiddleButton || e->button() == Qt::LeftButton)) {
        m_panning = false;
        setCursor(Qt::CrossCursor);
        e->accept();
        return;
    }
    
    QPointF sp = mapToScene(e->pos());
    emit sceneReleased(sp, e->button());
    QGraphicsView::mouseReleaseEvent(e);
}

void MapCanvas::resizeEvent(QResizeEvent *e)
{
    QGraphicsView::resizeEvent(e);
    emit canvasResized(e->size());
}

void MapCanvas::initMqtt()
{
    if (!MqttService::instance().isConnected()) {
        MqttService::instance().connectToHost(AppConfig::instance().serverIp(), 1883);
    }
    
    QString topic = QString("/rob/%1/info").arg(m_trackedCarId);
    MqttService::instance().subscribeTopic(topic, 0, [this](const QString &t, const QByteArray &p) {
        onMqttMessage(t, p);
    });
}

void MapCanvas::disconnectMqtt()
{
    if (!m_trackedCarId.isEmpty() && MqttService::instance().isConnected()) {
        QString topic = QString("/rob/%1/info").arg(m_trackedCarId);
        MqttService::instance().unsubscribeTopic(topic);
    }
}

void MapCanvas::onMqttMessage(const QString &topic, const QByteArray &payload)
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
    
    QString carId;
    if (d.contains("carId")) carId = d.value("carId").toString();
    else if (d.contains("carId")) carId = QString::number(d.value("carId").toInt()); // sometimes it's int
    
    if (carId.isEmpty()) carId = m_trackedCarId;
    
    RobotInfo &info = m_carInfos[carId];
    info.carId = carId;
    info.x = dv({"car_x","carX","x"});
    info.y = dv({"car_y","carY","y"});
    info.angle = dv({"car_yaw","carYaw","yaw"});
    
    if (d.contains("carName")) info.carName = d.value("carName").toString();
    
    drawRobot(carId);
}

void MapCanvas::drawRobot(const QString &carId)
{
    if (!m_worldInfoSet || !scene()) return;
    if (!m_carInfos.contains(carId)) return;
    
    RobotInfo &info = m_carInfos[carId];
    QPointF sp = worldToScene(info.x, info.y);
    
    if (!info.marker) {
        QGraphicsPixmapItem *robotItem = scene()->addPixmap(m_robotPixmap);
        double srcSz = m_robotPixmap.width();
        double iconSz = 30.0;
        robotItem->setScale(iconSz / srcSz);
        double pivot = srcSz / 2.0;
        robotItem->setTransformOriginPoint(pivot, pivot);
        robotItem->setZValue(100);
        info.marker = robotItem;
    }
    
    if (!info.nameLabel && !info.carName.isEmpty()) {
        info.nameLabel = scene()->addText(info.carName);
        info.nameLabel->setDefaultTextColor(Qt::white);
        info.nameLabel->setZValue(101);
    }
    
    if (info.marker) {
        double srcSz = m_robotPixmap.width();
        double pivot = srcSz / 2.0;
        info.marker->setPos(sp.x() - pivot, sp.y() - pivot);
        
        // ENU: yaw=0->East, yaw=PI/2->North. Qt rotate is CW.
        double yawRad = (qAbs(info.angle) > M_PI * 2) ? info.angle * M_PI / 180.0 : info.angle;
        double screenAngle = 90.0 - yawRad * 180.0 / M_PI;
        info.marker->setRotation(screenAngle);
    }
    
    if (info.nameLabel) {
        info.nameLabel->setPos(sp.x() + 10, sp.y() - 20);
    }
}
