#pragma once

#include <QDialog>
#include <QPoint>
#include <QTimer>

class QTabWidget;
class QDoubleSpinBox;
class QRadioButton;
class QLabel;
class QPushButton;

/**
 * @brief 平动转动控制弹窗 (对标参考界面)
 * 
 * 功能:
 * 1. 平动 Tab: 设置移动距离 (m)、vx 速度 (m/s)、vy 速度 (m/s)、模式 (里程模式 / 定位模式)
 * 2. 转动 Tab: 设置旋转角度 (°)、角速度 (°/s)
 * 3. 状态管理: 初始未运行 -> 运行中... -> 导航已完成 (或 取消导航 / 已暂停)
 * 4. 底部按钮: [取消导航] [暂无 / 暂停导航 / 继续导航] [发送]
 */
class MotionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MotionDialog(QWidget *parent = nullptr);
    ~MotionDialog() override;

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    void onSendClicked();
    void onCancelNavClicked();
    void onPauseNavClicked();
    void onCurrentRobotInfoUpdated(const struct RobotInfo &info);
    void onMotionTimeout();

private:
    void setupUi();
    QWidget* createTranslationTab();
    QWidget* createRotationTab();
    void setRunningState(bool running, bool paused = false);
    void setCompletedState();
    void setCanceledState();
    QString currentCarId() const;

    // 窗口拖拽
    bool   m_dragging = false;
    QPoint m_dragPosition;

    // 选项卡
    QTabWidget *m_tabWidget = nullptr;

    // 平动控件
    QDoubleSpinBox *m_transDistSpin = nullptr;
    QDoubleSpinBox *m_transVxSpin   = nullptr;
    QDoubleSpinBox *m_transVySpin   = nullptr;
    QRadioButton   *m_odomModeRadio = nullptr;
    QRadioButton   *m_locModeRadio  = nullptr;

    // 转动控件
    QDoubleSpinBox *m_rotAngleSpin = nullptr;
    QDoubleSpinBox *m_rotSpeedSpin = nullptr;

    // 底部控制与状态
    QLabel      *m_statusLabel = nullptr;
    QPushButton *m_cancelBtn   = nullptr;
    QPushButton *m_pauseBtn    = nullptr;
    QPushButton *m_sendBtn     = nullptr;

    // 运行追踪状态
    bool   m_isRunning = false;
    bool   m_isPaused  = false;
    int    m_motionType = 0; // 1: 平动, 2: 转动
    double m_targetDistance = 0.0;
    double m_targetAngle = 0.0;
    double m_startX = 0.0;
    double m_startY = 0.0;
    double m_startAngle = 0.0;
    bool   m_hasStartPose = false;

    QTimer m_motionWatchdog;
};
