/**
 * @file    CarDialogHelper.cpp
 * @layer   GUI
 * @brief   Implementation of add/edit/link dialogs and certification check for CarWidget.
 * @deps    CarDialogHelper.h, CarWidget.h, ApiService.h, LoadingDialog.h, Toast.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */
#include "CarDialogHelper.h"
#include "CarWidget.h"
#include "network/ApiService.h"
#include "ui/common/LoadingDialog.h"
#include "ui/common/Toast.h"
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QTextEdit>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QMessageBox>
#include <QDateTime>
#include <QPointer>

void CarDialogHelper::showAddDialog(CarWidget *owner)
{
    if (!owner) return;

    QDialog dlg(owner);
    dlg.setWindowTitle("添加设备");
    auto *form = new QFormLayout(&dlg);

    auto *mapCombo = new QComboBox;
    for (const auto &item : owner->m_mapOptions) {
        mapCombo->addItem(item.toObject().value("mapName").toString(),
                          jsonIdStr(item.toObject().value("mapId")));
    }

    auto *nameEdit    = new QLineEdit;
    auto *ipPortEdit  = new QLineEdit;
    ipPortEdit->setPlaceholderText("示例: 192.168.1.100:8888");
    auto *certKeyEdit = new QLineEdit;
    certKeyEdit->setPlaceholderText("请输入认证 Key");

    auto *robotTypeCombo = new QComboBox;
    robotTypeCombo->addItem("室内", "0");
    robotTypeCombo->addItem("室外", "1");

    auto *machineryCombo = new QComboBox;
    machineryCombo->addItem("普通", "0");
    machineryCombo->addItem("顶升", "1");
    machineryCombo->addItem("潜伏", "2");

    auto *remarkEdit = new QTextEdit;
    remarkEdit->setMaximumHeight(60);

    form->addRow("地图名称:", mapCombo);
    form->addRow("设备名称:", nameEdit);
    form->addRow("通讯地址:", ipPortEdit);
    form->addRow("认证 Key:", certKeyEdit);
    form->addRow("类型:",     robotTypeCombo);
    form->addRow("机械属性:", machineryCombo);
    form->addRow("描述:",     remarkEdit);

    QObject::connect(certKeyEdit, &QLineEdit::editingFinished, owner, [owner, certKeyEdit] {
        checkCertification(owner, certKeyEdit->text().trimmed());
    });

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    btns->button(QDialogButtonBox::Ok)->setText("确定");
    btns->button(QDialogButtonBox::Cancel)->setText("取消");
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(btns);

    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    if (nameEdit->text().isEmpty() || ipPortEdit->text().isEmpty()) {
        QMessageBox::warning(owner, "提示", "设备名称和通讯地址不能为空");
        return;
    }
    if (!owner->m_certKeyStatus) {
        QMessageBox::warning(owner, "提示", "认证失败，请检查认证 Key");
        return;
    }

    QJsonObject data;
    data["carName"]         = nameEdit->text();
    data["ipPort"]          = ipPortEdit->text();
    data["mapId"]           = mapCombo->currentData().toString();
    data["robotType"]       = robotTypeCombo->currentData().toString();
    data["machinery"]       = machineryCombo->currentData().toString();
    data["remark"]          = remarkEdit->toPlainText();
    data["certificationId"] = owner->m_certId;

    auto *loading = new LoadingDialog("正在新增设备...", owner);
    loading->show();
    QPointer<CarWidget> self = owner;
    QPointer<LoadingDialog> safeLoading = loading;
    ApiService::instance().addCar(data, [self, safeLoading](int code, const QJsonObject &) {
        if (safeLoading) {
            safeLoading->close();
            safeLoading->deleteLater();
        }
        if (!self) return;
        if (code == 200) {
            Toast::success(self, "新增成功");
            self->getList();
        }
    });
}

