/**
 * @file    MapViewNavInteraction.h
 * @layer   GUI
 * @brief   Controller for station path navigation, free coordinate movement, and navigation controls.
 * @deps    QObject, QPointF
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QObject>
#include <QPointF>

class MotionDialog;

class MapViewNavInteraction : public QObject
{
    Q_OBJECT
public:
    enum OperationType { OpNone, OpNavigation, OpMotion };

    explicit MapViewNavInteraction(QObject *parent = nullptr);
    ~MapViewNavInteraction() override = default;

    bool isNavMode() const { return m_navMode; }
    void setNavMode(bool on) { m_navMode = on; }

    bool isMoveMode() const { return m_moveMode; }
    void setMoveMode(bool on) { m_moveMode = on; }

    bool isNavActive() const { return m_navActive; }
    bool isNavPaused() const { return m_navPaused; }
    OperationType currentOpType() const { return m_currentOpType; }

    void handleStationNavClick(const QString &ptId, const QString &ptType, const QString &ptName,
                               const QPointF &wPos, QWidget *parent);

    void handleFreeMoveClick(const QPointF &worldPos, double curWx, double curWy, QWidget *parent);

    void cancelNavigation();
    void pauseOrResumeNavigation();
    void toggleMoveMode(QWidget *parent);

    void updateRobotNavStatus(const QString &newStatus);

signals:
    void statusHint(const QString &msg, const QString &color);
    void navButtonsUpdateRequested();

private:
    bool          m_navMode           = false;
    bool          m_moveMode          = false;
    bool          m_navActive         = false;
    bool          m_navPaused         = false;
    bool          m_navEnteredRunning = false;
    qint64        m_navStartTime      = 0;

    OperationType m_currentOpType     = OpNone;
    QString       m_currentNavTargetName;
    QString       m_currentMotionDesc;
    QString       m_lastRobotNavStatus = "0";

    MotionDialog *m_motionDialog      = nullptr;
};
