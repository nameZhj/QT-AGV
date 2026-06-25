#include "Toast.h"
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPropertyAnimation>
#include <QGraphicsDropShadowEffect>
#include <QTimer>
#include <QColor>

QList<Toast *> Toast::s_active;

void Toast::show(QWidget *parent, const QString &message, Type type)
{
    QWidget *topLevel = parent ? parent->window() : nullptr;
    auto *t = new Toast(topLevel, message, type);
    t->QWidget::show();
}

Toast::Toast(QWidget *topLevel, const QString &message, Type type)
    : QWidget(topLevel, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_DeleteOnClose);
    setFocusPolicy(Qt::NoFocus);

    QString accent = type == Success ? "#5cb85c" : type == Error ? "#e05555" : "#4a9eff";
    QString icon   = type == Success ? QString::fromUtf8("\xE2\x9C\x93") // ✓
                   : type == Error   ? QString::fromUtf8("\xE2\x9C\x95") // ✕
                                      : QString::fromUtf8("\xE2\x84\xB9"); // ℹ

    auto *card = new QWidget(this);
    card->setObjectName("toastCard");
    card->setStyleSheet(QString(
        "QWidget#toastCard{background:#252a35; border:1px solid %1; border-radius:8px;}"
    ).arg(accent));

    auto *iconLbl = new QLabel(icon, card);
    iconLbl->setStyleSheet(QString(
        "color:%1; font-size:14px; font-weight:700; background:transparent;").arg(accent));

    auto *msgLbl = new QLabel(message, card);
    msgLbl->setStyleSheet("color:#e0e8f0; font-size:13px; background:transparent;");
    msgLbl->setWordWrap(true);

    auto *hl = new QHBoxLayout(card);
    hl->setContentsMargins(14, 10, 18, 10);
    hl->setSpacing(10);
    hl->addWidget(iconLbl, 0, Qt::AlignTop);
    hl->addWidget(msgLbl, 1);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8); // 留白给阴影
    root->addWidget(card);

    auto *shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(20);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 150));
    card->setGraphicsEffect(shadow);

    adjustSize();
    int w = qBound(200, sizeHint().width(), 420);
    setFixedWidth(w);
    adjustSize();

    s_active.append(this);
    reposition();

    setWindowOpacity(0.0);
    auto *fadeIn = new QPropertyAnimation(this, "windowOpacity", this);
    fadeIn->setDuration(180);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);
    fadeIn->start(QAbstractAnimation::DeleteWhenStopped);

    QTimer::singleShot(2200, this, [this]() {
        auto *fadeOut = new QPropertyAnimation(this, "windowOpacity", this);
        fadeOut->setDuration(220);
        fadeOut->setStartValue(1.0);
        fadeOut->setEndValue(0.0);
        connect(fadeOut, &QPropertyAnimation::finished, this, [this]() {
            s_active.removeOne(this);
            for (Toast *t : s_active) t->reposition();
            close();
        });
        fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

void Toast::reposition()
{
    QWidget *top = parentWidget();
    QRect anchor;
    if (top) {
        QPoint topLeftGlobal = top->mapToGlobal(QPoint(0, 0));
        anchor = QRect(topLeftGlobal, top->size());
    } else {
        anchor = QRect(QPoint(0, 0), QSize(1280, 800));
    }

    int index = s_active.indexOf(this);
    if (index < 0) index = 0;

    int x = anchor.x() + (anchor.width() - width()) / 2;
    int baseY = anchor.y() + anchor.height() - 90;
    int y = baseY - index * (height() + 10);

    move(x, y);
}
