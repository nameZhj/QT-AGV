#pragma once

#include <QAbstractButton>
#include <QColor>

class QPropertyAnimation;
class QMouseEvent;

/**
 * @brief 现代胶囊风格平滑滑动开关控件 (Toggle Switch)
 * 
 * 架构规范与特性：
 * 1. 继承自 QAbstractButton，具备标准 checkable 语义，API 与 QPushButton/QCheckBox 100% 兼容；
 * 2. 引入 QPropertyAnimation 实现滑块位移与轨道背景色的平滑双重渐变缓动 (InOutCubic)；
 * 3. 支持自定义开启颜色 (onColor)、关闭颜色 (offColor)、滑块颜色 (knobColor)；
 * 4. 完备的禁用态 (Disabled) 与抗锯齿 (Antialiasing) 渲染支持；
 * 5. 消除硬编码魔数，规范尺寸常量。
 */
class ToggleSwitch final : public QAbstractButton
{
    Q_OBJECT
    Q_PROPERTY(qreal offset READ offset WRITE setOffset)

public:
    explicit ToggleSwitch(QWidget *parent = nullptr);
    ~ToggleSwitch() override = default;

    // ── 默认样式与动画常量 ──
    static constexpr int    kDefaultWidth       = 40;   ///< 默认推荐宽度
    static constexpr int    kDefaultHeight      = 22;   ///< 默认推荐高度
    static constexpr int    kAnimDurationMs     = 180;  ///< 滑动动画时长 (ms)
    static constexpr double kKnobMargin         = 2.0;  ///< 滑块与轨道边缘内衬 (px)

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    qreal offset() const;
    void setOffset(qreal value);

    QColor onColor() const;
    void setOnColor(const QColor &color);

    QColor offColor() const;
    void setOffColor(const QColor &color);

    QColor knobColor() const;
    void setKnobColor(const QColor &color);

protected:
    void paintEvent(QPaintEvent *event) override;
    void checkStateSet() override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void animateToChecked(bool checked);

    qreal m_offset = 0.0; // 0.0: Off, 1.0: On
    QPropertyAnimation *m_anim = nullptr;

    QColor m_onColor  {0, 188, 212}; // 现代青蓝 #00bcd4
    QColor m_offColor {71, 85, 105}; // 现代暗蓝灰 #475569
    QColor m_knobColor{Qt::white};
};

