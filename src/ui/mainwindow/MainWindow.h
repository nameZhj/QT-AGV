/**
 * @file    MainWindow.h
 * @layer   GUI
 * @brief   Top-level window managing multi-view tabs, navigation sidebar, and status bar.
 * @deps    QMainWindow, MapViewWidget, TaskChainWidget, CarWidget
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */
#pragma once

#include <QMainWindow>
#include <QString>

class QStackedWidget;
class QToolBar;
class QActionGroup;
class MapViewWidget;
class MainWindowStatusController;
class MainWindowPageManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT
    friend class MainWindowStatusController;
    friend class MainWindowPageManager;

public:
    // === [Section: Construction & Lifecycle] ===
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

    MapViewWidget *mapPage() const;
    void connectServices();

public slots:
    // === [Section: Interactive Slots] ===
    void onSectionChanged(int index);
    void onNavigateRequested(const QString &target, const QString &id);
    void onChangeDevice();
    void onAbout();
    void updateStatusBar();
    void onMqttStatusClicked();
    void onTeleopToggled();
    void onTeleopActiveChanged(bool active);

protected:
    // === [Section: Protected Overrides & Event Filtering] ===
    bool eventFilter(QObject *obj, QEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    // === [Section: UI Setup Helpers] ===
    void setupGlobalStyle();
    void setupHeaderBar();
    void setupContentArea();
    void setupStatusBar();
    void initPages();

    // === [Section: Member State & UI Coordinators] ===
    QToolBar                   *m_sectionBar      = nullptr;
    QActionGroup               *m_secGroup        = nullptr;
    QStackedWidget             *m_stack           = nullptr;

    MainWindowStatusController *m_statusController = nullptr;
    MainWindowPageManager      *m_pageManager      = nullptr;
};
