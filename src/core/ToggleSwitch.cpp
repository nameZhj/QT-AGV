#include "ToggleSwitch.h"
#include <QPainter>

ToggleSwitch::ToggleSwitch(QWidget *parent) : QAbstractButton(parent)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
}

QSize ToggleSwitch::sizeHint() const
{
    return QSize(40, 22);
}

void ToggleSwitch::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRectF track = rect().adjusted(1, 1, -1, -1);
    qreal radius = track.height() / 2.0;

    const QColor onColor(41, 128, 185);
    const QColor offColor(96, 104, 116);
    p.setPen(Qt::NoPen);
    p.setBrush(isEnabled() ? (isChecked() ? onColor : offColor) : offColor.lighter(115));
    p.drawRoundedRect(track, radius, radius);

    qreal knobD = track.height() - 4;
    qreal knobX = isChecked() ? (track.right() - knobD - 2) : (track.left() + 2);
    p.setBrush(Qt::white);
    p.drawEllipse(QRectF(knobX, track.top() + 2, knobD, knobD));
}
