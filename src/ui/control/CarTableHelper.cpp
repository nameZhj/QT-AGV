/**
 * @file    CarTableHelper.cpp
 * @layer   GUI
 * @brief   Implementation of table row populator, map name resolver, and row actions for CarWidget.
 * @deps    CarTableHelper.h, CarWidget.h, ApiService.h, RobotInfoManager.h, LoadingDialog.h, Toast.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */
#include "CarTableHelper.h"
#include "CarWidget.h"
#include "network/ApiService.h"
#include "core/RobotInfoManager.h"
#include "ui/common/LoadingDialog.h"
#include "ui/common/Toast.h"
#include <QTableWidget>
#include <QComboBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPointer>

QString CarTableHelper::resolveMapNameById(const QJsonArray &mapOptions, int mapId)
{
    if (mapId <= 0) return QString();
    const QString idStr = QString::number(mapId);
    for (const auto &m : mapOptions) {
        const QJsonObject obj = m.toObject();
        if (jsonIdStr(obj.value(QStringLiteral("mapId"))) == idStr ||
            obj.value(QStringLiteral("mapId")).toInt() == mapId) {
            return obj.value(QStringLiteral("mapName")).toString().trimmed();
        }
    }
    return QString();
}

void CarTableHelper::populateCarRow(CarWidget *owner, QTableWidget *table, int row,
                                   const QJsonObject &car, const QJsonArray &mapOptions)
{
    if (!owner || !table) return;

    const QJsonObject carInfo = car.value(QStringLiteral("carInfo")).toObject();
    const QString cid = jsonIdStr(car.value(QStringLiteral("carId")));
    QString mapName = car.value(QStringLiteral("mapName")).toString();
    QString statusText = (carInfo.value(QStringLiteral("status")).toString() == QStringLiteral("0")) ? QStringLiteral("在线") : QStringLiteral("离线");
    QColor statusCol = (carInfo.value(QStringLiteral("status")).toString() == QStringLiteral("0")) ? QColor(QStringLiteral("#22c55e")) : QColor(QStringLiteral("#64748b"));

    if (RobotInfoManager::instance().hasInfoForCar(cid)) {
        const RobotInfo rInfo = RobotInfoManager::instance().infoForCar(cid);
        statusText = rInfo.statusText();
        statusCol = QColor(rInfo.statusColor());
        if (mapName.isEmpty() && rInfo.map_id != 0) {
            const QString mname = resolveMapNameById(mapOptions, rInfo.map_id);
            mapName = !mname.isEmpty() ? mname : QString::number(rInfo.map_id);
        }
    }

    table->setItem(row, 0, new QTableWidgetItem(cid));
    table->setItem(row, 1, new QTableWidgetItem(car.value(QStringLiteral("carName")).toString()));
    table->setItem(row, 2, new QTableWidgetItem(mapName));

    auto *stItem = new QTableWidgetItem(statusText);
    stItem->setForeground(QBrush(statusCol));
    table->setItem(row, 3, stItem);

    const bool isLinked = (car.value(QStringLiteral("linkStatus")).toString() == QStringLiteral("0"));
    const bool isEnabled = (car.value(QStringLiteral("status")).toString() == QStringLiteral("0"));
    table->setItem(row, 4, new QTableWidgetItem(isLinked ? QStringLiteral("已链接") : QStringLiteral("未链接")));

    // Status combo
    auto *statusWidget = new QWidget;
    auto *statusLayout = new QHBoxLayout(statusWidget);
    statusLayout->setContentsMargins(4, 0, 4, 0);
    auto *statusCombo = new QComboBox;
    statusCombo->addItem(QStringLiteral("正常"), QStringLiteral("0"));
    statusCombo->addItem(QStringLiteral("停用"), QStringLiteral("2"));
    statusCombo->setCurrentIndex(isEnabled ? 0 : 1);
    QObject::connect(statusCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     owner, [owner, row](int) { owner->onStatusChanged(row); });
    statusLayout->addWidget(statusCombo);
    table->setCellWidget(row, 5, statusWidget);

    table->setItem(row, 6, new QTableWidgetItem(car.value(QStringLiteral("remark")).toString()));

    // Action buttons
    auto *opWidget = new QWidget;
    auto *opLayout = new QHBoxLayout(opWidget);
    opLayout->setContentsMargins(4, 2, 4, 2);
    opLayout->setSpacing(4);
    auto *editBtn = new QPushButton(QStringLiteral("修改"));
    auto *delBtn  = new QPushButton(QStringLiteral("删除"));
    auto *linkBtn = new QPushButton(QStringLiteral("链接"));

    linkBtn->setEnabled(!isLinked && isEnabled);
    if (isLinked) {
        linkBtn->setToolTip(QStringLiteral("设备已链接"));
    } else if (!isEnabled) {
        linkBtn->setToolTip(QStringLiteral("设备已停用，无法链接"));
    }

    QObject::connect(editBtn, &QPushButton::clicked, owner, [owner, row] {
        if (row >= 0 && row < owner->m_carList.size()) {
            owner->showEditDialog(owner->m_carList[row].toObject());
        }
    });
    QObject::connect(delBtn, &QPushButton::clicked, owner, [owner, row] {
        (void)row; owner->onDelete();
    });
    QObject::connect(linkBtn, &QPushButton::clicked, owner, [owner, row] {
        if (row >= 0 && row < owner->m_carList.size()) {
            owner->showLinkDialog(owner->m_carList[row].toObject());
        }
    });

    opLayout->addWidget(editBtn);
    opLayout->addWidget(delBtn);
    opLayout->addWidget(linkBtn);
    table->setCellWidget(row, 7, opWidget);
}

