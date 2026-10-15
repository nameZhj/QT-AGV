#include "Toast.h"

#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPropertyAnimation>
#include <QGraphicsDropShadowEffect>
#include <QTimer>
#include <QColor>
#include <QDateTime>
#include <QScreen>
#include <QGuiApplication>
#include <QThread>
#include <QCoreApplication>
#include <algorithm>

QList<QPointer<Toast>> Toast::s_active;

void Toast::show(QWidget *parent, const QString &message, Type type, int durationMs)
{
    // 线程安全守护：如果非主 GUI 线程发起调用，自动调度至主线程执行
    if (QThread::currentThread() != qApp->thread()) {
        QMetaObject::invokeMethod(qApp, [parent, message, type, durationMs]() {
            Toast::show(parent, message, type, durationMs);
        }, Qt::QueuedConnection);
        return;
    }

    QWidget *topLevel = parent ? parent->window() : nullptr;
    cleanExpired();

    // 限制最大活跃 Toast 数量，若超过则平滑加速淘汰最旧的
    while (s_active.size() >= kMaxActiveToasts) {
        if (!s_active.isEmpty() && s_active.first()) {
            s_active.first()->startFadeOut();
            s_active.removeFirst();
        } else {
            s_active.removeFirst();
        }
    }

    auto *toast = new Toast(topLevel, message, type, durationMs);
    toast->QWidget::show();
}

Toast::Toast(QWidget *topLevel, const QString &message, Type type, int durationMs)
    : QWidget(topLevel, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    , m_durationMs(qMax(500, durationMs))
    , m_remainingMs(m_durationMs)
    , m_anchorWindow(topLevel)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_DeleteOnClose);
    setFocusPolicy(Qt::NoFocus);

    if (m_anchorWindow) {
        m_anchorWindow->installEventFilter(this);
    }

    // 视觉配色体系定义
    QString accentColor;
    QString iconSymbol;
    QString iconBgColor;

    switch (type) {
    case Success:
        accentColor = "#10b981"; // 现代薄荷绿
        iconBgColor = "rgba(16, 185, 129, 0.18)";
        iconSymbol  = QString::fromUtf8("\xE2\x9C\x93"); // ✓
        break;
    case Error:
        accentColor = "#ef4444"; // 柔和珊瑚红
        iconBgColor = "rgba(239, 68, 68, 0.18)";
        iconSymbol  = QString::fromUtf8("\xE2\x9C\x95"); // ✕
        break;
    case Warning:
        accentColor = "#f59e0b"; // 琥珀橙黄
        iconBgColor = "rgba(245, 158, 11, 0.18)";
        iconSymbol  = QString::fromUtf8("\xE2\x9A\xA0"); // ⚠
        break;
    case Info:
    default:
        accentColor = "#0284c7"; // 科技青蓝
        iconBgColor = "rgba(2, 132, 199, 0.18)";
        iconSymbol  = QString::fromUtf8("\xE2\x84\xB9"); // ℹ
        break;
    }

    // 1. 卡片容器 (带柔和半透明暗色底板与状态主色左边框)
    auto *card = new QWidget(this);
    card->setObjectName("toastCard");
    card->setStyleSheet(QString(
        "QWidget#toastCard {"
        "  background-color: rgba(30, 41, 59, 0.95);"
        "  border: 1px solid rgba(255, 255, 255, 0.10);"
        "  border-left: 4px solid %1;"
        "  border-radius: 8px;"
        "}"
    ).arg(accentColor));

    // 2. 图标徽章
    auto *iconLbl = new QLabel(iconSymbol, card);
    iconLbl->setFixedSize(24, 24);
    iconLbl->setAlignment(Qt::AlignCenter);
    iconLbl->setStyleSheet(QString(
        "color: %1; background-color: %2; border-radius: 12px; font-size: 13px; font-weight: bold;"
    ).arg(accentColor, iconBgColor));

    // 3. 消息正文
    auto *msgLbl = new QLabel(message, card);
    msgLbl->setStyleSheet("color: #f1f5f9; font-size: 13px; font-weight: 500; background: transparent;");
    msgLbl->setWordWrap(true);

    // 4. 水平内容布局
    auto *hl = new QHBoxLayout(card);
    hl->setContentsMargins(14, 10, 18, 10);
    hl->setSpacing(12);
    hl->addWidget(iconLbl, 0, Qt::AlignVCenter);
    hl->addWidget(msgLbl, 1, Qt::AlignVCenter);

    // 5. 顶层留白 (留出空间给阴影渲染)
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->addWidget(card);

    auto *shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(22);
    shadow->setOffset(0, 5);
    shadow->setColor(QColor(0, 0, 0, 140));
    card->setGraphicsEffect(shadow);

    // 6. 尺寸自适应排版
    adjustSize();
    const int targetWidth = qBound(220, sizeHint().width(), 460);
    setFixedWidth(targetWidth);
    adjustSize();

    // 7. 定时自销毁定时器
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &Toast::startFadeOut);

    s_active.append(this);
    reposition();
    startFadeIn();
}

