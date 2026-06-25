#include "LoadingDialog.h"
#include <QVBoxLayout>
#include <QProgressBar>
#include <QApplication>
#include <QScreen>

LoadingDialog::LoadingDialog(const QString &message, QWidget *parent, Qt::WindowModality modality)
    : QDialog(parent, Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint)
{
    setWindowTitle("请稍候");
    setWindowModality(modality);
    setFixedSize(300, 110);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(24, 18, 24, 18);
    lay->setSpacing(14);

    m_label = new QLabel(message, this);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setWordWrap(true);
    lay->addWidget(m_label);

    auto *bar = new QProgressBar(this);
    bar->setMinimum(0);
    bar->setMaximum(0); // 不确定进度动画
    bar->setTextVisible(false);
    bar->setFixedHeight(8);
    lay->addWidget(bar);

    // 居中于父窗口或屏幕
    if (parent) {
        QPoint center = parent->mapToGlobal(parent->rect().center());
        move(center.x() - width() / 2, center.y() - height() / 2);
    } else if (QScreen *scr = QApplication::primaryScreen()) {
        QRect sg = scr->availableGeometry();
        move(sg.center().x() - width() / 2, sg.center().y() - height() / 2);
    }
}

void LoadingDialog::setMessage(const QString &msg)
{
    m_label->setText(msg);
}
