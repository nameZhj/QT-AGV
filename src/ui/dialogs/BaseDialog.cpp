/**
 * @file    BaseDialog.cpp
 * @module  GUILayer
 * @brief   Implementation of standardized modal dialog base class.
 * @deps    BaseDialog.h, QVBoxLayout, QHBoxLayout
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "BaseDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScreen>
#include <QGuiApplication>

BaseDialog::BaseDialog(const QString &title, QWidget *parent)
    : QDialog(parent)
{
    if (!title.isEmpty()) {
        setWindowTitle(title);
    }
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    m_rootLayout = new QVBoxLayout(this);
    m_rootLayout->setContentsMargins(18, 16, 18, 16);
    m_rootLayout->setSpacing(14);
}

QHBoxLayout* BaseDialog::createButtonBox(const QString &okText, const QString &cancelText) {
    auto *btnLayout = new QHBoxLayout;
    btnLayout->setSpacing(10);
    btnLayout->addStretch(1);

    if (!cancelText.isEmpty()) {
        m_cancelBtn = new QPushButton(cancelText, this);
        m_cancelBtn->setObjectName("btnSecondary");
        m_cancelBtn->setCursor(Qt::PointingHandCursor);
        connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        btnLayout->addWidget(m_cancelBtn);
    }

    if (!okText.isEmpty()) {
        m_okBtn = new QPushButton(okText, this);
        m_okBtn->setObjectName("btnPrimary");
        m_okBtn->setCursor(Qt::PointingHandCursor);
        connect(m_okBtn, &QPushButton::clicked, this, &QDialog::accept);
        btnLayout->addWidget(m_okBtn);
    }

    return btnLayout;
}

void BaseDialog::centerOnParent() {
    QWidget *p = parentWidget();
    if (p) {
        QPoint centerPoint = p->geometry().center();
        move(centerPoint.x() - width() / 2, centerPoint.y() - height() / 2);
    } else {
        QScreen *screen = QGuiApplication::primaryScreen();
        if (screen) {
            QRect screenGeom = screen->availableGeometry();
            move(screenGeom.center().x() - width() / 2, screenGeom.center().y() - height() / 2);
        }
    }
}

void BaseDialog::showEvent(QShowEvent *event) {
    QDialog::showEvent(event);
    centerOnParent();
}
