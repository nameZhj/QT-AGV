/**
 * @file    MapEditHelpDialog.cpp
 * @layer   GUI
 * @brief   Dialog rendering keyboard shortcuts, mouse gestures, and map element color codes.
 * @deps    MapEditHelpDialog.h, QDialog, QVBoxLayout, QHBoxLayout, QLabel, QGroupBox, QPushButton
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapEditHelpDialog.h"
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QPushButton>

namespace MapEditHelpDialog {

void showHelp(QWidget *parent)
{
    QDialog dlg(parent);
    dlg.setWindowTitle("地图编辑与快捷操作指南");
    dlg.setMinimumWidth(560);
    dlg.setStyleSheet(
        "QDialog { background: #16202e; color: #d0e0f0; font-size: 13px; }"
        "QGroupBox { font-weight: bold; border: 1px solid rgba(255,255,255,0.12); border-radius: 6px; margin-top: 12px; padding: 12px; background: #1a2638; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; color: #5dade2; }"
        "QLabel { background: transparent; color: #b8d2e8; }"
        "QPushButton { background: #2471a3; color: white; border: none; border-radius: 4px; padding: 6px 18px; font-weight: bold; font-size: 12px; }"
        "QPushButton:hover { background: #2980b9; }");

    auto *layout = new QVBoxLayout(&dlg);
    layout->setSpacing(12);
    layout->setContentsMargins(18, 18, 18, 18);

    // 1. 快捷键
    auto *scBox = new QGroupBox("⌨️ 常用键盘快捷键", &dlg);
    auto *scLayout = new QVBoxLayout(scBox);
    scLayout->setSpacing(6);
    auto addRow = [&](const QString &key, const QString &desc) {
        auto *row = new QHBoxLayout;
        auto *kLbl = new QLabel(key);
        kLbl->setFixedWidth(130);
        kLbl->setStyleSheet("color: #f1c40f; font-family: Consolas, monospace; font-weight: bold;");
        auto *dLbl = new QLabel(desc);
        dLbl->setStyleSheet("color: #ecf0f1;");
        row->addWidget(kLbl);
        row->addWidget(dLbl, 1);
        scLayout->addLayout(row);
    };
    addRow("Esc", "指针模式：选择要素 / 拖拽移动站点与线路");
    addRow("M", "站点工具：点击放置站点 LM（蓝）");
    addRow("C", "站点工具-特殊工作站：点击放置充电桩点 CP_（金，typeName=ChargePoint）");
    addRow("T", "站点工具-工作站：点击放置高精度点 AP（红，typeName=ActionPoint）");
    addRow("P / L", "路径/线段工具：空地自动建站，连续连线");
    addRow("B", "三阶曲线工具：起点 ➔ 控制点 ➔ 终点自动吸附/建站");
    addRow("A", "区域工具：框选高级区域（紫色矩形框）");
    addRow("E", "橡皮擦：擦除栅格涂改，恢复可通行区域");
    addRow("Delete / Backspace", "删除当前选中的路网要素（站点、线段、曲线、高级区域）");
    addRow("Ctrl + Z", "撤销底图画笔 / 橡皮擦涂改");
    addRow("Ctrl + Y / Ctrl+Shift+Z", "恢复 (取消撤销) 底图涂改操作");
    addRow("Ctrl + S / Alt + S", "保存地图底图与路网结构");
    addRow("空格键 (Space)", "循环切换绘图工具");
    addRow("F1", "打开此地图编辑与快捷操作指南");
    layout->addWidget(scBox);

    // 2. 鼠标交互
    auto *mouseBox = new QGroupBox("🖱️ 鼠标交互与视图操作", &dlg);
    auto *mLayout = new QVBoxLayout(mouseBox);
    mLayout->setSpacing(6);
    auto addMouseRow = [&](const QString &act, const QString &desc) {
        auto *row = new QHBoxLayout;
        auto *aLbl = new QLabel(act);
        aLbl->setFixedWidth(130);
        aLbl->setStyleSheet("color: #3498db; font-weight: bold;");
        auto *dLbl = new QLabel(desc);
        dLbl->setStyleSheet("color: #ecf0f1;");
        row->addWidget(aLbl);
        row->addWidget(dLbl, 1);
        mLayout->addLayout(row);
    };
    addMouseRow("鼠标左键拖拽", "拖动站点（关联的所有线路与曲线实时跟随拉伸）/ 拖动手柄");
    addMouseRow("鼠标右键", "路径/曲线绘制中断连续连线；或返回指针模式");
    addMouseRow("中键 / Alt+左键", "平移拖拽地图画布视图");
    addMouseRow("鼠标滚轮", "以光标所在位置为中心缩放地图");
    layout->addWidget(mouseBox);

    // 3. 图元与配色说明
    auto *colorBox = new QGroupBox("📍 路网图元与配色规范", &dlg);
    auto *cLayout = new QVBoxLayout(colorBox);
    cLayout->setSpacing(6);
    auto addColorRow = [&](const QString &badge, const QString &col, const QString &desc) {
        auto *row = new QHBoxLayout;
        auto *bLbl = new QLabel(badge);
        bLbl->setFixedWidth(130);
        bLbl->setStyleSheet(QString("color: %1; font-weight: bold;").arg(col));
        auto *dLbl = new QLabel(desc);
        dLbl->setStyleSheet("color: #ecf0f1;");
        row->addWidget(bLbl);
        row->addWidget(dLbl, 1);
        cLayout->addLayout(row);
    };
    addColorRow("● 普通站点 (LM)", "#2980b9", "普通拓扑点位，用于路径规划中转与朝向控制");
    addColorRow("● 工作站 (AP)", "#c0392b", "高精定位停止站点，通常配置 EPC 二维码二次对齐");
    addColorRow("● 充电点 (CP)", "#f39c12", "充电机接驳工位，支持配置充电对接参数");
    addColorRow("━ 直线路段", "#27ae60", "双向或单向直行轨道，附带速度与限速配置");
    addColorRow("〜 贝塞尔曲线", "#e67e22", "平滑曲线过渡轨道，控制点手柄支持拖拽微调");
    addColorRow("▢ 高级区域", "#8e44ad", "特定限速/禁行/避障参数区域，支持 8 锚点缩放");
    addColorRow("◎ 反光柱", "#e74c3c", "激光反光板定位特征柱，仅作参考展示");
    layout->addWidget(colorBox);

    auto *btnBox = new QHBoxLayout;
    btnBox->addStretch();
    auto *closeBtn = new QPushButton("关闭 (Esc)");
    QObject::connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    btnBox->addWidget(closeBtn);
    layout->addLayout(btnBox);

    dlg.exec();
}

} // namespace MapEditHelpDialog
