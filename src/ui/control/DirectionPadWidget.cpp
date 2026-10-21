/**
 * @file    DirectionPadWidget.cpp
 * @module  GUILayer
 * @brief   Implementation of virtual D-pad omnidirectional motion control pad.
 * @deps    DirectionPadWidget.h, TeleopController.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "DirectionPadWidget.h"
#include "DirectionPadGeometry.h"
#include "ManualControlConfigDialog.h"
#include "core/TeleopController.h"
#include "ui/common/Toast.h"
#include "ui/common/ThemeColors.h"

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

void DirectionPadWidget::updatePaths()
{
    DirectionPadGeometry::buildPaths(m_upPath, m_downPath, m_leftPath, m_rightPath, m_centerPath, m_settingsPath);
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

    // 颜色配置 (精准采样自参考原图与按住 W 对比图，通过 ThemeColors 集中管理):
    const QColor cNormal = ThemeColors::DirectionPad::Normal();
    const QColor cHover = ThemeColors::DirectionPad::Hover();
    const QColor cPressed = ThemeColors::DirectionPad::Pressed();

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

int DirectionPadWidget::padKeyToQtKey(PadKey key)
{
    switch (key) {
    case PadKey::Up:     return Qt::Key_W;
    case PadKey::Down:   return Qt::Key_S;
    case PadKey::Left:   return Qt::Key_A;
    case PadKey::Right:  return Qt::Key_D;
    case PadKey::Center: return Qt::Key_Space;
    default:             return 0;
    }
}

void DirectionPadWidget::dispatchPadKey(PadKey key, bool pressed)
{
    const int qtKey = padKeyToQtKey(key);
    if (qtKey != 0) {
        TeleopController::instance().setVirtualKey(qtKey, pressed);
    }
}

// === [Section: Event Handlers] ===
void DirectionPadWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    const PadKey key = hitTest(event->pos());

    // 中间方块为刹车(安全动作)，任何时候都可直接按下，不需要先抢占控制权；
    // 方向键属于驱动车辆，必须由用户显式点击"抢占控制权"后才会生效
    if (key != PadKey::Center && !TeleopController::instance().isActive()) {
        Toast::info(this, QStringLiteral("未抢占控制权，请先点击“抢占控制权”再遥控"));
        QWidget::mousePressEvent(event);
        return;
    }

    m_mouseActiveKey = key;

    if (key != PadKey::None) {
        m_pressedKeys.insert(key);
        update();
        dispatchPadKey(key, true);
    }

    QWidget::mousePressEvent(event);
}

void DirectionPadWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || m_mouseActiveKey == PadKey::None) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    const PadKey prevKey = m_mouseActiveKey;
    m_mouseActiveKey = PadKey::None;

    if (prevKey == PadKey::Settings) {
        if (hitTest(event->pos()) == PadKey::Settings) {
            m_pressedKeys.remove(PadKey::Settings);
            update();
            ManualControlConfigDialog dlg(this);
            dlg.exec();
        }
    } else {
        dispatchPadKey(prevKey, false);
        m_pressedKeys.remove(prevKey);
        update();
    }

    QWidget::mouseReleaseEvent(event);
}

void DirectionPadWidget::mouseMoveEvent(QMouseEvent *event)
{
    const PadKey hovered = hitTest(event->pos());
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
        dispatchPadKey(m_mouseActiveKey, false);
        m_pressedKeys.remove(m_mouseActiveKey);
        m_mouseActiveKey = PadKey::None;
        update();
    }

    QWidget::leaveEvent(event);
}

// === [Section: Teleoperation Sync Slots] ===
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

