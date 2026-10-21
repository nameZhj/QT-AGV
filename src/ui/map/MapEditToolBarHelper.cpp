/**
 * @file    MapEditToolBarHelper.cpp
 * @layer   GUI
 * @brief   Implementation of map editor toolbar construction.
 * @deps    MapEditToolBarHelper.h, MapEditStyle.h, ToggleSwitch.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapEditToolBarHelper.h"
#include "MapEditStyle.h"
#include "ui/common/ToggleSwitch.h"
#include "MapEditTypes.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QMouseEvent>

using namespace MapEditStyle;
using namespace MapEdit;

namespace MapEditToolBarHelper {

TopBarWidgets setupTopBar(QWidget *parent, QVBoxLayout *root)
{
    TopBarWidgets w;
    w.container = new QWidget(parent);
    w.container->setFixedHeight(44);
    w.container->setStyleSheet(S_TOPBAR);
    QHBoxLayout *tl = new QHBoxLayout(w.container);
    tl->setContentsMargins(8, 4, 8, 4);
    tl->setSpacing(8);

    auto topBtn = [&](const QString &txt, const QString &bg = QString()) -> QPushButton* {
        auto *b = new QPushButton(txt, parent);
        QString base = bg.isEmpty() ? "transparent" : bg;
        b->setStyleSheet(QString(
            "QPushButton{color:white;border:none;border-radius:4px;"
            "padding:4px 12px;font-size:12px;background:%1;}"
            "QPushButton:hover{background:%2;}")
            .arg(base, bg.isEmpty() ? "rgba(255,255,255,18)" : bg));
        return b;
    };

    w.backBtn = topBtn("← 返回");
    tl->addWidget(w.backBtn);

    QFrame *div = new QFrame(parent);
    div->setFrameShape(QFrame::VLine);
    div->setStyleSheet("color:rgba(255,255,255,0.08);");
    tl->addWidget(div);

    w.mapInfoLabel = new QLabel("编辑地图", parent);
    w.mapInfoLabel->setStyleSheet("color:#9ab8d8;font-weight:bold;font-size:13px;");
    tl->addWidget(w.mapInfoLabel);
    tl->addStretch();

    w.undoBtn = topBtn("撤销");
    w.undoBtn->setIcon(makeToolIcon("undo"));
    w.undoBtn->setToolTip(ttHtml("撤销", "撤销上一步底图修图"));
    w.undoBtn->setEnabled(false);
    tl->addWidget(w.undoBtn);

    w.redoBtn = topBtn("恢复");
    w.redoBtn->setIcon(makeToolIcon("redo"));
    w.redoBtn->setToolTip(ttHtml("恢复", "恢复 (取消撤销)"));
    w.redoBtn->setEnabled(false);
    tl->addWidget(w.redoBtn);

    QLabel *brushSizeLbl = new QLabel("笔刷:", parent);
    brushSizeLbl->setStyleSheet("color:#9ab8d8;font-size:12px;margin-left:4px;");
    tl->addWidget(brushSizeLbl);

    w.brushSizeCombo = new QComboBox(parent);
    for (int px : {2, 4, 8, 16, 32}) w.brushSizeCombo->addItem(QString("%1 px").arg(px), px);
    w.brushSizeCombo->setCurrentIndex(1);
    w.brushSizeCombo->setStyleSheet(
        "QComboBox{background:#1e2d40; color:#c8ddef; border:1px solid rgba(255,255,255,0.12); border-radius:4px; padding:2px 6px; font-size:12px;}"
        "QComboBox::drop-down{border:none; width:16px;}"
        "QComboBox QAbstractItemView{background:#1a2332; color:#c8ddef; selection-background-color:#2980b9;}");
    tl->addWidget(w.brushSizeCombo);

    auto makeToolBtn = [&](const QString &icon, const QString &title, const QString &tip, bool checkable = false) {
        auto *b = new QPushButton(parent);
        b->setIcon(makeToolIcon(icon)); b->setIconSize(QSize(18, 18));
        b->setCheckable(checkable); b->setFixedSize(30, 30);
        b->setToolTip(ttHtml(title, tip)); b->setCursor(Qt::PointingHandCursor);
        b->installEventFilter(new InstantTip(b));
        b->setStyleSheet("QPushButton{background:transparent;border:none;border-radius:5px;padding:0;}"
                         "QPushButton:hover{background:rgba(255,255,255,12);}"
                         "QPushButton:checked{background:rgba(41,128,185,160);}");
        return b;
    };
    w.resetViewBtn = makeToolBtn("reset", "重置视图", "恢复地图到初始缩放与角度");
    tl->addWidget(w.resetViewBtn);
    w.gridBtn = makeToolBtn("grid", "网格", "切换网格显示", true);
    tl->addWidget(w.gridBtn);

    auto makeToggle = [&](const QString &title, const QString &tip) {
        auto *lbl = new QLabel(title, parent);
        lbl->setCursor(Qt::PointingHandCursor);
        lbl->setToolTip(ttHtml(title, tip));
        tl->addWidget(lbl);
        auto *btn = new ToggleSwitch(parent);
        btn->setToolTip(ttHtml(title, tip));
        auto updateUI = [lbl](bool on) {
            lbl->setStyleSheet(on ? "color:#00e5ff;font-size:12px;font-weight:bold;cursor:pointer;"
                                  : "color:#9ab8d8;font-size:12px;font-weight:normal;cursor:pointer;");
        };
        QObject::connect(btn, &ToggleSwitch::toggled, parent, updateUI);
        updateUI(btn->isChecked());
        struct ClickFilter : public QObject {
            QAbstractButton *target;
            ClickFilter(QAbstractButton *t, QObject *p) : QObject(p), target(t) {}
            bool eventFilter(QObject *o, QEvent *e) override {
                if (e->type() == QEvent::MouseButtonPress && static_cast<QMouseEvent*>(e)->button() == Qt::LeftButton && target) {
                    target->toggle(); return true;
                }
                return QObject::eventFilter(o, e);
            }
        };
        lbl->installEventFilter(new ClickFilter(btn, lbl));
        tl->addWidget(btn);
        return btn;
    };

    w.robotBtn = makeToggle("设备位置", "切换实时设备位置显示");
    w.reflectorBtn = makeToggle("反光柱", "切换反光柱显示");

    w.editMapBtn = topBtn("地图属性");
    tl->addWidget(w.editMapBtn);

    w.importSmapBtn = topBtn("导入 .smap路网", "#2e7d32");
    w.importSmapBtn->setToolTip(ttHtml("导入 .smap", "选择并导入 .smap 地图路网文件"));
    w.importSmapBtn->installEventFilter(new InstantTip(w.importSmapBtn));
    tl->addWidget(w.importSmapBtn);

    w.saveBtn = topBtn("保存地图", "#2471a3");
    w.saveBtn->setToolTip(ttHtml("保存地图 (Ctrl+S)", "同时保存路网数据与修改后的栅格地图底图"));
    tl->addWidget(w.saveBtn);

    w.teleopBtn = topBtn("抢占控制权", "#1e6fd4");
    w.teleopBtn->setIcon(makeToolIcon("teleop"));
    w.teleopBtn->setToolTip(ttHtml("抢占控制权", "点击抢占控制权，再次点击恢复导航\n键盘: W A S D 移动 | Space 刹车 | Ctrl 加速 | P 升降上 | L 升降下"));
    w.teleopBtn->installEventFilter(new InstantTip(w.teleopBtn));
    tl->addWidget(w.teleopBtn);

    w.helpBtn = topBtn("帮助 (F1)", "#34495e");
    w.helpBtn->setToolTip(ttHtml("帮助 (F1)", "查看键盘快捷键与地图编辑操作指南"));
    w.helpBtn->installEventFilter(new InstantTip(w.helpBtn));
    tl->addWidget(w.helpBtn);

    root->addWidget(w.container);
    return w;
}

SubToolBarWidgets setupSubToolBars(QWidget *parent, QVBoxLayout *root)
{
    SubToolBarWidgets w;

    // 站点类型子工具条
    w.stationBar = new QWidget(parent);
    w.stationBar->setStyleSheet("background:#1e2d3d; border-bottom:1px solid rgba(255,255,255,0.06);");
    {
        QHBoxLayout *sbl = new QHBoxLayout(w.stationBar);
        sbl->setContentsMargins(8, 3, 8, 3);
        sbl->setSpacing(6);
        QLabel *sbTitle = new QLabel("站点", parent);
        sbTitle->setStyleSheet("color:#9ab8d8; font-size:12px; font-weight:bold; background:transparent; border:none;");
        sbl->addWidget(sbTitle);

        auto mkBtn = [&](const QString &text, int mode) {
            auto *b = new QPushButton(text, parent);
            b->setCheckable(true);
            b->setFocusPolicy(Qt::NoFocus);
            b->setCursor(Qt::PointingHandCursor);
            b->setFixedHeight(24);
            b->setStyleSheet(
                "QPushButton{background:transparent; color:#9ab8d8; border:1px solid transparent;"
                "border-radius:4px; padding:2px 12px; font-size:12px;}"
                "QPushButton:hover{background:rgba(255,255,255,14);}"
                "QPushButton:checked{background:#2b4a6f; color:#ffffff; border:1px solid #3d6a9a;}");
            sbl->addWidget(b);
            w.stationBtns[mode] = b;
        };
        mkBtn("站点", Point);
        mkBtn("工作站", HighPrecision);
        mkBtn("特殊工作站", Charge);
        sbl->addStretch();
    }
    w.stationBar->setVisible(false);
    root->addWidget(w.stationBar);

    // 高级区域子工具条
    w.areaBar = new QWidget(parent);
    w.areaBar->setStyleSheet("background:#1e2d3d; border-bottom:1px solid rgba(255,255,255,0.06);");
    {
        QHBoxLayout *abl = new QHBoxLayout(w.areaBar);
        abl->setContentsMargins(8, 3, 8, 3);
        abl->setSpacing(6);
        QLabel *abTitle = new QLabel("高级区域", parent);
        abTitle->setStyleSheet("color:#c084fc; font-size:12px; font-weight:bold; background:transparent; border:none;");
        abl->addWidget(abTitle);

        w.areaAdvBtn = new QPushButton("高级区域", parent);
        w.areaAdvBtn->setCheckable(true);
        w.areaAdvBtn->setFocusPolicy(Qt::NoFocus);
        w.areaAdvBtn->setCursor(Qt::PointingHandCursor);
        w.areaAdvBtn->setFixedHeight(24);
        w.areaAdvBtn->setStyleSheet(
            "QPushButton{background:#3b1d5c; color:#c084fc; border:1px solid #7c3aed;"
            "border-radius:4px; padding:2px 14px; font-size:12px; font-weight:bold;}"
            "QPushButton:hover{background:#4c1d95; color:#ffffff;}");
        abl->addWidget(w.areaAdvBtn);
        abl->addStretch();
    }
    w.areaBar->setVisible(false);
    root->addWidget(w.areaBar);

    return w;
}

LeftToolBarWidgets setupLeftToolbar(QWidget *parent, QHBoxLayout *contentLayout)
{
    LeftToolBarWidgets w;
    w.container = new QWidget(parent);
    w.container->setFixedWidth(46);
    w.container->setStyleSheet(S_TOOLBAR);
    QVBoxLayout *tbLayout = new QVBoxLayout(w.container);
    tbLayout->setContentsMargins(5, 8, 5, 8);
    tbLayout->setSpacing(3);

    struct Tool { int mode; QString icon; QString tip; QString detail; };
    const QList<Tool> tools = {
        {Drag, "pan", "指针 (Esc)", "选择要素 / 拖动站点与线路 / 调整曲线控制手柄"},
        {-1, "", "", ""},
        {Line, "line", "线路 (L)", "绘制直线路径：空地自动建站，连续连线，右键/Esc结束"},
        {Curve, "curve", "曲线 (B)", "绘制多阶贝塞尔曲线：起点→控制点→终点，自动吸附/建站"},
        {-1, "", "", ""},
        {Point, "point", "站点 (M)", "放置站点：站点 LM（蓝）/ 工作站 AP（红）/ 特殊工作站 CP_（金）\n在顶部“站点”子工具条中切换类型"},
        {-1, "", "", ""},
        {Area, "region", "区域 (A)", "框选高级区域：鼠标在地图中拖动框选一个范围，设置运动控制与避障参数"},
        {-1, "", "", ""},
        {Eraser, "eraser", "橡皮擦 (E)", "擦除栅格涂改：恢复可通行区域"},
        {Brush, "brush", "画笔 (P)", "涂黑栅格地图：标记障碍物/墙体"},
        {-1, "", "", ""},
        {-2, "devicepoint", "设备打点", "添加设备当前位置为普通站点"},
        {-3, "charge", "充电打点", "通过设备相对定位添加充电桩站点 CP\n(等待设备返回 epc 坐标)"},
        {-4, "highprecision", "精定位打点", "通过设备相对定位添加高精定位站点 TL\n(等待设备返回 epc 坐标)"},
    };

    for (const auto &t : tools) {
        if (t.mode == -1) {
            auto *sep = new QFrame(parent);
            sep->setFrameShape(QFrame::HLine);
            sep->setFixedHeight(1);
            sep->setStyleSheet("background:#262f42; border:none; margin:3px 2px;");
            tbLayout->addWidget(sep);
            continue;
        }
        auto *b = new QPushButton(parent);
        b->setIcon(makeToolIcon(t.icon));
        b->setIconSize(QSize(20, 20));
        b->setCheckable(t.mode >= 0);
        b->setFixedSize(36, 36);
        b->setStyleSheet(S_TOOLBTN);
        b->setToolTip(ttHtml(t.tip, t.detail));
        b->setCursor(Qt::PointingHandCursor);
        b->installEventFilter(new InstantTip(b));
        tbLayout->addWidget(b, 0, Qt::AlignHCenter);
        if (t.mode >= 0) w.toolBtns[t.mode] = b;
        else w.toolBtns[t.mode] = b;
    }

    tbLayout->addStretch();
    auto *sep2 = new QFrame(parent);
    sep2->setFrameShape(QFrame::HLine);
    sep2->setFixedHeight(1);
    sep2->setStyleSheet("background:#262f42; border:none; margin:3px 2px;");
    tbLayout->addWidget(sep2);

    w.tableModeBtn = new QPushButton(parent);
    w.tableModeBtn->setIcon(makeToolIcon("list"));
    w.tableModeBtn->setIconSize(QSize(20, 20));
    w.tableModeBtn->setCheckable(true);
    w.tableModeBtn->setFixedSize(36, 36);
    w.tableModeBtn->setStyleSheet(S_TOOLBTN);
    w.tableModeBtn->setToolTip(ttHtml("要素列表", "显示/隐藏路网要素列表"));
    w.tableModeBtn->setCursor(Qt::PointingHandCursor);
    w.tableModeBtn->installEventFilter(new InstantTip(w.tableModeBtn));
    tbLayout->addWidget(w.tableModeBtn, 0, Qt::AlignHCenter);

    w.toolBtns[Drag]->setChecked(true);
    contentLayout->addWidget(w.container);
    return w;
}

} // namespace MapEditToolBarHelper
