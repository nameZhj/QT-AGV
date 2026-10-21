/**
 * @file    TaskChainRuntimeController.h
 * @layer   GUI
 * @brief   Controller for task chain MQTT runtime monitoring, overlay locking, and control commands.
 * @deps    QObject, QString, QByteArray
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QWidget>
#include <QLabel>

class TaskChainWidget;

class TaskChainRuntimeController : public QObject
{
    Q_OBJECT
public:
    enum ExecStatus { NotStarted, Running, Suspended };

    explicit TaskChainRuntimeController(TaskChainWidget *owner);
    ~TaskChainRuntimeController() override;

    void subscribeTopics();
    void unsubscribeTopics();

    ExecStatus execStatus() const { return m_execStatus; }
    void setExecStatus(ExecStatus s) { m_execStatus = s; }
    bool isWorking() const { return m_chainWorking; }
    bool isPaused() const { return m_chainPaused; }
    bool isLocked() const { return m_listLocked; }

    const QString& currentTaskName() const { return m_currentTaskName; }
    const QString& currentGroupName() const { return m_currentGroupName; }

    void setListsLocked(bool locked);
    void updateLockOverlayGeometry();
    void installLockFilter(QWidget *w, bool install);
    bool filterEvent(QObject *obj, QEvent *event);

    void applyRuntimeSelection();
    void updateRuntimeCardHighlights(int currentTaskIdx, int currentGroupIdx);
    bool matchSubTaskName(int taskIdx, const QString &targetName) const;
    bool matchGroupName(int groupIdx, const QString &targetName) const;

    void onSuspendResume();
    void onCancelChain();
    void onExecuteNext();

private:
    void onChainStatusMessage(const QByteArray &payload);
    void onChainCurrentTaskMessage(const QByteArray &payload);

    TaskChainWidget *m_owner = nullptr;
    ExecStatus       m_execStatus = NotStarted;
    bool             m_chainWorking = false;
    bool             m_chainPaused  = false;
    bool             m_statusSubscribed = false;
    bool             m_currentSubscribed = false;
    bool             m_listLocked = false;
    QString          m_currentTaskName;
    QString          m_currentGroupName;
};
