/**
 * @file    DeviceManagerAddPanel.cpp
 * @layer   GUILayer
 * @brief   Implementation of DeviceManagerDialog add-device panel.
 * @deps    DeviceManagerAddPanel.h, DeviceManagerDialog.h, AppConfig.h, Toast.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "DeviceManagerAddPanel.h"
#include "DeviceManagerDialog.h"
#include "AppConfig.h"
#include "Toast.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QDateTime>

QWidget* DeviceManagerAddPanel::buildAddPanel(DeviceManagerDialog *dlg)
{
    auto *panel = new QWidget;
    panel->setStyleSheet("background:#1a1d23;");
    auto *al = new QVBoxLayout(panel);
    al->setContentsMargins(0, 0, 0, 0);
    al->setSpacing(0);

    auto *phdr = new QWidget;
    phdr->setFixedHeight(52);
    phdr->setStyleSheet("background:#12151a; border-bottom:1px solid #2d3340;");
    auto *phl = new QHBoxLayout(phdr);
    phl->setContentsMargins(16, 0, 16, 0);

    dlg->m_cancelBtn = new QPushButton("← 返回");
    dlg->m_cancelBtn->setFixedHeight(32);
    dlg->m_cancelBtn->setStyleSheet(
        "QPushButton{background:transparent;color:#8898a8;border:1px solid #3a3f4a;"
        "border-radius:4px;padding:0 12px;font-size:12px;}"
        "QPushButton:hover{color:#c8d0d8;border-color:#8898a8;}");
    QObject::connect(dlg->m_cancelBtn, &QPushButton::clicked, dlg, &DeviceManagerDialog::onCancelAdd);
    phl->addWidget(dlg->m_cancelBtn);
    phl->addStretch();

    auto *panelTitle = new QLabel("添加设备");
    panelTitle->setStyleSheet("color:#c8d0d8; font-size:15px; font-weight:700;");
    phl->addWidget(panelTitle);
    phl->addStretch();
    phl->addSpacing(80);

    al->addWidget(phdr);

    auto *center = new QWidget;
    auto *cl = new QVBoxLayout(center);
    cl->setContentsMargins(0, 60, 0, 0);
    cl->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    cl->setSpacing(20);

    auto *formBox = new QWidget;
    formBox->setFixedWidth(500);
    formBox->setStyleSheet("background:#1e2228; border:1px solid #2d3340; border-radius:8px;");
    auto *fbl = new QVBoxLayout(formBox);
    fbl->setContentsMargins(32, 28, 32, 28);
    fbl->setSpacing(16);

    auto *fTitle = new QLabel("输入设备IP");
    fTitle->setStyleSheet("color:#c8d0d8; font-size:16px; font-weight:700;");
    fbl->addWidget(fTitle);

    fbl->addSpacing(4);

    dlg->m_ipEdit = new QLineEdit;
    dlg->m_ipEdit->setPlaceholderText("例如: 192.168.1.100");
    dlg->m_ipEdit->setFixedHeight(42);
    fbl->addWidget(dlg->m_ipEdit);

    dlg->m_nameRow = new QWidget;
    auto *nrl = new QVBoxLayout(dlg->m_nameRow);
    nrl->setContentsMargins(0, 0, 0, 0);
    nrl->setSpacing(6);

    auto *nameLbl = new QLabel("设备名称");
    nameLbl->setStyleSheet("color:#8898a8; font-size:12px;");
    nrl->addWidget(nameLbl);

    dlg->m_nameEdit = new QLineEdit;
    dlg->m_nameEdit->setPlaceholderText("例如: AGV-01 (选填，默认 AGV @ IP)");
    dlg->m_nameEdit->setFixedHeight(42);
    nrl->addWidget(dlg->m_nameEdit);

    fbl->addWidget(dlg->m_nameRow);

    dlg->m_addStatus = new QLabel;
    dlg->m_addStatus->setStyleSheet("color:#f0a060; font-size:12px;");
    dlg->m_addStatus->setWordWrap(true);
    dlg->m_addStatus->setMinimumHeight(18);
    fbl->addWidget(dlg->m_addStatus);

    dlg->m_confirmBtn = new QPushButton("添加设备");
    dlg->m_confirmBtn->setFixedHeight(42);
    dlg->m_confirmBtn->setStyleSheet(
        "QPushButton{background:#00bcd4;color:#fff;border:none;"
        "border-radius:4px;font-size:14px;font-weight:700;}"
        "QPushButton:hover{background:#00acc1;}"
        "QPushButton:disabled{background:#2d3340;color:#556677;}");
    fbl->addWidget(dlg->m_confirmBtn);

    QObject::connect(dlg->m_confirmBtn, &QPushButton::clicked, dlg, &DeviceManagerDialog::onConfirmAdd);
    QObject::connect(dlg->m_ipEdit, &QLineEdit::returnPressed, dlg->m_confirmBtn, &QPushButton::click);

    cl->addWidget(formBox, 0, Qt::AlignHCenter);
    al->addWidget(center, 1);

    return panel;
}

void DeviceManagerAddPanel::onAddDevice(DeviceManagerDialog *dlg)
{
    if (!dlg) return;
    dlg->m_ipEdit->clear();
    if (dlg->m_nameEdit) dlg->m_nameEdit->clear();
    dlg->m_addStatus->clear();
    dlg->m_confirmBtn->setEnabled(true);
    dlg->m_confirmBtn->setText("添加设备");
    dlg->m_stack->setCurrentWidget(dlg->m_addPanel);
    dlg->m_ipEdit->setFocus();
}

void DeviceManagerAddPanel::onCancelAdd(DeviceManagerDialog *dlg)
{
    if (!dlg) return;
    dlg->m_stack->setCurrentWidget(dlg->m_mainPanel);
}

void DeviceManagerAddPanel::onConfirmAdd(DeviceManagerDialog *dlg)
{
    if (!dlg) return;
    QString ip = dlg->m_ipEdit->text().trimmed();
    if (ip.startsWith("http://"))  ip = ip.mid(7);
    if (ip.startsWith("https://")) ip = ip.mid(8);
    dlg->m_ipEdit->setText(ip);

    if (ip.isEmpty()) {
        dlg->m_addStatus->setStyleSheet("color:#e05555; font-size:12px;");
        dlg->m_addStatus->setText("请输入服务器 IP 地址");
        return;
    }

    QString name = dlg->m_nameEdit ? dlg->m_nameEdit->text().trimmed() : "";
    if (name.isEmpty()) {
        name = QString("AGV @ %1").arg(ip);
    }

    DeviceProfile dev;
    dev.ip            = ip;
    dev.carId         = "1";
    dev.name          = name;
    dev.lastConnected = QDateTime::currentDateTime();
    dev.linkStatus    = "0";
    AppConfig::instance().saveDevice(dev);
    AppConfig::instance().setServerIp(ip);
    AppConfig::instance().setAgvId(dev.carId);

    dlg->m_stack->setCurrentWidget(dlg->m_mainPanel);
    dlg->refreshCards();
    dlg->setStatus(QString("设备 [%1] 已添加").arg(dev.name), "#5cb85c");
    Toast::success(dlg, QString("设备 [%1] 已添加").arg(dev.name));
}
