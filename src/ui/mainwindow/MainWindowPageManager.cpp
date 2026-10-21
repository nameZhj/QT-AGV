/**
 * @file    MainWindowPageManager.cpp
 * @layer   GUI
 * @brief   Implementation of page stack management, routing, and dynamic map edit pages.
 * @deps    MainWindowPageManager.h, MainWindow.h, MapViewWidget.h, CreateMapWidget.h, EditConfigWidget.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */
#include "MainWindowPageManager.h"
#include "MainWindow.h"
#include "ui/map/CreateMapWidget.h"
#include "ui/control/EditConfigWidget.h"
#include "ui/map/MapViewWidget.h"
#include "ui/map/MapEditWidget.h"
#include "ui/task/TaskLogWidget.h"
#include "ui/task/TaskChainWidget.h"
#include "core/TaskChainManager.h"
#include <QStackedWidget>
#include <QActionGroup>
#include <QAction>

MainWindowPageManager::MainWindowPageManager(MainWindow *window, QStackedWidget *stack, QActionGroup *secGroup)
    : QObject(window)
    , m_window(window)
    , m_stack(stack)
    , m_secGroup(secGroup)
{
}

void MainWindowPageManager::initPages()
{
    if (!m_stack) return;

    TaskChainManager::instance().init();

    m_createMapPage  = new CreateMapWidget;
    m_editConfigPage = new EditConfigWidget;
    m_mapPage        = new MapViewWidget;
    m_taskChainPage  = new TaskChainWidget;
    m_taskLogPage    = new TaskLogWidget;

    m_stack->addWidget(m_mapPage);         // 0 地图与控制
    m_stack->addWidget(m_createMapPage);   // 1 创建地图
    m_stack->addWidget(m_taskChainPage);   // 2 任务链
    m_stack->addWidget(m_taskLogPage);     // 3 任务日志
    m_stack->addWidget(m_editConfigPage);  // 4 参数配置

    connect(m_taskLogPage, &TaskLogWidget::navigateRequested,
            this, &MainWindowPageManager::onNavigateRequested);
    connect(m_mapPage, &MapViewWidget::navigateRequested,
            this, &MainWindowPageManager::onNavigateRequested);
    if (m_window) {
        connect(m_mapPage, &MapViewWidget::mapStatusChanged,
                m_window, &MainWindow::updateStatusBar);
    }
    connect(m_createMapPage, &CreateMapWidget::goBack, this, [this]() {
        m_stack->setCurrentWidget(m_mapPage);
        m_mapPage->forceRefreshAfterMapping();
        highlightTabForPageIndex(0);
    });
    connect(m_editConfigPage, &EditConfigWidget::routeNetworkUpdated,
            m_mapPage, &MapViewWidget::refreshData);

    connect(m_stack, &QStackedWidget::currentChanged, this, &MainWindowPageManager::refreshCurrentPage);

    m_stack->setCurrentIndex(0);
}

bool MainWindowPageManager::canLeaveCurrentWidget(QWidget *cur)
{
    if (!cur) return true;

    auto *mapEdit = qobject_cast<MapEditWidget*>(cur);
    if (mapEdit && !mapEdit->checkUnsavedChanges()) {
        return false;
    }

    auto *createMap = qobject_cast<CreateMapWidget*>(cur);
    if (createMap && !createMap->tryExitMapping()) {
        return false;
    }

    return true;
}

void MainWindowPageManager::revertSectionTabSelection()
{
    if (!m_stack || !m_secGroup) return;
    int prevIdx = m_stack->currentIndex();
    if (prevIdx == 1) prevIdx = 0;   // 创建地图 → 地图管理
    for (QAction *act : m_secGroup->actions()) {
        if (act->data().toInt() == prevIdx) {
            m_secGroup->blockSignals(true);
            act->setChecked(true);
            m_secGroup->blockSignals(false);
            break;
        }
    }
}

void MainWindowPageManager::refreshCurrentPage(int index)
{
    if (!m_stack) return;
    QWidget *w = m_stack->widget(index);
    if (w == m_mapPage && m_mapPage) m_mapPage->refreshData();
    else if (w == m_taskLogPage && m_taskLogPage) m_taskLogPage->refreshData();
    else if (w == m_editConfigPage && m_editConfigPage) m_editConfigPage->refreshData();
}

void MainWindowPageManager::switchToPage(const QString &pageKey)
{
    if (!m_stack) return;
    if (pageKey == "edit_config") {
        m_stack->setCurrentWidget(m_mapPage);
        highlightTabForPageIndex(0);
        if (m_mapPage) {
            m_mapPage->onShowConfigDialog();
        }
        return;
    }

    static const QMap<QString, int> pageMap = {
        {"map",         0},
        {"create_map",  1},
        {"taskchain",   2},
        {"tasklog",     3},
    };
    auto it = pageMap.find(pageKey);
    if (it != pageMap.end()) {
        m_stack->setCurrentIndex(it.value());
        highlightTabForPageIndex(it.value());
    }
}

void MainWindowPageManager::highlightTabForPageIndex(int pageIndex)
{
    if (!m_secGroup) return;
    const QList<QAction *> actions = m_secGroup->actions();
    for (QAction *act : actions) {
        if (act->data().toInt() == pageIndex) {
            act->setChecked(true);
            return;
        }
    }
}

void MainWindowPageManager::openDynamicMapEditPage(const QString &id)
{
    if (!m_stack) return;
    const QString key = QString("map_edit_%1").arg(id);
    if (!m_dynamicPages.contains(key)) {
        auto *w = new MapEditWidget(id);
        connect(w, &MapEditWidget::goBack, this, [this, key]() {
            m_stack->setCurrentWidget(m_mapPage);
            if (m_mapPage) m_mapPage->refreshData();
            QWidget *p = m_dynamicPages.take(key);
            if (p) {
                m_stack->removeWidget(p);
                p->deleteLater();
            }
        });
        m_dynamicPages[key] = w;
        m_stack->addWidget(w);
    }
    m_stack->setCurrentWidget(m_dynamicPages[key]);
}

void MainWindowPageManager::onSectionChanged(int index)
{
    if (!m_stack) return;
    QWidget *cur = m_stack->currentWidget();
    if (!canLeaveCurrentWidget(cur)) {
        revertSectionTabSelection();
        return;
    }

    if (m_stack->currentIndex() == index) {
        refreshCurrentPage(index);
    } else {
        m_stack->setCurrentIndex(index);
    }
}

void MainWindowPageManager::onNavigateRequested(const QString &target, const QString &id)
{
    if (!m_stack) return;
    if (target == "map_show") {
        m_stack->setCurrentWidget(m_mapPage);
        highlightTabForPageIndex(0);
        return;
    }
    if (target == "create_map") {
        m_stack->setCurrentWidget(m_createMapPage);
        return;
    }
    if (target == "map_edit" || target == "map_image_edit") {
        openDynamicMapEditPage(id);
        return;
    }
}

void MainWindowPageManager::clearDynamicPages()
{
    if (!m_stack) return;
    for (QWidget *w : m_dynamicPages.values()) {
        m_stack->removeWidget(w);
        w->deleteLater();
    }
    m_dynamicPages.clear();
}