void CarDialogHelper::showEditDialog(CarWidget *owner, const QJsonObject &carData)
{
    if (!owner) return;

    QDialog dlg(owner);
    dlg.setWindowTitle("修改设备");
    auto *form = new QFormLayout(&dlg);

    auto *mapCombo = new QComboBox;
    for (const auto &item : owner->m_mapOptions) {
        mapCombo->addItem(item.toObject().value("mapName").toString(),
                          jsonIdStr(item.toObject().value("mapId")));
    }

    auto *nameEdit   = new QLineEdit(carData.value("carName").toString());
    auto *ipPortEdit = new QLineEdit(carData.value("ipPort").toString());
    auto *remarkEdit = new QTextEdit(carData.value("remark").toString());
    remarkEdit->setMaximumHeight(60);

    form->addRow("地图名称:", mapCombo);
    form->addRow("设备名称:", nameEdit);
    form->addRow("通讯地址:", ipPortEdit);
    form->addRow("描述:",     remarkEdit);

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    btns->button(QDialogButtonBox::Ok)->setText("确定");
    btns->button(QDialogButtonBox::Cancel)->setText("取消");
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(btns);

    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    QJsonObject data;
    data["carId"]   = carData.value("carId");
    data["carName"] = nameEdit->text();
    data["ipPort"]  = ipPortEdit->text();
    data["mapId"]   = mapCombo->currentData().toString();
    data["remark"]  = remarkEdit->toPlainText();
    data["updateTime"] = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

    auto *loading = new LoadingDialog("正在保存设备信息...", owner);
    loading->show();
    QPointer<CarWidget> self = owner;
    QPointer<LoadingDialog> safeLoading = loading;
    ApiService::instance().updateCar(data, [self, safeLoading](int code, const QJsonObject &) {
        if (safeLoading) {
            safeLoading->close();
            safeLoading->deleteLater();
        }
        if (!self) return;
        if (code == 200) {
            Toast::success(self, "修改成功");
            self->getList();
        }
    });
}

void CarDialogHelper::showLinkDialog(CarWidget *owner, const QJsonObject &carData)
{
    if (!owner) return;

    QDialog dlg(owner);
    dlg.setWindowTitle("链接设备");
    auto *form = new QFormLayout(&dlg);

    auto *certKeyEdit = new QLineEdit;
    certKeyEdit->setPlaceholderText("请输入认证 Key");
    form->addRow("认证 Key:", certKeyEdit);

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    btns->button(QDialogButtonBox::Ok)->setText("确定");
    btns->button(QDialogButtonBox::Cancel)->setText("取消");
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(btns);

    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    QJsonObject data;
    data["carId"]   = carData.value("carId");
    data["carName"] = carData.value("carName");
    data["mapId"]   = carData.value("mapId");
    QJsonObject cert;
    cert["certificationKey"] = certKeyEdit->text().trimmed();
    data["certification"] = cert;

    auto *loading = new LoadingDialog("正在链接设备...", owner);
    loading->show();
    QPointer<CarWidget> self = owner;
    QPointer<LoadingDialog> safeLoading = loading;
    ApiService::instance().linkCar(data, [self, safeLoading](int code, const QJsonObject &) {
        if (safeLoading) {
            safeLoading->close();
            safeLoading->deleteLater();
        }
        if (!self) return;
        if (code == 200) {
            Toast::success(self, "已申请链接");
        }
    });
}

void CarDialogHelper::checkCertification(CarWidget *owner, const QString &ckey)
{
    if (!owner || ckey.isEmpty() || ckey == owner->m_certOldKey) return;
    owner->m_certId.clear();
    QPointer<CarWidget> self = owner;
    ApiService::instance().checkCertification(ckey, [self, ckey](int code, const QJsonObject &data) {
        if (!self) return;
        self->m_certKeyStatus = (code == 200);
        if (code == 200) {
            self->m_certId = jsonIdStr(data.value("certificationId"));
        }
        self->m_certOldKey = ckey;
    });
}
