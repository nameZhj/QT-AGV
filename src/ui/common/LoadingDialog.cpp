#include "LoadingDialog.h"

#include <QLabel>
#include <QProgressBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QScreen>
#include <QDebug>

LoadingDialog::LoadingDialog(const QString &message, QWidget *parent, Qt::WindowModality modality, int timeoutMs)
    : QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint)
    , m_timeoutTimer(new QTimer(this))
{
    setWindowTitle(QStringLiteral("请稍候"));
    setWindowModality(modality);
    setAttribute(Qt::WA_TranslucentBackground);

    setupUI(message);
    setTimeout(timeoutMs);

    connect(m_timeoutTimer, &QTimer::timeout, this, [this]() {
        qWarning().noquote() << QStringLiteral("[LoadingDialog Timeout] 异步等待超时，自动关闭弹窗: ")
                                << (m_label ? m_label->text() : QString());
        finish();
    });
}

LoadingDialog::~LoadingDialog()
{
    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
    }
}

void LoadingDialog::setupUI(const QString &message)
{
    setFixedSize(320, 116);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(24, 22, 24, 20);
    rootLayout->setSpacing(14);

    m_label = new QLabel(message, this);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setWordWrap(true);
    m_label->setStyleSheet(QStringLiteral(
        "color: #e6edf3; "
        "font-size: 13px; "
        "font-weight: 500; "
        "background: transparent;"
    ));
    rootLayout->addWidget(m_label);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setMinimum(0);
    m_progressBar->setMaximum(0); // 跑马灯流动进度动画
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(6);
    m_progressBar->setStyleSheet(QStringLiteral(
        "QProgressBar { "
        "   background-color: #222734; "
        "   border: none; "
        "   border-radius: 3px; "
        "} "
        "QProgressBar::chunk { "
        "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #3b82f6, stop:1 #10b981); "
        "   border-radius: 3px; "
        "}"
    ));
    rootLayout->addWidget(m_progressBar);
}

void LoadingDialog::setMessage(const QString &msg)
{
    if (m_label) {
        m_label->setText(msg);
    }
}

void LoadingDialog::setTimeout(int timeoutMs)
{
    if (!m_timeoutTimer) return;

    m_timeoutTimer->stop();
    if (timeoutMs > 0) {
        m_timeoutTimer->setInterval(timeoutMs);
        m_timeoutTimer->setSingleShot(true);
        m_timeoutTimer->start();
    }
}

void LoadingDialog::finish()
{
    if (m_finished) {
        return;
    }
    m_finished = true;

    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
    }

    close();
    deleteLater();
}

void LoadingDialog::centerOnParentOrScreen()
{
    QWidget *pw = parentWidget();
    if (pw) {
        QWidget *topW = pw->window();
        const QPoint center = topW->mapToGlobal(topW->rect().center());
        move(center.x() - width() / 2, center.y() - height() / 2);
    } else if (QScreen *scr = QApplication::primaryScreen()) {
        const QRect sg = scr->availableGeometry();
        move(sg.center().x() - width() / 2, sg.center().y() - height() / 2);
    }
}

void LoadingDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    centerOnParentOrScreen();
    if (m_timeoutTimer && m_timeoutTimer->interval() > 0 && !m_timeoutTimer->isActive()) {
        m_timeoutTimer->start();
    }
}

void LoadingDialog::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF cardRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath path;
    path.addRoundedRect(cardRect, 10.0, 10.0);

    // 深色卡片背景 (#161922)
    painter.fillPath(path, QColor(0x16, 0x19, 0x22));

    // 高精边框线 (#2d3340)
    painter.setPen(QPen(QColor(0x2d, 0x33, 0x40), 1.2));
    painter.drawPath(path);
}
