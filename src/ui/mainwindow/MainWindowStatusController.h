/**
 * @file    MainWindowStatusController.h
 * @layer   GUI
 * @brief   Manages MainWindow status bar widgets, telemetry synchronization, and network latency probes.
 * @deps    QObject, QString
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */
#pragma once

#include <QObject>
#include <QString>

class MainWindow;
class QLabel;
class QPushButton;
class QTimer;
class QEvent;
struct RobotInfo;

class MainWindowStatusController : public QObject
{
    Q_OBJECT

public:
    explicit MainWindowStatusController(MainWindow *window);
    ~MainWindowStatusController() override = default;

    void setupStatusBar();
    void updateStatusBar();
    void onMqttStatusClicked();
    void onTeleopActiveChanged(bool active);
    bool handleEventFilter(QObject *obj, QEvent *event);
    void updateDeviceProfile();

private:
    void updateMqttStatusLabel();
    void updateTelemetryLabels(const RobotInfo &info);
    void testNetworkLatency();

    MainWindow  *m_window         = nullptr;
    QLabel      *m_sbDevice       = nullptr;
    QLabel      *m_sbCarId        = nullptr;
    QLabel      *m_sbConn         = nullptr;
    QLabel      *m_sbCov          = nullptr;
    QLabel      *m_sbLatency      = nullptr;
    QLabel      *m_sbTeleop       = nullptr;
    QLabel      *m_sbLocStatus    = nullptr;
    QLabel      *m_sbMapStatus    = nullptr;
    QPushButton *m_teleopBtn      = nullptr;

    int          m_networkLatency = -1;
    QTimer      *m_statusTimer    = nullptr;
};
