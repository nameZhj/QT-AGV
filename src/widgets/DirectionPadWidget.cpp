#include "DirectionPadWidget.h"
#include "ManualControlConfigDialog.h"
#include "core/TeleopController.h"

#include <QPainter>
#include <QMouseEvent>
#include <QTransform>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

DirectionPadWidget::DirectionPadWidget(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(130, 130);
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover, true);
    setFocusPolicy(Qt::NoFocus);

    updatePaths();

    // 联动全局 TeleopController 的按键状态 (无论是物理按键还是虚拟按键)
    connect(&TeleopController::instance(), &TeleopController::keyStateChanged,
            this, &DirectionPadWidget::onTeleopKeyStateChanged);

    // 初始状态同步
    if (TeleopController::instance().isKeyPressed(Qt::Key_W)) m_pressedKeys.insert(PadKey::Up);
    if (TeleopController::instance().isKeyPressed(Qt::Key_S)) m_pressedKeys.insert(PadKey::Down);
    if (TeleopController::instance().isKeyPressed(Qt::Key_A)) m_pressedKeys.insert(PadKey::Left);
    if (TeleopController::instance().isKeyPressed(Qt::Key_D)) m_pressedKeys.insert(PadKey::Right);
    if (TeleopController::instance().isKeyPressed(Qt::Key_Space)) m_pressedKeys.insert(PadKey::Center);
}

void DirectionPadWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // 当该组件所在的地图界面显示时，确保 TeleopController 处于可用使能状态
    if (!TeleopController::instance().isActive()) {
        TeleopController::instance().setActive(true);
    }
}

void DirectionPadWidget::updatePaths()
{
    // 1. 上正三角 (前进 W)
    static const QPointF pts_up[] = {
        QPointF(50.0, 32.5), QPointF(51.4, 33.8), QPointF(79.3, 33.8), QPointF(80.6, 31.9),
        QPointF(75.4, 24.1), QPointF(74.1, 20.8), QPointF(72.8, 20.1), QPointF(72.8, 18.9),
        QPointF(67.0, 10.4), QPointF(65.7, 9.8),  QPointF(63.0, 10.4), QPointF(59.8, 16.2),
        QPointF(57.9, 18.2), QPointF(57.2, 20.1), QPointF(50.7, 29.2)
    };
    m_upPath.clear();
    QPolygonF poly_up;
    for (const auto &p : pts_up) poly_up << p;
    m_upPath.addPolygon(poly_up);
    m_upPath.closeSubpath();

    // 2. 下倒三角 (后退 S)
    static const QPointF pts_down[] = {
        QPointF(49.4, 98.8), QPointF(57.2, 110.5), QPointF(58.5, 113.8), QPointF(63.7, 120.9),
        QPointF(66.3, 120.9), QPointF(74.1, 109.8), QPointF(75.4, 106.6), QPointF(76.7, 106.0),
        QPointF(80.6, 98.8),  QPointF(80.0, 97.5),  QPointF(50.7, 97.5)
    };
    m_downPath.clear();
    QPolygonF poly_down;
    for (const auto &p : pts_down) poly_down << p;
    m_downPath.addPolygon(poly_down);
    m_downPath.closeSubpath();

    // 3. 中心圆角方块 (刹车/急停 Space)
    static const QPointF pts_center[] = {
        QPointF(53.3, 54.6), QPointF(53.3, 71.5), QPointF(52.6, 72.2), QPointF(53.3, 72.8),
        QPointF(53.3, 75.4), QPointF(55.9, 76.7), QPointF(73.5, 76.7), QPointF(74.8, 76.0),
        QPointF(76.7, 72.8), QPointF(76.7, 56.5), QPointF(75.4, 55.2), QPointF(75.4, 54.0),
        QPointF(74.1, 54.0), QPointF(73.5, 53.3), QPointF(55.2, 53.3)
    };
    m_centerPath.clear();
    QPolygonF poly_center;
    for (const auto &p : pts_center) poly_center << p;
    m_centerPath.addPolygon(poly_center);
    m_centerPath.closeSubpath();

    // 4. 左转弧弯箭头 (左转 A)
    static const QPointF pts_left[] = {
        QPointF(5.2, 65.0),  QPointF(5.2, 65.7),  QPointF(11.7, 72.2), QPointF(12.3, 72.2),
        QPointF(18.2, 77.3), QPointF(19.5, 76.7), QPointF(19.5, 71.5), QPointF(20.8, 70.2),
        QPointF(23.4, 70.2), QPointF(25.4, 71.5), QPointF(29.2, 71.5), QPointF(30.6, 72.8),
        QPointF(31.9, 72.8), QPointF(34.5, 74.8), QPointF(36.4, 77.3), QPointF(37.7, 77.3),
        QPointF(38.4, 76.7), QPointF(38.4, 74.8), QPointF(37.0, 72.8), QPointF(37.0, 70.8),
        QPointF(34.5, 66.3), QPointF(29.9, 63.0), QPointF(28.6, 63.0), QPointF(27.3, 61.8),
        QPointF(24.7, 61.8), QPointF(22.8, 60.5), QPointF(20.1, 60.5), QPointF(19.5, 59.8),
        QPointF(19.5, 54.0), QPointF(18.9, 53.3), QPointF(11.1, 59.1), QPointF(11.1, 59.8)
    };
    m_leftPath.clear();
    QPolygonF poly_left;
    for (const auto &p : pts_left) poly_left << p;
    m_leftPath.addPolygon(poly_left);
    m_leftPath.closeSubpath();

    // 5. 右转弧弯箭头 (右转 D)
    static const QPointF pts_right[] = {
        QPointF(125.5, 65.0), QPointF(124.2, 64.3), QPointF(117.7, 57.9), QPointF(111.2, 53.3),
        QPointF(110.5, 54.0), QPointF(110.5, 59.8), QPointF(109.8, 60.5), QPointF(107.2, 60.5),
        QPointF(105.3, 61.8), QPointF(102.7, 61.8), QPointF(101.4, 63.0), QPointF(100.1, 63.0),
        QPointF(98.2, 64.3),  QPointF(95.5, 67.0),  QPointF(93.0, 70.8),  QPointF(93.0, 77.3),
        QPointF(93.6, 77.3),  QPointF(94.9, 75.4),  QPointF(97.5, 73.5),  QPointF(101.4, 71.5),
        QPointF(104.7, 71.5), QPointF(106.6, 70.2), QPointF(109.8, 70.2), QPointF(110.5, 70.8),
        QPointF(110.5, 76.7), QPointF(111.8, 77.3), QPointF(117.0, 72.8), QPointF(117.7, 72.8),
        QPointF(122.8, 67.6), QPointF(123.5, 67.6)
    };
    m_rightPath.clear();
    QPolygonF poly_right;
    for (const auto &p : pts_right) poly_right << p;
    m_rightPath.addPolygon(poly_right);
    m_rightPath.closeSubpath();

    // 6. 设置齿轮 (右下角 ⚙ 线框轮廓)
    m_settingsPath.clear();
    const double gx = 109.5;
    const double gy = 109.2;
    QPolygonF gearPoly;
    const int numSteps = 72;
    for (int i = 0; i < numSteps; ++i) {
        double angle = i * 2.0 * M_PI / numSteps;
        double r = 11.7 + 1.8 * std::cos(6.0 * angle);
        gearPoly << QPointF(gx + r * std::cos(angle), gy + r * std::sin(angle));
    }
    m_settingsPath.addPolygon(gearPoly);
    m_settingsPath.closeSubpath();

    // 齿轮中心内圆
    m_settingsPath.addEllipse(QPointF(gx, gy), 3.8, 3.8);
}

