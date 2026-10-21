/**
 * @file    ButtonClickEffect.cpp
 * @module  GUILayer
 * @brief   Implementation of button click visual animation event filter.
 * @deps    ButtonClickEffect.h, QPushButton, QPropertyAnimation
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "ButtonClickEffect.h"
#include "ToggleSwitch.h"

#include <QAbstractButton>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>
#include <QMouseEvent>

namespace {
constexpr const char *kAnimGroupObjectName = "__btn_click_anim_group__";
}

// === [Section: Constructors & Setup] ===
ButtonClickEffect::ButtonClickEffect(QObject *parent)
    : QObject(parent)
{
}

// === [Section: Control & Status] ===
bool ButtonClickEffect::isEnabled() const
{
    return m_enabled;
}

void ButtonClickEffect::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

// === [Section: Private Animation Helpers] ===
void ButtonClickEffect::triggerClickAnimation(QAbstractButton *btn)
{
    if (!btn) return;

    // 显式免除动画的按钮
    if (btn->property("noClickEffect").toBool()) {
        return;
    }

    // 检查祖先控件：若父控件已有 QGraphicsEffect (如卡片投影)，严禁挂载 OpacityEffect，防止 Qt 离屏渲染双重合成导致按钮不可见
    QWidget *ancestor = btn->parentWidget();
    while (ancestor) {
        if (ancestor->graphicsEffect() != nullptr) {
            return;
        }
        ancestor = ancestor->parentWidget();
    }

    // 确保不破坏按钮已有的非透明度特效（如自定义阴影 QGraphicsDropShadowEffect）
    auto *effect = qobject_cast<QGraphicsOpacityEffect*>(btn->graphicsEffect());
    if (!effect) {
        if (btn->graphicsEffect() != nullptr) {
            return;
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

// === [Section: Event Filtering] ===
bool ButtonClickEffect::eventFilter(QObject *obj, QEvent *event)
{
    if (!m_enabled || event->type() != QEvent::MouseButtonPress) {
        return QObject::eventFilter(obj, event);
    }

    // ToggleSwitch 拥有自定义滑块与跑道动画，严禁挂载 OpacityEffect 以免干扰自定义绘制与位移动效
    if (qobject_cast<ToggleSwitch*>(obj)) {
        return QObject::eventFilter(obj, event);
    }

    auto *btn = qobject_cast<QAbstractButton*>(obj);
    auto *mouseEvent = static_cast<QMouseEvent*>(event);
    if (!btn || !btn->isEnabled() || mouseEvent->button() != Qt::LeftButton) {
        return QObject::eventFilter(obj, event);
    }

    triggerClickAnimation(btn);
    return QObject::eventFilter(obj, event);
}