Toast::~Toast()
{
    if (m_anchorWindow) {
        m_anchorWindow->removeEventFilter(this);
    }
    s_active.removeAll(this);
    repositionAll();
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void Toast::enterEvent(QEnterEvent *event)
#else
void Toast::enterEvent(QEvent *event)
#endif
{
    QWidget::enterEvent(event);
    m_isHovered = true;
    if (m_timer && m_timer->isActive()) {
        m_remainingMs -= static_cast<int>(QDateTime::currentMSecsSinceEpoch() - m_lastTickTime);
        m_remainingMs = qMax(200, m_remainingMs);
        m_timer->stop();
    }
}

void Toast::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    m_isHovered = false;
    if (!m_isFadingOut && m_timer) {
        m_lastTickTime = QDateTime::currentMSecsSinceEpoch();
        m_timer->start(m_remainingMs);
    }
}

bool Toast::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_anchorWindow.data()) {
        if (event->type() == QEvent::Move || event->type() == QEvent::Resize) {
            reposition();
        }
    }
    return QWidget::eventFilter(watched, event);
}

void Toast::startFadeIn()
{
    setWindowOpacity(0.0);
    auto *fadeIn = new QPropertyAnimation(this, "windowOpacity", this);
    fadeIn->setDuration(180);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);
    connect(fadeIn, &QPropertyAnimation::finished, this, [this]() {
        if (!m_isHovered && m_timer) {
            m_lastTickTime = QDateTime::currentMSecsSinceEpoch();
            m_timer->start(m_durationMs);
        }
    });
    fadeIn->start(QAbstractAnimation::DeleteWhenStopped);
}

void Toast::startFadeOut()
{
    if (m_isFadingOut) {
        return;
    }
    m_isFadingOut = true;
    if (m_timer && m_timer->isActive()) {
        m_timer->stop();
    }

    auto *fadeOut = new QPropertyAnimation(this, "windowOpacity", this);
    fadeOut->setDuration(220);
    fadeOut->setStartValue(windowOpacity());
    fadeOut->setEndValue(0.0);
    connect(fadeOut, &QPropertyAnimation::finished, this, [this]() {
        close();
    });
    fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
}

void Toast::reposition()
{
    QRect anchor;
    if (m_anchorWindow && m_anchorWindow->isVisible()) {
        const QPoint topLeftGlobal = m_anchorWindow->mapToGlobal(QPoint(0, 0));
        anchor = QRect(topLeftGlobal, m_anchorWindow->size());
    } else {
        if (QScreen *screen = QGuiApplication::primaryScreen()) {
            anchor = screen->availableGeometry();
        } else {
            anchor = QRect(0, 0, 1280, 800);
        }
    }

    const int index = s_active.indexOf(this);
    if (index < 0) {
        return;
    }

    const int x = anchor.x() + (anchor.width() - width()) / 2;
    const int baseY = anchor.y() + anchor.height() - 90;
    const int y = baseY - index * (height() + 10);

    move(x, y);
}

void Toast::repositionAll()
{
    cleanExpired();
    for (const auto &toast : s_active) {
        if (toast) {
            toast->reposition();
        }
    }
}

void Toast::cleanExpired()
{
    s_active.erase(std::remove_if(s_active.begin(), s_active.end(), [](const QPointer<Toast> &ptr) {
        return ptr.isNull();
    }), s_active.end());
}
