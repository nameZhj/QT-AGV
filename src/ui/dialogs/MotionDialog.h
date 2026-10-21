/**
 * @file    MotionDialog.h
 * @layer   GUILayer
 * @brief   Quick modal dialog for editing AGV motion limits, speeds, and acceleration profiles.
 * @deps    QDialog, QPoint, QTimer
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QDialog>
#include <QPoint>
#include <QTimer>

class QTabWidget;
class QDoubleSpinBox;
class QRadioButton;
class QLabel;
class QPushButton;
struct RobotInfo;

class MotionDialogTabs;
class MotionDialogController;

/**
 * @brief 平动转动控制弹窗 (对标参考界面)
 * 包含平动参数、转动参数、状态跟踪、看门狗以及取消/暂停/继续导航控制。
 */
class MotionDialog : public QDialog
{
    Q_OBJECT
    friend class MotionDialogTabs;
    friend class MotionDialogController;

public:
    explicit MotionDialog(QWidget *parent = nullptr);
    ~MotionDialog() override;

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void showEvent(QShowEvent *event) override;

public slots:
    void refreshSpeedLimits();

signals:
    void motionStarted(int motionType, double param, const QString &desc);
    void motionCompleted();
    void motionCanceled();
    void motionPaused(bool paused);
    void statusHintRequested(const QString &msg, const QString &color = "#00e676");

private slots:
    void onSendClicked();
    void onCancelNavClicked();
    void onPauseNavClicked();
    void onCurrentRobotInfoUpdated(const RobotInfo &info);
    void onMotionTimeout();

private:
    void setupUi();
    void setRunningState(bool running, bool paused = false);
    void setCompletedState();
    void setCanceledState();

private:
    // 窗口拖拽
    bool   m_dragging = false;
    QPoint m_dragPosition;

    // 选项卡
    QTabWidget *m_tabWidget = nullptr;

    // 平动控件
    QLabel         *m_transDistLbl  = nullptr;
    QDoubleSpinBox *m_transDistSpin = nullptr;
    QLabel         *m_transVxLbl    = nullptr;
    QDoubleSpinBox *m_transVxSpin   = nullptr;
    QLabel         *m_transVyLbl    = nullptr;
    QDoubleSpinBox *m_transVySpin   = nullptr;
    QRadioButton   *m_odomModeRadio = nullptr;
    QRadioButton   *m_locModeRadio  = nullptr;

    // 转动控件
    QLabel         *m_rotAngleLbl  = nullptr;
    QDoubleSpinBox *m_rotAngleSpin = nullptr;
    QLabel         *m_rotSpeedLbl  = nullptr;
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
