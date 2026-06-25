#include "ButtonClickEffect.h"
#include <QAbstractButton>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QMouseEvent>

ButtonClickEffect::ButtonClickEffect(QObject *parent) : QObject(parent) {}

bool ButtonClickEffect::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        auto *btn = qobject_cast<QAbstractButton*>(obj);
        auto *me  = static_cast<QMouseEvent*>(event);
        if (btn && btn->isEnabled() && me->button() == Qt::LeftButton) {
            auto *effect = qobject_cast<QGraphicsOpacityEffect*>(btn->graphicsEffect());
            if (!effect) {
                effect = new QGraphicsOpacityEffect(btn);
                btn->setGraphicsEffect(effect);
            }
            effect->setOpacity(1.0);

            auto *down = new QPropertyAnimation(effect, "opacity", btn);
            down->setDuration(70);
            down->setStartValue(1.0);
            down->setEndValue(0.55);

            auto *up = new QPropertyAnimation(effect, "opacity", btn);
            up->setDuration(140);
            up->setStartValue(0.55);
            up->setEndValue(1.0);

            connect(down, &QPropertyAnimation::finished, up, [up]() {
                up->start(QAbstractAnimation::DeleteWhenStopped);
            });
            down->start(QAbstractAnimation::DeleteWhenStopped);
        }
    }
    return QObject::eventFilter(obj, event);
}
