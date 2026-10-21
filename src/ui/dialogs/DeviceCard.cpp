/**
 * @file    DeviceCard.cpp
 * @layer   GUILayer
 * @brief   Single device card frame widget lifecycle, event filtering, and orchestration.
 * @deps    DeviceCard.h, DeviceCardLayout.h, DeviceCardDisplay.h, DeviceCardProbe.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "DeviceCard.h"
#include "DeviceCardLayout.h"
#include "DeviceCardDisplay.h"
#include "DeviceCardProbe.h"
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QAbstractButton>

DeviceCard::DeviceCard(const DeviceProfile &profile, int index, QWidget *parent)
    : QFrame(parent), m_index(index), m_profile(profile)
{
    setObjectName("deviceCard");
    setFrameShape(QFrame::StyledPanel);
    setFixedSize(260, 335);

    m_shadow = new QGraphicsDropShadowEffect(this);
    m_shadow->setBlurRadius(16);
    m_shadow->setOffset(0, 4);
    m_shadow->setColor(QColor(0, 0, 0, 160));
    setGraphicsEffect(m_shadow);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    DeviceCardLayout::buildHeader(this, root);
    DeviceCardLayout::buildBody(this, root);
    DeviceCardLayout::buildFooter(this, root);

    setCursor(Qt::PointingHandCursor);
    setToolTip("双击卡片进入主页");

    if (m_profile.version.isEmpty() || m_profile.version.startsWith("v3.4.8")) {
        m_profile.version = "v2.1.1";
    }
    m_fwVersion = "v2.1.1";

    bool isCurrent = (AppConfig::instance().currentDeviceIndex() == m_index);
    setOnlineUI(isCurrent);
    updateCardDisplay();

    const auto widgets = findChildren<QWidget*>();
    for (QWidget *w : widgets) {
        if (!qobject_cast<QAbstractButton*>(w)) {
            w->installEventFilter(this);
        }
    }

    if (isCurrent && !profile.ip.isEmpty()) {
        probeDeviceStatus(profile.ip);
    }
}

DeviceCard::~DeviceCard()
{
    if (m_latencyTimer) {
        m_latencyTimer->stop();
    }
}

void DeviceCard::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QWidget *child = childAt(event->pos());
        while (child && child != this) {
            if (qobject_cast<QAbstractButton*>(child)) {
                QFrame::mouseDoubleClickEvent(event);
                return;
            }
            child = child->parentWidget();
        }
        emit doubleClicked(m_index);
        event->accept();
        return;
    }
    QFrame::mouseDoubleClickEvent(event);
}

bool DeviceCard::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonDblClick) {
        auto *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            if (!qobject_cast<QAbstractButton*>(watched)) {
                emit doubleClicked(m_index);
                return true;
            }
        }
    }
    return QFrame::eventFilter(watched, event);
}

void DeviceCard::disconnectDevice()
{
    m_isBusy = false;
    if (m_latencyTimer) {
        m_latencyTimer->stop();
    }
    m_latency = -1;
    m_isProbingLatency = false;
    m_confidence = -1.0;
    m_statusText = "未连接";
    setOnlineUI(false);
}

void DeviceCard::setBusy(bool busy)
{
    m_isBusy = busy;
    updateCardDisplay();
}

void DeviceCard::setLinkStatus(bool)
{
}

void DeviceCard::setOnlineUI(bool online)
{
    DeviceCardDisplay::setOnlineUI(this, online);
}

void DeviceCard::updateCardDisplay()
{
    DeviceCardDisplay::updateCardDisplay(this);
}

void DeviceCard::probeDeviceStatus(const QString &ip)
{
    DeviceCardProbe::probeDeviceStatus(this, ip);
}
