#ifndef TOAST_H
#define TOAST_H

#include <QWidget>
#include <QPointer>
#include <QList>

/**
 * @brief 现代深色悬浮轻量级消息提示 (Toast)
 *
 * 架构规范与特性：
 * 1. 非阻塞、自动淡入淡出、深色卡片质感，支持多条消息垂直堆叠平滑排版；
 * 2. 线程安全守护：非 GUI 线程调用自动通过 invokeMethod 调度至主线程，杜绝多线程操作 GUI 崩溃；
 * 3. 内存与指针安全：全局使用 QPointer<Toast> 统一管理，析构时自动注销，杜绝宿主窗口提前销毁引发野指针；
 * 4. 具备最大堆叠数限制 (kMaxActiveToasts)，防止高频重复报错时刷屏占满屏幕；
 * 5. 鼠标悬停感知（Hover Pause）：用户悬停时暂停淡出倒计时，鼠标离开后继续。
 */
class Toast final : public QWidget
{
    Q_OBJECT

public:
    enum Type {
        Success,
        Error,
        Info
    };
    Q_ENUM(Type)

    static void show(QWidget *parent, const QString &message, Type type = Success, int durationMs = 2200);
    static void success(QWidget *parent, const QString &message, int durationMs = 2200) { show(parent, message, Success, durationMs); }
    static void error(QWidget *parent, const QString &message, int durationMs = 2200)   { show(parent, message, Error, durationMs); }
    static void info(QWidget *parent, const QString &message, int durationMs = 2200)    { show(parent, message, Info, durationMs); }

    ~Toast() override;

protected:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent *event) override;
#else
    void enterEvent(QEvent *event) override;
#endif
    void leaveEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    explicit Toast(QWidget *topLevel, const QString &message, Type type, int durationMs);

    void startFadeIn();
    void startFadeOut();
    void reposition();

    static void repositionAll();
    static void cleanExpired();

    int m_durationMs = 2200;
    int m_remainingMs = 2200;
    qint64 m_lastTickTime = 0;
    bool m_isHovered = false;
    bool m_isFadingOut = false;

    class QTimer *m_timer = nullptr;
    QPointer<QWidget> m_anchorWindow;

    static constexpr int kMaxActiveToasts = 5;
    static QList<QPointer<Toast>> s_active;
};

#endif // TOAST_H
