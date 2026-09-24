#include "MapCanvas.h"
#include "core/MqttService.h"
#include "core/AppConfig.h"
#include "core/RobotInfoManager.h"
#include <QWheelEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QNativeGestureEvent>
#include <cmath>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QDebug>
#include <QGestureEvent>
#include <QPinchGesture>
#include <QTimer>
#include <QSvgRenderer>
#include <QPainter>

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

    // Load SVG robot icon at 180×330 (matching 60×110 aspect ratio)
    QSvgRenderer renderer(QString(":/ic-robot1.svg"));
    if (renderer.isValid()) {
        m_robotPixmap = QPixmap(180, 330);
        m_robotPixmap.fill(Qt::transparent);
        QPainter rp(&m_robotPixmap);
        rp.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        renderer.render(&rp);
    }
    
    setAttribute(Qt::WA_AcceptTouchEvents);
    viewport()->setAttribute(Qt::WA_AcceptTouchEvents);
    viewport()->grabGesture(Qt::PinchGesture);
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
    // =========================================================================
    // Windows 触控板 WheelEvent 事件特征（实测）：
    //
    // ① 双指捏合/扩大 → 系统发出的 WheelEvent 特征：
    //      - source() 可能是 SynthesizedBySystem，也可能是 NotSynthesized（取决于驱动）
    //      - modifiers() 包含 Qt::ControlModifier（系统注入）
    //      - angleDelta().y() 是 120 的整数倍（每步 ±120）
    //      - pixelDelta() 为 null（捏合是离散步长，不是连续滑动）
    //   → 处理为：缩放地图
    //
    // ② 双指在触控板上滑动（无 Ctrl）→ WheelEvent 特征：
    //      - angleDelta 通常不是 120 的整数倍（小数值，连续）
    //        OR pixelDelta 有值（精确触控板驱动）
    //      - modifiers() 无 ControlModifier
    //   → 处理为：平移地图
    //
    // ③ Ctrl + 双指滑动 → WheelEvent 特征：
    //      - pixelDelta 有值，且值连续（精确触控板）
    //        OR angleDelta 不是 120 的整数倍
    //      - modifiers() 包含 ControlModifier
    //   → 处理为：旋转地图
    //
    // ④ 鼠标滚轮 → WheelEvent 特征：
    //      - angleDelta 是 120 的整数倍
    //      - pixelDelta 为 null
    //      - modifiers() 通常无 Ctrl（除非用户手动按 Ctrl）
    //   → 处理为：缩放（无Ctrl）或旋转（有Ctrl）
    //
    // 关键区分逻辑：
    //   捏合 vs Ctrl+滑动：pixelDelta 是否有值
    //   - 捏合：pixelDelta 为 null，angleDelta = ±120 倍数
    //   - Ctrl+滑动：pixelDelta 有值（连续小数值），angleDelta 非 120 倍数
    // =========================================================================

    bool hasCtrl = (e->modifiers() & Qt::ControlModifier);
    bool hasPixelDelta = !e->pixelDelta().isNull();
    int  angleY = e->angleDelta().y();
    int  angleX = e->angleDelta().x();

    // 判断是否为精确触控板连续滑动（非离散步长）
    // 精确触控板：pixelDelta 有值，或 angleDelta 不是 120 的整数倍
    bool isContinuousScroll = hasPixelDelta ||
                              (angleX != 0 && (angleX % 120) != 0) ||
                              (angleY != 0 && (angleY % 120) != 0);

    if (hasCtrl) {
        if (isContinuousScroll) {
            // ③ Ctrl + 触控板滑动 → 旋转地图（线性，温和）
            double dx = hasPixelDelta ? e->pixelDelta().x() : (angleX * 0.3);
            double dy = hasPixelDelta ? e->pixelDelta().y() : (angleY * 0.3);
            double delta = (qAbs(dx) > qAbs(dy)) ? dx : dy;
            if (qAbs(delta) > 0.05) {
                const double kRotateDeg = 0.04;
                double deg = delta * kRotateDeg;
                m_rotAngle += deg;
                setTransformationAnchor(QGraphicsView::AnchorViewCenter);
                rotate(deg);
                setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
                emit transformChanged();
            }
            e->accept();
        } else {
            // ① 双指捏合/扩大（离散步长 Ctrl+Wheel）OR ④ Ctrl+鼠标滚轮 → 缩放
            int delta = (qAbs(angleY) >= qAbs(angleX)) ? angleY : angleX;
            if (delta != 0) {
                // 触控板捏合：温和缩放；鼠标：正常缩放
                double factor = delta > 0 ? 1.12 : (1.0 / 1.12);
                setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
                scale(factor, factor);
                emit transformChanged();
            }
            e->accept();
        }
    } else {
        if (isContinuousScroll) {
            // ② 触控板双指滑动（无 Ctrl）→ 平移地图（线性，温和）
            double dx = hasPixelDelta ? e->pixelDelta().x() : (angleX * 0.3);
            double dy = hasPixelDelta ? e->pixelDelta().y() : (angleY * 0.3);
            const double kPanDamp = 0.5;
            dx *= kPanDamp;
            dy *= kPanDamp;
            if (qAbs(dx) > 0.05 || qAbs(dy) > 0.05) {
                horizontalScrollBar()->setValue(horizontalScrollBar()->value() - dx);
                verticalScrollBar()->setValue(verticalScrollBar()->value() - dy);
                emit transformChanged();
            }
            e->accept();
        } else {
            // ④ 鼠标滚轮（无 Ctrl）→ 缩放地图
            if (angleY != 0) {
                double factor = angleY > 0 ? 1.15 : (1.0 / 1.15);
                setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
                scale(factor, factor);
                emit transformChanged();
            }
            e->accept();
        }
    }
}


