/**
 * @file    MainWindowPageManager.h
 * @layer   GUI
 * @brief   Manages QStackedWidget page initialization, navigation routing, and dynamic map edit pages.
 * @deps    QObject, QString, QMap
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */
#pragma once

#include <QObject>
#include <QString>
#include <QMap>

class MainWindow;
class QStackedWidget;
class QActionGroup;
class QWidget;
class CreateMapWidget;
class EditConfigWidget;
class MapViewWidget;
class TaskLogWidget;
class TaskChainWidget;

class MainWindowPageManager : public QObject
{
    Q_OBJECT

public:
    explicit MainWindowPageManager(MainWindow *window, QStackedWidget *stack, QActionGroup *secGroup);
    ~MainWindowPageManager() override = default;

    void initPages();
    void onSectionChanged(int index);
    void onNavigateRequested(const QString &target, const QString &id);
    void switchToPage(const QString &pageKey);
    void highlightTabForPageIndex(int pageIndex);
    void refreshCurrentPage(int index);
    void clearDynamicPages();

    MapViewWidget *mapPage() const { return m_mapPage; }

private:
    bool canLeaveCurrentWidget(QWidget *cur);
    void revertSectionTabSelection();
    void openDynamicMapEditPage(const QString &id);

    MainWindow       *m_window         = nullptr;
    QStackedWidget   *m_stack          = nullptr;
    QActionGroup     *m_secGroup       = nullptr;

    CreateMapWidget  *m_createMapPage  = nullptr;
    EditConfigWidget *m_editConfigPage = nullptr;
    MapViewWidget    *m_mapPage        = nullptr;
    TaskLogWidget    *m_taskLogPage    = nullptr;
    TaskChainWidget  *m_taskChainPage  = nullptr;

    QMap<QString, QWidget *> m_dynamicPages;
};
