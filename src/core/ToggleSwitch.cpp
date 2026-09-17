#include "ToggleSwitch.h"

#include <QPainter>
#include <QPropertyAnimation>
#include <QEasingCurve>

ToggleSwitch::ToggleSwitch(QWidget *parent)
    : QAbstractButton(parent)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_anim = new QPropertyAnimation(this, "offset", this);
    m_anim->setDuration(kAnimDurationMs);
    m_anim->setEasingCurve(QEasingCurve::InOutCubic);
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
    if (qFuzzyCompare(m_offset, value)) {
        return;
    }
    m_offset = qBound(0.0, value, 1.0);
    update();
}

QColor ToggleSwitch::onColor() const
{
    return m_onColor;
}

void ToggleSwitch::setOnColor(const QColor &color)
{
    if (m_onColor != color) {
        m_onColor = color;
        update();
    }
}

QColor ToggleSwitch::offColor() const
{
    return m_offColor;
}

void ToggleSwitch::setOffColor(const QColor &color)
{
    if (m_offColor != color) {
        m_offColor = color;
        update();
    }
}

QColor ToggleSwitch::knobColor() const
{
    return m_knobColor;
}

void ToggleSwitch::setKnobColor(const QColor &color)
{
    if (m_knobColor != color) {
        m_knobColor = color;
        update();
    }
}

void ToggleSwitch::checkStateSet()
{
    QAbstractButton::checkStateSet();
    animateToChecked(isChecked());
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

    if (m_anim) {
        m_anim->stop();
        m_anim->setStartValue(m_offset);
        m_anim->setEndValue(target);
        m_anim->start();
    } else {
        setOffset(target);
    }
}

void ToggleSwitch::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // 1. 计算外层跑道尺寸
    const QRectF track = rect().adjusted(1, 1, -1, -1);
    const qreal radius = track.height() / 2.0;

    // 2. 颜色线性插值计算 (Off -> On)
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

    // 3. 绘制跑道背景
    p.setPen(Qt::NoPen);
    p.setBrush(currentTrackColor);
    p.drawRoundedRect(track, radius, radius);

    // 4. 计算滑块尺寸与位移
    const qreal knobD = track.height() - (kKnobMargin * 2.0);
    const qreal minX = track.left() + kKnobMargin;
    const qreal maxX = track.right() - kKnobMargin - knobD;
    const qreal knobX = minX + (maxX - minX) * m_offset;
    const qreal knobY = track.top() + kKnobMargin;

    // 5. 绘制滑块 (微弱柔和阴影与本体)
    const QRectF knobRect(knobX, knobY, knobD, knobD);

    // 启用状态下给滑块添加微弱边框增加层级质感
    if (isEnabled()) {
        p.setPen(QColor(0, 0, 0, 20));
        p.setBrush(m_knobColor);
    } else {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(240, 240, 240, 180));
    }
    p.drawEllipse(knobRect);
}
