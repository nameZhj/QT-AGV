/**
 * @file    ToggleSwitch.cpp
 * @module  GUILayer
 * @brief   Implementation of modern iOS-style animated toggle switch widget.
 * @deps    ToggleSwitch.h, QPainter, QPropertyAnimation
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "ToggleSwitch.h"

#include <QPainter>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QMouseEvent>
#include <cmath>

ToggleSwitch::ToggleSwitch(QWidget *parent)
    : QAbstractButton(parent)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFixedSize(kDefaultWidth, kDefaultHeight);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setStyleSheet("background:transparent;border:none;");

    m_anim = new QPropertyAnimation(this, "offset", this);
    m_anim->setDuration(120);
    m_anim->setEasingCurve(QEasingCurve::InOutCubic);
    connect(m_anim, &QPropertyAnimation::finished, this, [this]() {
        setOffset(isChecked() ? 1.0 : 0.0);
    });
}

QSize ToggleSwitch::sizeHint() const
{
    return QSize(kDefaultWidth, kDefaultHeight);
}

QSize ToggleSwitch::minimumSizeHint() const
{
    return QSize(kDefaultWidth, kDefaultHeight);
}

qreal ToggleSwitch::offset() const
{
    return m_offset;
}

void ToggleSwitch::setOffset(qreal value)
{
    if (std::abs(m_offset - value) < 0.001) {
        return;
    }
    m_offset = qBound(0.0, value, 1.0);
    update();
}

QColor ToggleSwitch::onColor() const
{
    return m_onColor;
}

// === [Section: Properties & Accessors] ===
void ToggleSwitch::setOnColor(const QColor &color)
{
    if (m_onColor == color) return;
    m_onColor = color;
    update();
}

QColor ToggleSwitch::offColor() const
{
    return m_offColor;
}

void ToggleSwitch::setOffColor(const QColor &color)
{
    if (m_offColor == color) return;
    m_offColor = color;
    update();
}

QColor ToggleSwitch::knobColor() const
{
    return m_knobColor;
}

void ToggleSwitch::setKnobColor(const QColor &color)
{
    if (m_knobColor == color) return;
    m_knobColor = color;
    update();
}

// === [Section: State & Animation Handlers] ===
void ToggleSwitch::checkStateSet()
{
    QAbstractButton::checkStateSet();
    animateToChecked(isChecked());
}

void ToggleSwitch::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        event->accept();
        return;
    }
    QAbstractButton::mousePressEvent(event);
}

void ToggleSwitch::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (rect().contains(event->pos())) {
            toggle();
            event->accept();
            return;
        }
    }
    QAbstractButton::mouseReleaseEvent(event);
}

void ToggleSwitch::animateToChecked(bool checked)
{
    const qreal target = checked ? 1.0 : 0.0;
    if (!isVisible()) {
        if (m_anim && m_anim->state() == QAbstractAnimation::Running) {
            m_anim->stop();
        }
        setOffset(target);
        return;
    }

    if (!m_anim) {
        setOffset(target);
        return;
    }

    m_anim->stop();
    m_anim->setStartValue(m_offset);
    m_anim->setEndValue(target);
    m_anim->start();
}

// === [Section: Rendering & Drawing Helpers] ===
void ToggleSwitch::paintTrack(QPainter &p, const QRectF &track, qreal radius) const
{
    auto interpolateColor = [](const QColor &c1, const QColor &c2, qreal factor) -> QColor {
        const int r = static_cast<int>(c1.red()   + (c2.red()   - c1.red())   * factor);
        const int g = static_cast<int>(c1.green() + (c2.green() - c1.green()) * factor);
        const int b = static_cast<int>(c1.blue()  + (c2.blue()  - c1.blue())  * factor);
        const int a = static_cast<int>(c1.alpha() + (c2.alpha() - c1.alpha()) * factor);
        return QColor(r, g, b, a);
    };

    QColor currentTrackColor = interpolateColor(m_offColor, m_onColor, m_offset);
    if (!isEnabled()) {
        currentTrackColor.setAlpha(120);
    }

    p.setPen(Qt::NoPen);
    p.setBrush(currentTrackColor);
    p.drawRoundedRect(track, radius, radius);
}

void ToggleSwitch::paintKnob(QPainter &p, const QRectF &track, qreal knobD) const
{
    const qreal minX = track.left() + kKnobMargin;
    const qreal maxX = track.right() - kKnobMargin - knobD;
    const qreal knobX = minX + (maxX - minX) * m_offset;
    const qreal knobY = track.top() + kKnobMargin;
    const QRectF knobRect(knobX, knobY, knobD, knobD);

    if (isEnabled()) {
        p.setPen(QColor(0, 0, 0, 20));
        p.setBrush(m_knobColor);
    } else {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(240, 240, 240, 180));
    }
    p.drawEllipse(knobRect);
}

void ToggleSwitch::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF track = rect().adjusted(1, 1, -1, -1);
    const qreal radius = track.height() / 2.0;
    const qreal knobD = track.height() - (kKnobMargin * 2.0);

    paintTrack(p, track, radius);
    paintKnob(p, track, knobD);
}

