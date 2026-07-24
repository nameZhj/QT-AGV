#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QLabel>
#include <QTimer>
#include <QJsonObject>

class AgvInfoWidget;
class CreateMapWidget;
class EditConfigWidget;
class MapViewWidget;
class MapEditWidget;
class TaskWidget;
class TaskEditWidget;
class TaskLogWidget;
class QToolBar;
class QAction;
class QActionGroup;
class QPushButton;

// Main window with dark-themed top-tab navigation matching the reference UI.
// Layout: top header bar → icon toolbar → (left tools | center content | right panel) → status bar
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onSectionChanged(int index);
    void onNavigateRequested(const QString &target, const QString &id);
    void onChangeDevice();
    void onAbout();
    void updateStatusBar();
    void onMqttStatusClicked();
    void onTeleopToggled();
    void onTeleopActiveChanged(bool active);

private:
    void setupGlobalStyle();
    void setupHeaderBar();
    void setupContentArea();
    void setupStatusBar();
    void initPages();
    void connectServices();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void showEvent(QShowEvent *event) override;

    void switchToPage(const QString &pageKey);
    void highlightTabForPageIndex(int pageIndex);
    void refreshCurrentPage(int index);

    // Section toolbar (top tabs)
    QToolBar *m_sectionBar     = nullptr;
    QActionGroup *m_secGroup   = nullptr;

    // Content
    QStackedWidget *m_stack    = nullptr;

    // Pages
    AgvInfoWidget    *m_agvInfoPage    = nullptr;
    CreateMapWidget  *m_createMapPage  = nullptr;
    EditConfigWidget *m_editConfigPage = nullptr;
    MapViewWidget    *m_mapPage        = nullptr;
    TaskWidget       *m_taskPage       = nullptr;
    TaskLogWidget    *m_taskLogPage    = nullptr;

    QMap<QString, QWidget *> m_dynamicPages;

    // Status bar labels
    QLabel *m_sbDevice  = nullptr;
    QLabel *m_sbCarId   = nullptr;
    QLabel *m_sbConn    = nullptr;
    QPushButton *m_teleopBtn = nullptr;   // 底部"开始遥控"按钮

    QTimer *m_statusTimer = nullptr;
};

#endif // MAINWINDOW_H
