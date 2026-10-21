/**
 * @file    MapWidgetDialogs.cpp
 * @layer   GUILayer
 * @brief   Implementation of map creation and rename dialogs.
 * @deps    MapWidgetDialogs.h, network/ApiService.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapWidgetDialogs.h"
#include "network/ApiService.h"
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QPointer>

namespace MapWidgetDialogs {

void showAddDialog(QWidget *parent, std::function<void()> onSuccess)
{
    QDialog dlg(parent);
    dlg.setWindowTitle("添加地图");
    QFormLayout *form = new QFormLayout(&dlg);

    QLineEdit *nameEdit = new QLineEdit;
    QTextEdit *remarkEdit = new QTextEdit;
    remarkEdit->setMaximumHeight(60);

    // 文件选择
    QLineEdit *pgmPath = new QLineEdit;
    QPushButton *pgmBtn = new QPushButton("选择");
    QLineEdit *yamlPath = new QLineEdit;
    QPushButton *yamlBtn = new QPushButton("选择");
    QLineEdit *pbStreamPath = new QLineEdit;
    QPushButton *pbStreamBtn = new QPushButton("选择");
    QLineEdit *reflectorBinPath = new QLineEdit;
    QPushButton *reflectorBinBtn = new QPushButton("选择");
    QLineEdit *reflectorJsonPath = new QLineEdit;
    QPushButton *reflectorJsonBtn = new QPushButton("选择");

    pgmPath->setReadOnly(true);
    yamlPath->setReadOnly(true);
    pbStreamPath->setReadOnly(true);
    reflectorBinPath->setReadOnly(true);
    reflectorJsonPath->setReadOnly(true);

    QObject::connect(pgmBtn, &QPushButton::clicked, [pgmPath]() {
        QString f = QFileDialog::getOpenFileName(nullptr, "选择PGM文件", "", "PGM Files (*.pgm)");
        if (!f.isEmpty()) pgmPath->setText(f);
    });
    QObject::connect(yamlBtn, &QPushButton::clicked, [yamlPath]() {
        QString f = QFileDialog::getOpenFileName(nullptr, "选择YAML文件", "", "YAML Files (*.yaml)");
        if (!f.isEmpty()) yamlPath->setText(f);
    });
    QObject::connect(pbStreamBtn, &QPushButton::clicked, [pbStreamPath]() {
        QString f = QFileDialog::getOpenFileName(nullptr, "选择PBStream文件", "", "PBStream Files (*.pbstream)");
        if (!f.isEmpty()) pbStreamPath->setText(f);
    });
    QObject::connect(reflectorBinBtn, &QPushButton::clicked, [reflectorBinPath]() {
        QString f = QFileDialog::getOpenFileName(nullptr, "选择反光柱bin文件", "", "BIN Files (*.bin)");
        if (!f.isEmpty()) reflectorBinPath->setText(f);
    });
    QObject::connect(reflectorJsonBtn, &QPushButton::clicked, [reflectorJsonPath]() {
        QString f = QFileDialog::getOpenFileName(nullptr, "选择反光柱json文件", "", "JSON Files (*.json)");
        if (!f.isEmpty()) reflectorJsonPath->setText(f);
    });

    form->addRow("地图名称:", nameEdit);
    form->addRow("描述:", remarkEdit);
    form->addRow("PGM文件:", pgmPath);
    form->addRow("", pgmBtn);
    form->addRow("YAML文件:", yamlPath);
    form->addRow("", yamlBtn);
    form->addRow("PBStream文件:", pbStreamPath);
    form->addRow("", pbStreamBtn);
    form->addRow("反光柱bin文件:", reflectorBinPath);
    form->addRow("", reflectorBinBtn);
    form->addRow("反光柱json文件:", reflectorJsonPath);
    form->addRow("", reflectorJsonBtn);

    QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    btns->button(QDialogButtonBox::Ok)->setText("确定");
    btns->button(QDialogButtonBox::Cancel)->setText("取消");
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(btns);

    if (dlg.exec() != QDialog::Accepted) return;

    if (nameEdit->text().isEmpty()) {
        QMessageBox::warning(parent, "提示", "地图名称不能为空");
        return;
    }

    QJsonObject data;
    data["mapName"] = nameEdit->text();
    data["remark"] = remarkEdit->toPlainText();

    QPointer<QWidget> pGuard = parent;
    ApiService::instance().addAgvMap(data, [pGuard, onSuccess](int code, const QJsonObject &) {
        if (code == 200) {
            if (pGuard) QMessageBox::information(pGuard, "成功", "添加成功");
            if (onSuccess) onSuccess();
        }
    });
}

void showRenameDialog(QWidget *parent, const QJsonObject &mapData, std::function<void()> onSuccess)
{
    QDialog dlg(parent);
    dlg.setWindowTitle("修改地图信息");
    QFormLayout *form = new QFormLayout(&dlg);

    QLineEdit *nameEdit = new QLineEdit(mapData.value("mapName").toString());
    QTextEdit *remarkEdit = new QTextEdit(mapData.value("remark").toString());
    remarkEdit->setMaximumHeight(60);

    form->addRow("地图名称:", nameEdit);
    form->addRow("描述:", remarkEdit);

    QDialogButtonBox *btns2 = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    btns2->button(QDialogButtonBox::Ok)->setText("确定");
    btns2->button(QDialogButtonBox::Cancel)->setText("取消");
    QObject::connect(btns2, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns2, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(btns2);

    if (dlg.exec() != QDialog::Accepted) return;

    QJsonObject data;
    data["mapId"] = mapData.value("mapId");
    data["mapName"] = nameEdit->text();
    data["remark"] = remarkEdit->toPlainText();

    QPointer<QWidget> pGuard = parent;
    ApiService::instance().updateMap(data, [pGuard, onSuccess](int code, const QJsonObject &) {
        if (code == 200) {
            if (pGuard) QMessageBox::information(pGuard, "成功", "修改成功");
            if (onSuccess) onSuccess();
        }
    });
}

} // namespace MapWidgetDialogs
