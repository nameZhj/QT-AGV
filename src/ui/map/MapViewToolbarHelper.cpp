/**
 * @file    MapViewToolbarHelper.cpp
 * @layer   GUI
 * @brief   Toolbar setup, layout, styling, and navigation button state synchronization.
 * @deps    MapViewToolbarHelper.h, MapViewIcons.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapViewToolbarHelper.h"
#include "MapViewIcons.h"
#include "MapViewStyle.h"
#include <QHBoxLayout>
#include <QFrame>

const QString MapViewToolbarHelper::kToolBtnStyle   = MapViewStyle::kToolBtnStyle;
const QString MapViewToolbarHelper::kToolBtnPrimary = MapViewStyle::kToolBtnPrimary;
const QString MapViewToolbarHelper::kToolBtnDanger  = MapViewStyle::kToolBtnDanger;

MapViewToolbarButtons MapViewToolbarHelper::createToolbar(QWidget *parent, QObject *receiver)
{
    MapViewToolbarButtons btns;
    auto *lay = new QHBoxLayout(parent);
    lay->setContentsMargins(8, 0, 8, 0);
    lay->setSpacing(4);

    auto addBtn = [&](const QString &iconName, const QString &text, const char *slot) {
        auto *b = new QToolButton(parent);
        b->setText(text);
        b->setIcon(MapViewIcons::makeMapToolIcon(iconName));
        b->setIconSize(QSize(20, 20));
        b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        b->setAutoRaise(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(kToolBtnStyle);
        if (slot && receiver) {
            QObject::connect(b, SIGNAL(clicked()), receiver, slot);
        }
        lay->addWidget(b);
        return b;
    };

    btns.editMapBtn = addBtn("edit", "启用编辑", SLOT(onEditMap()));
    btns.editMapBtn->setToolTip("进入地图编辑与拓扑标注模式");

    btns.uploadSmapBtn = addBtn("smap-upload", "加载地图", SLOT(onUploadSmap()));
    btns.uploadSmapBtn->setToolTip("选择本地 .smap 地图文件加载至当前视图");

    btns.createMapBtn = addBtn("add", "2D建图", SLOT(onCreateMap()));
    btns.createMapBtn->setToolTip("开启 2D SLAM 激光扫描建图模式");

    btns.relocBtn = addBtn("target", "重定位", SLOT(onRelocalize()));
    btns.relocBtn->setCheckable(true);
    btns.relocBtn->setToolTip("单次点击激活：在地图上点击并拖拽设定初始位姿");

    btns.navModeBtn = addBtn("route", "路径导航", SLOT(onToggleNavMode()));
    btns.navModeBtn->setCheckable(true);
    btns.navModeBtn->setToolTip("路径导航模式：点击地图上的站点发起导航任务");

    btns.moveBtn = addBtn("move", "移动", SLOT(onToggleMoveMode()));
    btns.moveBtn->setCheckable(false);
    btns.moveBtn->setToolTip("平动与转动控制弹窗");

    btns.cancelNavBtn = addBtn("cancel", "取消导航", SLOT(onCancelNavigation()));
    btns.cancelNavBtn->setEnabled(false);
    btns.cancelNavBtn->setToolTip("取消当前执行中的导航或移动任务");

    btns.pauseNavBtn = addBtn("pause", "暂停导航", SLOT(onPauseNavigation()));
    btns.pauseNavBtn->setEnabled(false);
    btns.pauseNavBtn->setToolTip("暂停或恢复当前执行中的导航任务");

    auto *sepNav = new QFrame(parent);
    sepNav->setFrameShape(QFrame::VLine);
    sepNav->setStyleSheet("color:#2d3340;");
    lay->addWidget(sepNav);

    btns.uploadMapBtn = addBtn("map-upload", "推送地图", SLOT(onUploadMap()));
    btns.uploadMapBtn->setToolTip("将当前地图推送到设备/服务端");

    btns.downloadMapBtn = addBtn("file-download", "拉取地图", SLOT(onDownloadSource()));
    btns.downloadMapBtn->setToolTip("打开当前地图的本地缓存文件夹");

    btns.switchMapBtn = addBtn("swap", "地图管理", SLOT(onSwitchMap()));
    btns.switchMapBtn->setStyleSheet(kToolBtnPrimary);
    btns.switchMapBtn->setToolTip("地图管理：切换、重命名或删除地图");

    btns.teleopBtn = addBtn("teleop", "抢占控制权", SLOT(onTeleopClicked()));
    btns.teleopBtn->setToolTip(
        "点击抢占控制权（暂停导航并接管键盘控制），再次点击恢复导航\n"
        "键盘: W A S D 移动 | Space 刹车 | Ctrl 加速 | P 升降上 | L 升降下");

    btns.configBtn = addBtn("config", "参数", SLOT(onShowConfigDialog()));
    btns.configBtn->setToolTip("参数配置：查看与修改机器人各项配置参数");

    btns.statusBtn = addBtn("status", "状态", SLOT(onShowRobotStatus()));
    btns.statusBtn->setToolTip("查看机器人运行状态与详细诊断信息");

    btns.moreBtn = addBtn("more", "其它", SLOT(onShowMoreMenu()));
    btns.moreBtn->setToolTip("更多工具");
    btns.moreMenu = new QMenu(btns.moreBtn);
    btns.moreMenu->setStyleSheet(MapViewStyle::kMoreMenuStyle);
    QAction *audioManagerAct = btns.moreMenu->addAction("音频管理器");
    if (receiver) {
        QObject::connect(audioManagerAct, SIGNAL(triggered()), receiver, SLOT(onShowAudioManager()));
    }

    auto *sepEnd = new QFrame(parent);
    sepEnd->setFrameShape(QFrame::VLine);
    sepEnd->setStyleSheet("color:#2d3340;");
    lay->addWidget(sepEnd);

    btns.resetViewBtn = addBtn("reset", "重置视图", SLOT(onResetView()));
    btns.resetViewBtn->setToolTip("复位视图缩放与中心位置");

    btns.refreshBtn = addBtn("refresh", "刷新", SLOT(onRefreshMap()));
    btns.refreshBtn->setToolTip("重新刷新当前地图与路网数据");

    lay->addStretch();
    return btns;
}

void MapViewToolbarHelper::updateNavButtonsState(MapViewToolbarButtons &btns,
                                                 bool isNav, bool isPaused,
                                                 bool isUserPreempted, bool navModeActive)
{
    const bool canControlNav = isNav && !isUserPreempted;
    if (btns.cancelNavBtn) btns.cancelNavBtn->setEnabled(canControlNav);
    if (btns.pauseNavBtn) {
        btns.pauseNavBtn->setEnabled(canControlNav);
        if (isPaused) {
            btns.pauseNavBtn->setText("继续导航");
            btns.pauseNavBtn->setIcon(MapViewIcons::makeMapToolIcon("resume"));
        } else {
            btns.pauseNavBtn->setText("暂停导航");
            btns.pauseNavBtn->setIcon(MapViewIcons::makeMapToolIcon("pause"));
        }
    }
    if (btns.navModeBtn) {
        if (isNav) {
            btns.navModeBtn->setEnabled(false);
            btns.navModeBtn->setChecked(false);
            btns.navModeBtn->setStyleSheet(kToolBtnStyle);
        } else {
            btns.navModeBtn->setEnabled(true);
            btns.navModeBtn->setChecked(navModeActive);
            btns.navModeBtn->setStyleSheet(navModeActive ? kToolBtnPrimary : kToolBtnStyle);
        }
    }
}