DirectionPadWidget::PadKey DirectionPadWidget::hitTest(const QPoint &pos) const
{
    // 齿轮判定优先检测圆形区域
    const double gx = 109.5;
    const double gy = 109.2;
    if (std::hypot(pos.x() - gx, pos.y() - gy) <= 15.0) {
        return PadKey::Settings;
    }

    if (m_upPath.contains(pos))       return PadKey::Up;
    if (m_downPath.contains(pos))     return PadKey::Down;
    if (m_leftPath.contains(pos))     return PadKey::Left;
    if (m_rightPath.contains(pos))    return PadKey::Right;
    if (m_centerPath.contains(pos))   return PadKey::Center;

    return PadKey::None;
}

void DirectionPadWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

    // 颜色配置 (精准采样自参考原图与按住 W 对比图):
    const QColor cNormal(28, 114, 159);   // #1c729f 科技蓝 (未按下)
    const QColor cHover(36, 138, 194);    // #248ac2 悬停亮蓝
    const QColor cPressed(157, 36, 20);   // #9d2414 暗红 (按键激活按下变红)

    auto getColor = [&](PadKey key) -> QColor {
        if (m_pressedKeys.contains(key)) return cPressed;
        if (m_hoveredKey == key) return cHover;
        return cNormal;
    };

    // 绘制 5 个填充部件 (上、下、左、右、中)
    auto drawFilled = [&](const QPainterPath &path, PadKey key) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(getColor(key));
        painter.drawPath(path);
    };

    drawFilled(m_upPath,     PadKey::Up);
    drawFilled(m_downPath,   PadKey::Down);
    drawFilled(m_leftPath,   PadKey::Left);
    drawFilled(m_rightPath,  PadKey::Right);
    drawFilled(m_centerPath, PadKey::Center);

    // 绘制右下角线框齿轮 (空心波浪齿轮 + 中心圆圈，对标原图)
    painter.setBrush(Qt::NoBrush);
    QPen gearPen(getColor(PadKey::Settings), 2.0);
    gearPen.setCapStyle(Qt::RoundCap);
    gearPen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(gearPen);
    painter.drawPath(m_settingsPath);
}

void DirectionPadWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        PadKey key = hitTest(event->pos());
        m_mouseActiveKey = key;

        if (key != PadKey::None) {
            m_pressedKeys.insert(key);
            update();

            switch (key) {
            case PadKey::Up:
                TeleopController::instance().setVirtualKey(Qt::Key_W, true);
                break;
            case PadKey::Down:
                TeleopController::instance().setVirtualKey(Qt::Key_S, true);
                break;
            case PadKey::Left:
                TeleopController::instance().setVirtualKey(Qt::Key_A, true);
                break;
            case PadKey::Right:
                TeleopController::instance().setVirtualKey(Qt::Key_D, true);
                break;
            case PadKey::Center:
                TeleopController::instance().setVirtualKey(Qt::Key_Space, true);
                break;
            case PadKey::Settings:
                break;
            default:
                break;
            }
        }
    }
    QWidget::mousePressEvent(event);
}

void DirectionPadWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_mouseActiveKey != PadKey::None) {
            PadKey prevKey = m_mouseActiveKey;
            m_mouseActiveKey = PadKey::None;

            // 恢复虚拟按键状态 (若对应物理按键未按下)
            switch (prevKey) {
            case PadKey::Up:
                if (!TeleopController::instance().isKeyPressed(Qt::Key_W)) {
                    TeleopController::instance().setVirtualKey(Qt::Key_W, false);
                }
                break;
            case PadKey::Down:
                if (!TeleopController::instance().isKeyPressed(Qt::Key_S)) {
                    TeleopController::instance().setVirtualKey(Qt::Key_S, false);
                }
                break;
            case PadKey::Left:
                if (!TeleopController::instance().isKeyPressed(Qt::Key_A)) {
                    TeleopController::instance().setVirtualKey(Qt::Key_A, false);
                }
                break;
            case PadKey::Right:
                if (!TeleopController::instance().isKeyPressed(Qt::Key_D)) {
                    TeleopController::instance().setVirtualKey(Qt::Key_D, false);
                }
                break;
            case PadKey::Center:
                if (!TeleopController::instance().isKeyPressed(Qt::Key_Space)) {
                    TeleopController::instance().setVirtualKey(Qt::Key_Space, false);
                }
                break;
            case PadKey::Settings:
                if (hitTest(event->pos()) == PadKey::Settings) {
                    m_pressedKeys.remove(PadKey::Settings);
                    update();
                    ManualControlConfigDialog dlg(this);
                    dlg.exec();
                }
                break;
            default:
                break;
            }

            if (prevKey != PadKey::Settings) {
                m_pressedKeys.remove(prevKey);
                update();
            }
        }
    }
    QWidget::mouseReleaseEvent(event);
}

void DirectionPadWidget::mouseMoveEvent(QMouseEvent *event)
{
    PadKey hovered = hitTest(event->pos());
    if (hovered != m_hoveredKey) {
        m_hoveredKey = hovered;
        setCursor((hovered != PadKey::None) ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void DirectionPadWidget::leaveEvent(QEvent *event)
{
    if (m_hoveredKey != PadKey::None) {
        m_hoveredKey = PadKey::None;
        setCursor(Qt::ArrowCursor);
        update();
    }

    // 鼠标移出边界时安全释放按住的虚拟键，防止失控移动
    if (m_mouseActiveKey != PadKey::None && m_mouseActiveKey != PadKey::Settings) {
        switch (m_mouseActiveKey) {
        case PadKey::Up:
            TeleopController::instance().setVirtualKey(Qt::Key_W, false);
            break;
        case PadKey::Down:
            TeleopController::instance().setVirtualKey(Qt::Key_S, false);
            break;
        case PadKey::Left:
            TeleopController::instance().setVirtualKey(Qt::Key_A, false);
            break;
        case PadKey::Right:
            TeleopController::instance().setVirtualKey(Qt::Key_D, false);
            break;
        case PadKey::Center:
            TeleopController::instance().setVirtualKey(Qt::Key_Space, false);
            break;
        default:
            break;
        }
        m_pressedKeys.remove(m_mouseActiveKey);
        m_mouseActiveKey = PadKey::None;
        update();
    }

    QWidget::leaveEvent(event);
}

void DirectionPadWidget::onTeleopKeyStateChanged(int key, bool pressed)
{
    PadKey target = PadKey::None;
    switch (key) {
    case Qt::Key_W:     target = PadKey::Up; break;
    case Qt::Key_S:     target = PadKey::Down; break;
    case Qt::Key_A:     target = PadKey::Left; break;
    case Qt::Key_D:     target = PadKey::Right; break;
    case Qt::Key_Space: target = PadKey::Center; break;
    default: return;
    }

    if (pressed) {
        m_pressedKeys.insert(target);
    } else {
        // 仅当鼠标也没在按该键时才移除
        if (m_mouseActiveKey != target) {
            m_pressedKeys.remove(target);
        }
    }
    update();
}
