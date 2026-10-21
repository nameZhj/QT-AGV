/**
 * @file    MapEditDialogs.cpp
 * @layer   GUI
 * @brief   Dialog implementations for unsaved changes prompt and map metadata editing.
 * @deps    MapEditDialogs.h, ApiService.h, Toast.h, LoadingDialog.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapEditDialogs.h"
#include "ApiService.h"
#include "Toast.h"
#include "LoadingDialog.h"
#include <QMessageBox>
#include <QPushButton>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QLabel>

namespace MapEditDialogs {

bool checkUnsavedChanges(QWidget *parent, bool dirty,
                         std::function<void()> onSave,
                         std::function<void()> onDiscard)
{
    if (!dirty) return true;
    QMessageBox box(QMessageBox::Question, "未保存的修改", "路网有未保存的修改，是否保存？", QMessageBox::NoButton, parent);
    QPushButton *saveBtn = box.addButton("保存并退出", QMessageBox::AcceptRole);
    QPushButton *discardBtn = box.addButton("放弃修改", QMessageBox::DestructiveRole);
    box.addButton("取消", QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() == saveBtn) {
        if (onSave) onSave();
        return false;
    }
    if (box.clickedButton() == discardBtn) {
        if (onDiscard) onDiscard();
        return true;
    }
    return false;
}

void showEditMapDialog(const QString &mapId, QString &mapName,
                       QLabel *mapInfoLabel, QWidget *parent)
{
    ApiService::instance().getMap(mapId, [mapId, &mapName, mapInfoLabel, parent](int code, const QJsonObject &data) {
        if (code != 200) return;
        QJsonObject info = data.value("data").toObject(data);
        QDialog dlg(parent);
        dlg.setWindowTitle("编辑地图属性");
        auto *form = new QFormLayout(&dlg);
        auto *nameEdit = new QLineEdit(info.value("mapName").toString());
        auto *remarkEdit = new QLineEdit(info.value("remark").toString());
        form->addRow("地图名称:", nameEdit);
        form->addRow("描述:", remarkEdit);
        auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        form->addRow(btns);
        if (dlg.exec() != QDialog::Accepted) return;

        QJsonObject upd;
        upd["mapId"] = mapId;
        upd["mapName"] = nameEdit->text().trimmed();
        upd["remark"] = remarkEdit->text().trimmed();
        auto *loading = new LoadingDialog("正在保存...", parent);
        loading->show();
        ApiService::instance().updateMap(upd, [&mapName, mapInfoLabel, parent, loading, upd](int c2, const QJsonObject &) {
            loading->close(); loading->deleteLater();
            if (c2 == 200) {
                mapName = upd.value("mapName").toString();
                if (mapInfoLabel) mapInfoLabel->setText(QString("路网编辑  %1").arg(mapName));
                Toast::success(parent, "保存成功");
            } else {
                Toast::error(parent, "保存失败");
            }
        });
    });
}

} // namespace MapEditDialogs