void CarTableHelper::handleDelete(CarWidget *owner)
{
    if (!owner || !owner->m_table) return;
    int row = owner->m_table->currentRow();
    if (row < 0 || row >= owner->m_carList.size()) return;
    QJsonObject car = owner->m_carList[row].toObject();
    if (car.value("status").toString() != "2") {
        QMessageBox::warning(owner, "提示", "设备未停用，无法删除！");
        return;
    }
    QMessageBox msgBox(owner);
    msgBox.setWindowTitle("确认");
    msgBox.setText(QString("确认删除设备「%1」？").arg(car.value("carName").toString()));
    QPushButton *yesBtn = msgBox.addButton("确定", QMessageBox::YesRole);
    msgBox.addButton("取消", QMessageBox::NoRole);
    msgBox.exec();
    if (msgBox.clickedButton() == yesBtn) {
        auto *loading = new LoadingDialog("正在删除设备...", owner);
        loading->show();
        QPointer<CarWidget> self = owner;
        QPointer<LoadingDialog> safeLoading = loading;
        ApiService::instance().delCar(jsonIdStr(car.value("carId")), [self, safeLoading](int code, const QJsonObject &) {
            if (safeLoading) {
                safeLoading->close();
                safeLoading->deleteLater();
            }
            if (!self) return;
            if (code == 200) {
                Toast::success(self, "删除成功");
                self->getList();
            }
        });
    }
}

void CarTableHelper::handleStatusChanged(CarWidget *owner, int row)
{
    if (!owner || !owner->m_table || row < 0 || row >= owner->m_carList.size()) return;
    QJsonObject car = owner->m_carList[row].toObject();
    auto *w     = owner->m_table->cellWidget(row, 5);
    if (!w) return;
    auto *combo = w->findChild<QComboBox *>();
    if (!combo) return;

    QString newStatus = combo->currentData().toString();
    QString text = newStatus == "0" ? "启用" : "停用";
    QMessageBox msgBox2(owner);
    msgBox2.setWindowTitle("确认");
    msgBox2.setText(QString("确认要「%1」设备「%2」吗？").arg(text, car.value("carName").toString()));
    QPushButton *yesBtn2 = msgBox2.addButton("确定", QMessageBox::YesRole);
    msgBox2.addButton("取消", QMessageBox::NoRole);
    msgBox2.exec();
    if (msgBox2.clickedButton() == yesBtn2) {
        auto *loading = new LoadingDialog("正在更改设备状态...", owner);
        loading->show();
        QPointer<CarWidget> self = owner;
        QPointer<LoadingDialog> safeLoading = loading;
        QPointer<QComboBox> safeCombo = combo;
        ApiService::instance().changeCarStatus(
            jsonIdStr(car.value("carId")), jsonIdStr(car.value("mapId")), newStatus,
            [self, safeLoading, safeCombo, newStatus](int code, const QJsonObject &) {
                if (safeLoading) {
                    safeLoading->close();
                    safeLoading->deleteLater();
                }
                if (!self) return;
                if (code == 200) {
                    Toast::success(self, "状态已更新");
                } else {
                    if (safeCombo) {
                        safeCombo->setCurrentIndex(newStatus == "0" ? 1 : 0);
                    }
                }
            });
    }
}
