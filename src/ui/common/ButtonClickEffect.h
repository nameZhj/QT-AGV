/**
 * @file    ButtonClickEffect.h
 * @module  GUILayer
 * @brief   Event filter providing rebound visual click animations for push buttons.
 * @deps    QObject, QEvent, QPushButton
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QObject>

/**
 * @brief 全局按钮点击交互反馈动效事件过滤器
 * 
 * 架构规范与特性：
 * 1. 作为应用级全局事件过滤器（安装至 QApplication），自动为 QAbstractButton 派生类注入点击反馈；
 * 2. 采用 QSequentialAnimationGroup 顺序动画组，实现按下瞬间微调(0.55)与弹起恢复(1.0)的平滑过渡；
 * 3. 彻底防范重入与连击竞争：通过 Qt 对象树管理机制自动清理前置未完结动画，杜绝透明度卡死与内存泄漏；
 * 4. 尊重已有图形效果：若按钮已配置非透明度特效（如阴影），则安全回退不破坏既有渲染；
 * 5. 提供全局使能开关 (setEnabled / isEnabled)。
 */
class ButtonClickEffect final : public QObject
{
    Q_OBJECT

public:
    // === [Section: Constructors & Destructor] ===
    explicit ButtonClickEffect(QObject *parent = nullptr);
    ~ButtonClickEffect() override = default;

    // === [Section: Constants] ===
    static constexpr int    kFadeOutDurationMs = 70;   ///< 按下淡化时长 (ms)
    static constexpr int    kFadeInDurationMs  = 140;  ///< 弹起恢复时长 (ms)
    static constexpr double kNormalOpacity     = 1.0;   ///< 默认不透明度
    static constexpr double kPressedOpacity    = 0.55;  ///< 按下状态不透明度

    // === [Section: Control & Status] ===
    bool isEnabled() const;
    void setEnabled(bool enabled);

protected:
    // === [Section: Event Filtering] ===
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    // === [Section: Private Animation Helpers] ===
    void triggerClickAnimation(class QAbstractButton *btn);

    bool m_enabled = true;
};