bool MapCanvas::viewportEvent(QEvent *event)
{
    if (event->type() == QEvent::Gesture) {
        QGestureEvent *ge = static_cast<QGestureEvent*>(event);
        if (QGesture *pinch = ge->gesture(Qt::PinchGesture)) {
            pinchTriggered(static_cast<QPinchGesture*>(pinch));
            return true;
        }
    }
    return QGraphicsView::viewportEvent(event);
}

void MapCanvas::pinchTriggered(QPinchGesture *gesture)
{
    QPinchGesture::ChangeFlags changeFlags = gesture->changeFlags();
    if (changeFlags & QPinchGesture::ScaleFactorChanged) {
        double factor = gesture->scaleFactor();
        // (1) 双指捏合扩大：缩放地图 (线性且温和)
        double sensitiveFactor = 1.0 + (factor - 1.0) * 0.35;
        sensitiveFactor = qBound(0.95, sensitiveFactor, 1.05);
        scale(sensitiveFactor, sensitiveFactor);
        emit transformChanged();

        m_justPinched = true;
        QTimer::singleShot(200, this, [this]() { m_justPinched = false; });
    }
}

bool MapCanvas::event(QEvent *e)
{
    // 处理原生手势事件(Windows 触控板捏合/旋转)
    if (e->type() == QEvent::NativeGesture) {
        auto *nge = static_cast<QNativeGestureEvent*>(e);
        handleNativeGesture(nge);
        return true;
    }
    return QGraphicsView::event(e);
}

void MapCanvas::handleNativeGesture(QNativeGestureEvent *e)
{
    switch (e->gestureType()) {
    case Qt::BeginNativeGesture:
        // 手势开始，重置状态
        m_gestureZoomActive = false;
        m_gestureRotateActive = false;
        break;
    case Qt::EndNativeGesture:
        // 手势结束
        m_gestureZoomActive = false;
        m_gestureRotateActive = false;
        break;
    case Qt::ZoomNativeGesture: {
        // (1) 双指捏合扩大/缩小：缩放地图
        // value > 0 = 放大(手指分开), value < 0 = 缩小(手指捏合)
        double value = e->value();
        if (qAbs(value) < 1e-6) break;
        // 线性缩放，value 通常在 [-0.1, 0.1] 范围
        double factor = 1.0 + value * 1.5;
        factor = qBound(0.95, factor, 1.05);
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        scale(factor, factor);
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        emit transformChanged();
        m_gestureZoomActive = true;
        m_justPinched = true;
        QTimer::singleShot(200, this, [this]() { m_justPinched = false; });
        break;
    }
    case Qt::RotateNativeGesture: {
        // 双指旋转手势 - 不响应(用户要求用 Ctrl+单指滑动旋转)
        // 忽略此手势，避免与 Ctrl+单指滑动旋转冲突
        break;
    }
    case Qt::PanNativeGesture: {
        // 三指/四指平移手势 - 不处理(系统级手势)
        break;
    }
    case Qt::SmartZoomNativeGesture:
        // 智能缩放(双击双指) - 不处理
        break;
    case Qt::SwipeNativeGesture:
        // 滑动手势 - 不处理
        break;
    default:
        break;
    }
}

