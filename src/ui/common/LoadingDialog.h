/**
 * @file    LoadingDialog.h
 * @module  GUILayer
 * @brief   Modal spinner loading dialog for blocking asynchronous network operations.
 * @deps    QDialog, QLabel, QMovie
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QDialog>

class QLabel;
class QProgressBar;
class QTimer;

/**
 * @brief 现代科技风异步等待对话框 (Modern Loading/Progress Dialog)
 * 
 * 核心规范与架构设计：
 * 1. 视觉风格与整体工程主题统一：采用无边框深色质感卡片布局与平滑未知进度条；
 * 2. 窗口居中与模态支持：支持 Qt::WindowModal / Qt::ApplicationModal，自动精准居中宿主或屏幕；
 * 3. 超时安全熔断：内置超时保护定时器（默认 45 秒），防止网络请求异常挂起导致界面被永久锁死；
 * 4. 幂等安全释放：提供 finish() 方法，幂等触发关闭与延迟销毁 (deleteLater)；
 * 5. 100% 向下兼容原有构造函数与 setMessage 接口。
 */
class LoadingDialog : public QDialog
{
    Q_OBJECT

public:
    // === [Section: Constants & Dimensions] ===
    static constexpr int    kDefaultWidth   = 320;
    static constexpr int    kDefaultHeight  = 116;
    static constexpr int    kDefaultTimeout = 45000;
    static constexpr double kCardRadius     = 10.0;

    // === [Section: Constructors & Destructor] ===
    explicit LoadingDialog(const QString &message,
                          QWidget *parent = nullptr,
                          Qt::WindowModality modality = Qt::WindowModal,
                          int timeoutMs = kDefaultTimeout);
    ~LoadingDialog() override;

    // === [Section: Public Operations] ===
    /// 动态修改提示文本
    void setMessage(const QString &msg);

    /// 安全完成并延迟释放 (幂等安全，可多次调用)
    void finish();

    /// 设置/更新安全超时时间 (毫秒，<=0 表示禁用超时)
    void setTimeout(int timeoutMs);

protected:
    // === [Section: Event Handlers] ===
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    // === [Section: Private Helpers & UI Setup] ===
    void setupUI(const QString &message);
    void centerOnParentOrScreen();

    QLabel       *m_label        = nullptr;
    QProgressBar *m_progressBar  = nullptr;
    QTimer       *m_timeoutTimer = nullptr;
    bool          m_finished     = false;
};

