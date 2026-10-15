#include "ButtonClickEffect.h"

#include <QAbstractButton>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>
#include <QMouseEvent>

namespace {
constexpr const char *kAnimGroupObjectName = "__btn_click_anim_group__";
}

ButtonClickEffect::ButtonClickEffect(QObject *parent)
    : QObject(parent)
{
}

bool ButtonClickEffect::isEnabled() const
{
    return m_enabled;
}

void ButtonClickEffect::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

bool ButtonClickEffect::eventFilter(QObject *obj, QEvent *event)
{
    if (m_enabled && event->type() == QEvent::MouseButtonPress) {
        auto *btn = qobject_cast<QAbstractButton*>(obj);
        auto *me  = static_cast<QMouseEvent*>(event);
        if (btn && btn->isEnabled() && me->button() == Qt::LeftButton) {
            // 确保不破坏按钮已有的非透明度特效（如自定义阴影 QGraphicsDropShadowEffect）
            auto *effect = qobject_cast<QGraphicsOpacityEffect*>(btn->graphicsEffect());
            if (!effect) {
                if (btn->graphicsEffect() != nullptr) {
                    return QObject::eventFilter(obj, event);
                }
                effect = new QGraphicsOpacityEffect(btn);
                btn->setGraphicsEffect(effect);
            }

            // 防重入：若前一次点击触发的动画尚未结束，先行终止并重置为正常透明度
            auto *oldGroup = btn->findChild<QSequentialAnimationGroup*>(kAnimGroupObjectName, Qt::FindDirectChildrenOnly);
            if (oldGroup) {
                oldGroup->stop();
                delete oldGroup;
            }
            effect->setOpacity(kNormalOpacity);

            // 使用 QSequentialAnimationGroup 保证按下淡化与弹起恢复的严格时序
            auto *group = new QSequentialAnimationGroup(btn);
            group->setObjectName(kAnimGroupObjectName);

            auto *down = new QPropertyAnimation(effect, "opacity", group);
            down->setDuration(kFadeOutDurationMs);
            down->setStartValue(kNormalOpacity);
            down->setEndValue(kPressedOpacity);

            auto *up = new QPropertyAnimation(effect, "opacity", group);
            up->setDuration(kFadeInDurationMs);
            up->setStartValue(kPressedOpacity);
            up->setEndValue(kNormalOpacity);

            group->addAnimation(down);
            group->addAnimation(up);

            // 动画播放完成后自动由 Qt 内存管理释放
            group->start(QAbstractAnimation::DeleteWhenStopped);
        }
    }
    return QObject::eventFilter(obj, event);
}
