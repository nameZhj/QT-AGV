/**
 * @file    MainWindow.cpp
 * @layer   GUI
 * @brief   Implementation of top-level application window and navigation shell.
 * @deps    MainWindow.h, MainWindowStatusController.h, MainWindowPageManager.h, AppConfig.h, StyleLoader.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */
#include "MainWindow.h"
#include "MainWindowStatusController.h"
#include "MainWindowPageManager.h"
#include "MainWindowStyle.h"
#include "app/AppConfig.h"
#include "app/AppConstants.h"
#include "network/MqttService.h"
#include "ui/dialogs/DeviceManagerDialog.h"
#include "ui/common/Toast.h"
#include "ui/common/StyleLoader.h"
#include "ui/map/MapViewWidget.h"
#include "core/TeleopController.h"
#include "core/RobotInfoManager.h"
#include <QToolBar>
#include <QAction>
#include <QActionGroup>
#include <QStackedWidget>
#include <QMenuBar>
#include <QMenu>
#include <QMessageBox>
#include <QApplication>
#include <QPalette>
#include <QIcon>
#include <QEvent>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupGlobalStyle();
    setupHeaderBar();
    setupContentArea();

    m_pageManager = new MainWindowPageManager(this, m_stack, m_secGroup);
    m_statusController = new MainWindowStatusController(this);

    connectServices();
    initPages();
    setupStatusBar();
}

MapViewWidget *MainWindow::mapPage() const
{
    return m_pageManager ? m_pageManager->mapPage() : nullptr;
}

void MainWindow::setupGlobalStyle()
{
    StyleLoader::applyApplicationStyle();
    setWindowTitle(AppConstants::AppTitle);
    resize(AppConstants::Window::DefaultWidth, AppConstants::Window::DefaultHeight);
    setWindowIcon(QIcon(AppConstants::Resources::AppIconSvg));

    QPalette darkPalette;
    MainWindowStyle::applyDarkPalette(darkPalette);
    qApp->setPalette(darkPalette);
}

void MainWindow::setupHeaderBar()
{
    m_sectionBar = new QToolBar(this);
    m_sectionBar->setObjectName("sectionBar");
    m_sectionBar->setMovable(false);
    m_sectionBar->setFloatable(false);
    m_sectionBar->setFixedHeight(42);
    m_sectionBar->setToolButtonStyle(Qt::ToolButtonTextOnly);

    m_secGroup = new QActionGroup(this);
    m_secGroup->setExclusive(true);

    struct SectionDef { QString text; int pageIndex; };
    const QList<SectionDef> sections = {
        {"  地图与控制", 0},
        {"  任务链  ", 2},
        {"  任务日志  ", 3},
    };

    for (const auto &sec : sections) {
        QAction *act = m_sectionBar->addAction(sec.text);
        act->setCheckable(true);
        act->setData(sec.pageIndex);
        m_secGroup->addAction(act);
        connect(act, &QAction::triggered, this, [this, act]() {
            onSectionChanged(act->data().toInt());
        });
    }

    addToolBar(Qt::TopToolBarArea, m_sectionBar);

    QMenu *systemMenu = menuBar()->addMenu("系统");
    systemMenu->addAction("切换设备", this, &MainWindow::onChangeDevice);
    systemMenu->addSeparator();
    systemMenu->addAction("退出", qApp, &QApplication::quit);

    QMenu *helpMenu = menuBar()->addMenu("帮助");
    helpMenu->addAction("关于", this, &MainWindow::onAbout);
}

void MainWindow::setupContentArea()
{
    m_stack = new QStackedWidget(this);
    setCentralWidget(m_stack);
}

void MainWindow::initPages()
{
    if (m_pageManager) {
        m_pageManager->initPages();
        m_pageManager->highlightTabForPageIndex(0);
    }
}

void MainWindow::setupStatusBar()
{
    if (m_statusController) {
        m_statusController->setupStatusBar();
    }
}

void MainWindow::connectServices()
{
    // 单车独占：仅当已由用户选定有效已连接设备时发起连接；未连接设备时绝不自动发起连接
    int curIdx = AppConfig::instance().currentDeviceIndex();
    if (curIdx < 0) {
        MqttService::instance().clearAllSubscriptions();
        MqttService::instance().disconnectFromHost();
        return;
    }

    QString curCarId = AppConfig::instance().agvId().trimmed();
    if (curCarId.isEmpty()) curCarId = AppConfig::instance().currentDevice().carId.trimmed();
    RobotInfoManager::instance().resetForCar(curCarId);
    RobotInfoManager::instance().init();

    const QString ip = AppConfig::instance().serverIp();
    if (ip.isEmpty()) return;

    MqttService::instance().connectToHost(ip, 1883);
}

void MainWindow::onSectionChanged(int index)
{
    if (m_pageManager) {
        m_pageManager->onSectionChanged(index);
    }
}

void MainWindow::onNavigateRequested(const QString &target, const QString &id)
{
    if (m_pageManager) {
        m_pageManager->onNavigateRequested(target, id);
    }
}

void MainWindow::onChangeDevice()
{
    DeviceManagerDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;

    connectServices();
    if (m_statusController) {
        m_statusController->updateDeviceProfile();
    }
    if (m_pageManager) {
        m_pageManager->clearDynamicPages();
        m_pageManager->highlightTabForPageIndex(0);
        if (m_stack && m_stack->currentIndex() == 0) {
            if (m_pageManager->mapPage()) m_pageManager->mapPage()->refreshData();
        } else if (m_stack) {
            m_stack->setCurrentIndex(0);
        }
    }
}

void MainWindow::onAbout()
{
    QMessageBox::about(this, "关于",
        "<b>RT-AGV 移动机器人管理平台</b><br>"
        "版本: v1.0<br><br>"
        "基于 Qt6 开发的 AGV 设备管理系统");
}

void MainWindow::updateStatusBar()
{
    if (m_statusController) {
        m_statusController->updateStatusBar();
    }
}

void MainWindow::onMqttStatusClicked()
{
    if (m_statusController) {
        m_statusController->onMqttStatusClicked();
    }
}

void MainWindow::onTeleopToggled()
{
    if (!MqttService::instance().isConnected()) {
        Toast::error(this, "MQTT 未连接，无法启用遥控");
        return;
    }
    if (AppConfig::instance().agvId().isEmpty()) {
        Toast::error(this, "未选择设备，无法启用遥控");
        return;
    }
    TeleopController::instance().setActive(!TeleopController::instance().isActive());
}

void MainWindow::onTeleopActiveChanged(bool active)
{
    if (m_statusController) {
        m_statusController->onTeleopActiveChanged(active);
    }
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (m_statusController && m_statusController->handleEventFilter(obj, event)) {
        return true;
    }
    return QMainWindow::eventFilter(obj, event);
}
