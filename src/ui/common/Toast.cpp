/**
 * @file    Toast.cpp
 * @module  GUILayer
 * @brief   Implementation of non-intrusive floating toast notification widget.
 * @deps    Toast.h, QLabel, QTimer
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

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

namespace {
constexpr int kMinDurationMs     = 500;
constexpr int kFadeInDurationMs  = 180;
constexpr int kFadeOutDurationMs = 220;
constexpr int kMinToastWidth     = 220;
constexpr int kMaxToastWidth     = 460;
constexpr int kToastShadowBlur   = 22;

void resolveToastMeta(Toast::Type type, QString &outTypeStr, QString &outIconSymbol)
{
    switch (type) {
    case Toast::Success:
        outTypeStr = QStringLiteral("success");
        outIconSymbol = QString::fromUtf8("\xE2\x9C\x93"); // ✓
        break;
    case Toast::Error:
        outTypeStr = QStringLiteral("error");
        outIconSymbol = QString::fromUtf8("\xE2\x9C\x95"); // ✕
        break;
    case Toast::Warning:
        outTypeStr = QStringLiteral("warning");
        outIconSymbol = QString::fromUtf8("\xE2\x9A\xA0"); // ⚠
        break;
    case Toast::Info:
    default:
        outTypeStr = QStringLiteral("info");
        outIconSymbol = QString::fromUtf8("\xE2\x84\xB9"); // ℹ
        break;
    }
}
} // namespace

QList<QPointer<Toast>> Toast::s_active;

// === [Section: Public Static Factory API] ===
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
        }
        s_active.removeFirst();
    }

    auto *toast = new Toast(topLevel, message, type, durationMs);
    toast->QWidget::show();
}

// === [Section: Constructors & Destructor] ===
Toast::Toast(QWidget *topLevel, const QString &message, Type type, int durationMs)
    : QWidget(topLevel, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    , m_durationMs(qMax(kMinDurationMs, durationMs))
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

    // 视觉配色体系定义与动态属性映射
    QString typeStr;
    QString iconSymbol;
    resolveToastMeta(type, typeStr, iconSymbol);

    // 1. 卡片容器
    auto *card = new QWidget(this);
    card->setObjectName("toastCard");
    card->setProperty("toastType", typeStr);

    // 2. 图标徽章
    auto *iconLbl = new QLabel(iconSymbol, card);
    iconLbl->setObjectName("toastIcon");
    iconLbl->setProperty("toastType", typeStr);
    iconLbl->setFixedSize(24, 24);
    iconLbl->setAlignment(Qt::AlignCenter);

    // 3. 消息正文
    auto *msgLbl = new QLabel(message, card);
    msgLbl->setObjectName("toastMsg");
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
    shadow->setBlurRadius(kToastShadowBlur);
    shadow->setOffset(0, 5);
    shadow->setColor(QColor(0, 0, 0, 140));
    card->setGraphicsEffect(shadow);

    // 6. 尺寸自适应排版
    adjustSize();
    const int targetWidth = qBound(kMinToastWidth, sizeHint().width(), kMaxToastWidth);
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

// === [Section: Event Handlers] ===
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
    if (watched == m_anchorWindow.data() && (event->type() == QEvent::Move || event->type() == QEvent::Resize)) {
        reposition();
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
