#pragma once

#include <QWidget>
#include <QSet>
#include <QPainterPath>

/**
 * @brief 虚拟方向控制盘 (常驻于地图与建图界面左下角)
 * 
 * 视觉结构 (像素级契合参考图1):
 * - 上: 前进正三角 ▲ (W)
 * - 下: 后退倒三角 ▼ (S)
 * - 左: 弧形左转弯箭头 ↶ (A)
 * - 右: 弧形右转弯箭头 ↷ (D)
 * - 中: 急停/制动圆角方块 ■ (Space)
 * - 右下: 设置齿轮 ⚙ (弹出设置参数弹窗)
 * - 不包含 P 按钮
 * 
 * 交互支持:
 * - 鼠标长按点击移动，松开停止；
 * - 键盘按下 W/A/S/D/Space 对应按键实时高亮/按下变色；
 * - 纯矢量自绘，高帧率平滑渲染。
 */
class DirectionPadWidget : public QWidget
{
    Q_OBJECT

public:
    enum class PadKey {
        None,
        Up,       ///< 前进 (W)
        Down,     ///< 后退 (S)
        Left,     ///< 左转 (A)
        Right,    ///< 右转 (D)
        Center,   ///< 停止/刹车 (Space)
        Settings  ///< 设置 (齿轮)
    };

    explicit DirectionPadWidget(QWidget *parent = nullptr);
    ~DirectionPadWidget() override = default;

    QSize sizeHint() const override { return QSize(130, 130); }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void onTeleopKeyStateChanged(int key, bool pressed);

private:
    PadKey hitTest(const QPoint &pos) const;
    void updatePaths();

    // 几何路径定义
    QPainterPath m_upPath;
    QPainterPath m_downPath;
    QPainterPath m_leftPath;
    QPainterPath m_rightPath;
    QPainterPath m_centerPath;
    QPainterPath m_settingsPath;

    // 状态
    PadKey       m_hoveredKey = PadKey::None;
    QSet<PadKey> m_pressedKeys;
    PadKey       m_mouseActiveKey = PadKey::None;
};
