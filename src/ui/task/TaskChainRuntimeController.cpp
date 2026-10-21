/**
 * @file    TaskChainRuntimeController.cpp
 * @layer   GUI
 * @brief   Implementation of task chain MQTT runtime monitoring, overlay locking, and control commands.
 * @deps    TaskChainRuntimeController.h, TaskChainWidget.h, MqttService.h, ApiService.h, TaskChainManager.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "TaskChainRuntimeController.h"
#include "TaskChainWidget.h"
#include "network/MqttService.h"
#include "network/ApiService.h"
#include "core/TaskChainManager.h"
#include "TaskChainStyle.h"
#include "ui/common/Toast.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QEvent>
#include <QListWidget>
#include <QListWidgetItem>

TaskChainRuntimeController::TaskChainRuntimeController(TaskChainWidget *owner)
    : QObject(owner), m_owner(owner) {}

TaskChainRuntimeController::~TaskChainRuntimeController() { unsubscribeTopics(); }

void TaskChainRuntimeController::subscribeTopics()
{
    if (!m_statusSubscribed) {
        m_statusSubscribed = true;
        MqttService::instance().subscribeTopic("/taskchain/status", 0,
            [this](const QString &, const QByteArray &p) { onChainStatusMessage(p); });
    }
    if (!m_currentSubscribed) {
        m_currentSubscribed = true;
        MqttService::instance().subscribeTopic("/taskchain/currentTask", 0,
            [this](const QString &, const QByteArray &p) { onChainCurrentTaskMessage(p); });
    }
}

void TaskChainRuntimeController::unsubscribeTopics()
{
    if (m_statusSubscribed) { MqttService::instance().unsubscribeTopic("/taskchain/status"); m_statusSubscribed = false; }
    if (m_currentSubscribed) { MqttService::instance().unsubscribeTopic("/taskchain/currentTask"); m_currentSubscribed = false; }
}

void TaskChainRuntimeController::onChainStatusMessage(const QByteArray &payload)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return;

    const QJsonObject data = doc.object();
    m_chainWorking = data.value("robotWorking").toBool(false);
    m_chainPaused  = data.value("robotPause").toBool(false);
    m_execStatus   = !m_chainWorking ? NotStarted : (m_chainPaused ? Suspended : Running);

    if (m_owner) {
        m_owner->m_chainWorking = m_chainWorking;
        m_owner->m_chainPaused  = m_chainPaused;
        m_owner->m_execStatus   = static_cast<TaskChainWidget::ChainStatus>(m_execStatus);
        if (m_chainWorking && !m_listLocked) setListsLocked(true);
        else if (!m_chainWorking && m_listLocked) { setListsLocked(false); m_owner->updateStatus(QStringLiteral("任务链已结束, 编辑区域已解锁")); }
        if (m_owner->m_overlayTextLabel) m_owner->m_overlayTextLabel->setText(m_chainPaused ? QStringLiteral("任务链已暂停") : QStringLiteral("任务执行中"));
        m_owner->updateBottomBar();
    }
}

void TaskChainRuntimeController::onChainCurrentTaskMessage(const QByteArray &payload)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return;

    const QJsonObject data = doc.object();
    m_currentTaskName  = data.value("currentTask").toString();
    m_currentGroupName = data.value("currentGroup").toString();
    if (m_owner) {
        m_owner->m_currentTaskName = m_currentTaskName;
        m_owner->m_currentGroupName = m_currentGroupName;
    }
    applyRuntimeSelection();
    if (m_owner) m_owner->updateBottomBar();
}

void TaskChainRuntimeController::updateLockOverlayGeometry()
{
    if (!m_owner || !m_owner->m_lockOverlay || !m_owner->m_panelsContainer) return;
    m_owner->m_lockOverlay->setGeometry(m_owner->m_panelsContainer->rect());
    if (m_listLocked || m_execStatus == Running || m_chainWorking) {
        m_owner->m_lockOverlay->show();
        m_owner->m_lockOverlay->raise();
    }
}

void TaskChainRuntimeController::setListsLocked(bool locked)
{
    m_listLocked = locked;
    if (!m_owner) return;
    m_owner->m_listLocked = locked;

    if (m_owner->m_lockOverlay && m_owner->m_panelsContainer) {
        if (locked) {
            if (m_owner->m_overlayTextLabel) m_owner->m_overlayTextLabel->setText(m_chainPaused ? QStringLiteral("任务链已暂停") : QStringLiteral("任务执行中"));
            m_owner->m_lockOverlay->setGeometry(m_owner->m_panelsContainer->rect());
            m_owner->m_lockOverlay->show();
            m_owner->m_lockOverlay->raise();
        } else {
            m_owner->m_lockOverlay->hide();
        }
    }

    installLockFilter(m_owner->m_panelsContainer, locked);
    if (m_owner->m_lockOverlay) {
        m_owner->m_lockOverlay->removeEventFilter(m_owner);
        if (m_owner->m_overlayTextLabel) m_owner->m_overlayTextLabel->removeEventFilter(m_owner);
    }

    const auto policy = locked ? Qt::NoFocus : Qt::StrongFocus;
    if (m_owner->m_chainList) m_owner->m_chainList->setFocusPolicy(policy);
    if (m_owner->m_taskList)  m_owner->m_taskList->setFocusPolicy(policy);
    if (m_owner->m_groupList) m_owner->m_groupList->setFocusPolicy(policy);

    if (locked) {
        if (m_owner->m_groupHintLabel) m_owner->m_groupHintLabel->setText(QStringLiteral("任务链执行中, 编辑区域已锁定遮罩 (任务链结束后自动解锁)"));
    } else {
        m_currentTaskName.clear();
        m_currentGroupName.clear();
        updateRuntimeCardHighlights(-1, -1);
        if (m_owner->m_groupHintLabel) m_owner->m_groupHintLabel->setText(QStringLiteral("拖拽标签到组中, 每组只能添加一种标签"));
    }
}

void TaskChainRuntimeController::installLockFilter(QWidget *w, bool install)
{
    if (!w || !m_owner) return;
    auto toggle = [this, install](QWidget *target) { if (install) target->installEventFilter(m_owner); else target->removeEventFilter(m_owner); };
    toggle(w);
    for (QWidget *c : w->findChildren<QWidget*>()) toggle(c);
}

bool TaskChainRuntimeController::filterEvent(QObject *obj, QEvent *event)
{
    if (!m_owner) return false;
    if (obj == m_owner->m_panelsContainer && event->type() == QEvent::Resize) {
        updateLockOverlayGeometry();
    }
    if (m_listLocked && obj != m_owner->m_lockOverlay &&
        (!m_owner->m_lockOverlay || !m_owner->m_lockOverlay->isAncestorOf(qobject_cast<const QWidget*>(obj)))) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: case QEvent::MouseButtonRelease:
        case QEvent::MouseButtonDblClick: case QEvent::MouseMove:
        case QEvent::Wheel: case QEvent::KeyPress: case QEvent::KeyRelease:
        case QEvent::DragEnter: case QEvent::DragMove:
        case QEvent::DragLeave: case QEvent::Drop: case QEvent::ContextMenu:
            return true;
        default: break;
        }
    }
    return false;
}

void TaskChainRuntimeController::applyRuntimeSelection()
{
    if (!m_owner) return;
    int targetTaskIdx = -1;
    if (!m_currentTaskName.isEmpty() && m_owner->m_taskList) {
        for (int i = 0; i < m_owner->m_taskList->count(); ++i) {
            QListWidgetItem *it = m_owner->m_taskList->item(i);
            if (it && matchSubTaskName(it->data(Qt::UserRole).toInt(), m_currentTaskName)) {
                targetTaskIdx = it->data(Qt::UserRole).toInt();
                m_owner->m_taskList->setCurrentItem(it);
                m_owner->m_taskList->scrollToItem(it);
                break;
            }
        }
    }
    if ((targetTaskIdx >= 0 && targetTaskIdx != m_owner->m_currentTaskIndex) ||
        (m_owner->m_groupList && m_owner->m_groupList->count() == 0 && targetTaskIdx >= 0)) {
        m_owner->m_currentTaskIndex = targetTaskIdx;
        m_owner->refreshGroupList();
    }
    int targetGroupIdx = -1;
    if (!m_currentGroupName.isEmpty() && m_owner->m_groupList) {
        for (int i = 0; i < m_owner->m_groupList->count(); ++i) {
            QListWidgetItem *it = m_owner->m_groupList->item(i);
            if (it && matchGroupName(it->data(Qt::UserRole).toInt(), m_currentGroupName)) {
                targetGroupIdx = it->data(Qt::UserRole).toInt();
                m_owner->m_groupList->setCurrentItem(it);
                m_owner->m_groupList->scrollToItem(it);
                break;
            }
        }
    }
    updateRuntimeCardHighlights(targetTaskIdx, targetGroupIdx);
}

void TaskChainRuntimeController::updateRuntimeCardHighlights(int currentTaskIdx, int currentGroupIdx)
{
    if (!m_owner) return;
    auto applyStyle = [](QListWidget *list, int activeIdx, const char *act, const char *norm) {
        if (!list) return;
        for (int i = 0; i < list->count(); ++i) {
            QListWidgetItem *it = list->item(i);
            if (it && list->itemWidget(it)) {
                list->itemWidget(it)->setStyleSheet(it->data(Qt::UserRole).toInt() == activeIdx && activeIdx >= 0 ? act : norm);
            }
        }
    };
    applyStyle(m_owner->m_taskList, currentTaskIdx, kTaskCardActive, kTaskCardRoot);
    applyStyle(m_owner->m_groupList, currentGroupIdx, kGroupCardActive, kGroupCardRoot);
}

bool TaskChainRuntimeController::matchSubTaskName(int taskIdx, const QString &targetName) const
{
    if (targetName.isEmpty() || !m_owner) return false;
    const int idx = m_owner->currentChainIndex();
    if (idx < 0 || idx >= m_owner->m_chains.size() || taskIdx < 0 || taskIdx >= m_owner->m_chains[idx].tasks.size()) return false;
    const QJsonObject t = m_owner->m_chains[idx].tasks[taskIdx].toObject();
    return (t.value("sub_task_name").toString() == targetName || t.value("name").toString() == targetName || QString("sub_task_%1").arg(taskIdx + 1) == targetName);
}

bool TaskChainRuntimeController::matchGroupName(int groupIdx, const QString &targetName) const
{
    if (targetName.isEmpty() || !m_owner) return false;
    const QJsonArray groups = m_owner->currentTaskGroups();
    if (groupIdx < 0 || groupIdx >= groups.size()) return false;
    const QJsonObject g = groups[groupIdx].toObject();
    return (g.value("group_name").toString() == targetName || g.value("name").toString() == targetName || QString("group_%1").arg(groupIdx + 1) == targetName);
}

void TaskChainRuntimeController::onSuspendResume()
{
    if (!m_owner) return;
    const bool shouldPause = (m_execStatus == Running || (m_chainWorking && !m_chainPaused));
    m_execStatus = shouldPause ? Suspended : Running;
    m_chainPaused = shouldPause;
    m_owner->m_execStatus = static_cast<TaskChainWidget::ChainStatus>(m_execStatus);
    m_owner->m_chainPaused = m_chainPaused;

    const QString action = shouldPause ? QStringLiteral("pause navigation") : QStringLiteral("continue navigation");
    const QString actionName = shouldPause ? QStringLiteral("暂停") : QStringLiteral("继续");
    ApiService::instance().controlTaskChain(action, [this, actionName](int code, const QJsonObject &resp) {
        if (code != 200 && m_owner) {
            Toast::error(m_owner, QStringLiteral("%1失败(%2): %3").arg(actionName).arg(code).arg(resp.value("msg").toString(QStringLiteral("请求失败"))));
        }
    });
    m_owner->updateStatus(QStringLiteral("任务链已向服务端发送%1指令").arg(actionName));
    if (shouldPause) Toast::info(m_owner, QStringLiteral("任务链已暂停"));
    else Toast::success(m_owner, QStringLiteral("任务链已继续"));
    if (m_owner->m_overlayTextLabel) m_owner->m_overlayTextLabel->setText(shouldPause ? QStringLiteral("任务链已暂停") : QStringLiteral("任务执行中"));
    m_owner->updateBottomBar();
}

void TaskChainRuntimeController::onCancelChain()
{
    if (!m_owner) return;
    m_execStatus = NotStarted;
    m_chainWorking = m_chainPaused = false;
    m_owner->m_execStatus = static_cast<TaskChainWidget::ChainStatus>(m_execStatus);
    m_owner->m_chainWorking = m_owner->m_chainPaused = false;
    TaskChainManager::instance().resetExecutionState();

    setListsLocked(false);
    ApiService::instance().controlTaskChain(QStringLiteral("stop navigation"), [this](int code, const QJsonObject &resp) {
        if (code != 200 && m_owner) {
            Toast::error(m_owner, QStringLiteral("结束失败(%1): %2").arg(code).arg(resp.value("msg").toString(QStringLiteral("请求失败"))));
        }
    });
    m_owner->updateStatus(QStringLiteral("任务链已向服务端发送结束指令"));
    Toast::info(m_owner, QStringLiteral("任务链已取消"));
    m_owner->updateBottomBar();
}

void TaskChainRuntimeController::onExecuteNext()
{
    if (!m_owner) return;
    m_execStatus = Running;
    m_owner->m_execStatus = static_cast<TaskChainWidget::ChainStatus>(m_execStatus);
    m_owner->updateStatus(QStringLiteral("正在请求服务端执行 Next 任务..."));
    m_owner->updateBottomBar();

    ApiService::instance().sendNextTask([this](int code, const QJsonObject &resp) {
        if (!m_owner) return;
        if (code == 200) {
            Toast::success(m_owner, QStringLiteral("执行 Next 任务成功"));
            m_owner->updateStatus(QStringLiteral("服务端已执行 Next 任务"));
        } else {
            QString msg = resp.value("msg").toString(QStringLiteral("请求失败"));
            Toast::error(m_owner, QStringLiteral("执行 Next 任务失败(%1): %2").arg(code).arg(msg));
            m_owner->updateStatus(QStringLiteral("执行 Next 任务失败: %1").arg(msg));
        }
        m_owner->updateBottomBar();
    });
}