void MapCanvas::mousePressEvent(QMouseEvent *e)
{
    QPointF sp = mapToScene(e->pos());
    emit sceneClicked(sp, e->button());

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
    
    QGraphicsView::mousePressEvent(e);
}

void MapCanvas::mouseDoubleClickEvent(QMouseEvent *e)
{
    emit sceneDoubleClicked(mapToScene(e->pos()), e->button());
    e->accept();
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
    
    QString carId = m_trackedCarId.trimmed().isEmpty() ? "1" : m_trackedCarId.trimmed();
    m_trackedCarId = carId;
    MqttService::instance().subscribeTopic("/info", 0, [this](const QString &t, const QByteArray &p) { onMqttMessage(t, p); });
    MqttService::instance().subscribeTopic("info", 0, [this](const QString &t, const QByteArray &p) { onMqttMessage(t, p); });
    MqttService::instance().subscribeTopic(QString("/rob/%1/info").arg(carId), 0, [this](const QString &t, const QByteArray &p) {
        onMqttMessage(t, p);
    });
}

void MapCanvas::disconnectMqtt()
{
    MqttService::instance().unsubscribeTopic("/info");
    MqttService::instance().unsubscribeTopic("info");
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
    
    RobotInfoManager::instance().updateFromPayload(topic, payload);

    QString carId;
    if (d.contains("carId")) carId = d.value("carId").toString();
    else if (d.contains("carId")) carId = QString::number(d.value("carId").toInt()); // sometimes it's int
    
    if (carId.isEmpty()) carId = m_trackedCarId;
    if (carId.isEmpty()) carId = "1";
    
    CanvasRobotMarker &info = m_carInfos[carId];
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
    
    CanvasRobotMarker &info = m_carInfos[carId];
    QPointF sp = worldToScene(info.x, info.y);
    
    // 地图比例尺计算: ppm (pixels per meter)
    double mapW = m_worldR - m_worldL;
    double mapH = m_worldT - m_worldB;
    double ppm = 20.0; // 默认备用比例尺 0.05m/px
    if (m_imgW > 0 && mapW > 0 && m_imgH > 0 && mapH > 0) {
        ppm = ((double)m_imgW / mapW + (double)m_imgH / mapH) / 2.0;
    } else if (m_imgW > 0 && mapW > 0) {
        ppm = (double)m_imgW / mapW;
    }

    // 真实尺寸: 车长 1.0m (纵向/车头朝向), 车宽 0.5m (横向)
    const double kRobotLength = 1.0;
    const double kRobotWidth  = 0.5;
    double sceneW = kRobotWidth * ppm;
    double sceneH = kRobotLength * ppm;

    double srcW   = m_robotPixmap.width();
    double srcH   = m_robotPixmap.height();
    double pivotX = srcW / 2.0;
    double pivotY = srcH / 2.0;
    double scale  = (srcW > 0) ? (sceneW / srcW) : 1.0;

    // ENU: yaw=0->East, yaw=PI/2->North. Qt rotate is CW.
    double yawRad = (qAbs(info.angle) > M_PI * 2) ? info.angle * M_PI / 180.0 : info.angle;
    double screenAngle = 90.0 - yawRad * 180.0 / M_PI;

    if (!info.marker) {
        QGraphicsPixmapItem *robotItem = scene()->addPixmap(m_robotPixmap);
        robotItem->setScale(scale);
        robotItem->setTransformOriginPoint(pivotX, pivotY);
        robotItem->setPos(sp.x() - pivotX, sp.y() - pivotY);
        robotItem->setRotation(screenAngle);
        robotItem->setZValue(100);
        info.marker = robotItem;
    }
    
    if (!info.nameLabel && !info.carName.isEmpty()) {
        info.nameLabel = scene()->addText(info.carName);
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
