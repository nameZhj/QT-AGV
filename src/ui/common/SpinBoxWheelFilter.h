/**
 * @file    SpinBoxWheelFilter.h
 * @module  GUILayer
 * @brief   Event filter blocking accidental mouse wheel modifications on unfocused spin boxes.
 * @deps    QObject, QEvent, QAbstractSpinBox
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QObject>

/**
 * @brief 全局步进器滚轮防误触事件过滤器
 * 
 * 规则：
 * 默认情况下 Qt 的 QAbstractSpinBox 具有 Qt::WheelFocus，鼠标悬停滚动即可触发数值变动并截断滚动。
 * 本过滤器将所有步进器的焦点策略规范为 Qt::StrongFocus，且仅在步进器当前拥有输入焦点
 * （即用户已明确通过鼠标点击或键盘聚焦）时才允许响应滚轮步进；
 * 未获得焦点时忽略滚轮事件并阻止其触发步进，使滚轮事件向上自然冒泡至父级滚动容器（如 QScrollArea）。
 */
class SpinBoxWheelFilter final : public QObject
{
    Q_OBJECT
public:
    // === [Section: Constructors & Destructor] ===
    explicit SpinBoxWheelFilter(QObject *parent = nullptr);
    ~SpinBoxWheelFilter() override = default;

protected:
    // === [Section: Event Filtering] ===
    bool eventFilter(QObject *watched, QEvent *event) override;
};

