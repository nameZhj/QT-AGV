/**
 * @file    TaskChainWidget.cpp
 * @layer   GUILayer
 * @brief   Visual task chain workflow orchestrator shell and UI delegator.
 * @deps    TaskChainWidget.h, TaskChainPanelsBuilder.h, TaskChainRuntimeController.h, TaskChainCrudHandler.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "TaskChainWidget.h"
#include "TaskChainPanelsBuilder.h"
#include "TaskChainRuntimeController.h"
#include "TaskChainActionDefs.h"
#include "TaskChainRowHelper.h"
#include "TaskChainActionExecutor.h"
#include "TaskChainClipboardHelper.h"
#include "TaskChainDataSync.h"
#include "TaskChainListSync.h"
#include "TaskChainCrudHandler.h"
#include "TaskChainGroupActionHandler.h"
#include "TaskChainJsonSerializer.h"
#include "core/TaskChainManager.h"
#include <QVBoxLayout>
#include <QApplication>
#include <QShortcut>
#include <QLineEdit>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QTimer>
#include <QFile>

TaskChainWidget::TaskChainWidget(QWidget *parent)
    : QWidget(parent)
{
    m_runtimeCtrl = new TaskChainRuntimeController(this);
    initActionDefs();
    setupUI();

    m_cacheTimer = new QTimer(this);
    m_cacheTimer->setSingleShot(true);
    m_cacheTimer->setInterval(800);
    connect(m_cacheTimer, &QTimer::timeout, this, &TaskChainWidget::writeCache);
}

TaskChainWidget::~TaskChainWidget()
{
    if (m_runtimeCtrl) m_runtimeCtrl->unsubscribeTopics();

    if (m_didSave) {
        if (m_cacheTimer && m_cacheTimer->isActive()) {
            m_cacheTimer->stop();
            writeCache();
        }
    } else {
        QFile::remove(TaskChainJsonSerializer::cacheFilePath());
    }
}

void TaskChainWidget::setupUI()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_panelsContainer = new QWidget(this);
    m_panelsContainer->installEventFilter(this);

    TaskChainPanelsBuilder::buildPanels(this, m_panelsContainer);
    root->addWidget(m_panelsContainer, 1);

    refreshActionTags();
    TaskChainPanelsBuilder::setupBottomBar(this, root);

    connect(&TaskChainManager::instance(), &TaskChainManager::chainStatusChanged, this, [this](bool working, bool paused) {
        m_chainWorking = working;
        m_chainPaused  = paused;
        if (!working) {
            m_execStatus = NotStarted;
            setListsLocked(false);
            updateStatus(QStringLiteral("任务链已结束, 编辑区域已解锁"));
        } else if (paused) {
            m_execStatus = Suspended;
            setListsLocked(true);
        } else {
            m_execStatus = Running;
            setListsLocked(true);
        }

        if (m_overlayTextLabel) {
            m_overlayTextLabel->setText(m_chainPaused ? QStringLiteral("任务链已暂停") : QStringLiteral("任务执行中"));
        }
        updateBottomBar();
    });

    connect(&TaskChainManager::instance(), &TaskChainManager::currentTaskChanged, this, [this](const QString &task, const QString &group) {
        m_currentTaskName  = task;
        m_currentGroupName = group;
        applyRuntimeSelection();
        updateBottomBar();
    });

    auto *copyShortcut = new QShortcut(QKeySequence::Copy, this, nullptr, nullptr, Qt::WidgetWithChildrenShortcut);
    connect(copyShortcut, &QShortcut::activated, this, [this]() {
        if (!isVisible() || m_listLocked || m_execStatus == Running) return;
        QWidget *fw = QApplication::focusWidget();
        if (fw && (qobject_cast<QLineEdit*>(fw) || qobject_cast<QTextEdit*>(fw) || qobject_cast<QPlainTextEdit*>(fw))) return;
        copySelectedGroup();
    });

    auto *pasteShortcut = new QShortcut(QKeySequence::Paste, this, nullptr, nullptr, Qt::WidgetWithChildrenShortcut);
    connect(pasteShortcut, &QShortcut::activated, this, [this]() {
        if (!isVisible() || m_listLocked || m_execStatus == Running) return;
        QWidget *fw = QApplication::focusWidget();
        if (fw && (qobject_cast<QLineEdit*>(fw) || qobject_cast<QTextEdit*>(fw) || qobject_cast<QPlainTextEdit*>(fw))) return;
        pasteGroup();
    });

    updateBottomBar();
}

void TaskChainWidget::handleActionDropped(const QString &actionName, int groupIdx)
{
    TaskChainGroupActionHandler::handleActionDropped(this, actionName, groupIdx);
}

void TaskChainWidget::showGroupContextMenu(const QPoint &pos)
{
    TaskChainGroupActionHandler::showGroupContextMenu(this, pos);
}

void TaskChainWidget::refreshChainList() { TaskChainListSync::refreshChainList(this); }
void TaskChainWidget::createChainRowWidget(QListWidgetItem *item, int chainIndex) { TaskChainListSync::createChainRowWidget(this, item, chainIndex); }
void TaskChainWidget::selectChain(int index) { TaskChainListSync::selectChain(this, index); }
int  TaskChainWidget::currentChainIndex() const { return TaskChainListSync::currentChainIndex(this); }
QJsonArray TaskChainWidget::currentTaskGroups() const { return TaskChainListSync::currentTaskGroups(this); }
void TaskChainWidget::setCurrentTaskGroups(const QJsonArray &groups) { TaskChainListSync::setCurrentTaskGroups(this, groups); }
void TaskChainWidget::refreshTaskList() { TaskChainListSync::refreshTaskList(this); }
void TaskChainWidget::refreshGroupList() { TaskChainListSync::refreshGroupList(this); }
void TaskChainWidget::createGroupRowWidget(QListWidgetItem *item, int groupIndex, bool checked) { TaskChainListSync::createGroupRowWidget(this, item, groupIndex, checked); }
void TaskChainWidget::refreshActionTags() { TaskChainListSync::refreshActionTags(this); }
void TaskChainWidget::initActionDefs() { m_actionDefs = TaskChainActionDefs::createDefaultDefs(); }
ActionDef* TaskChainWidget::findActionDef(const QString &name) { return TaskChainActionDefs::findDef(m_actionDefs, name); }
QString TaskChainWidget::openActionDefDialog(const ActionDef &def, const QJsonObject &existingParams) { return TaskChainGroupActionHandler::openActionDefDialog(this, def, existingParams); }

void TaskChainWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_listLocked && m_execStatus != Running && !m_chainWorking) {
        loadMapStations();
    }
    loadChainsFromCacheAndServer();
    subscribeChainStatus();
    subscribeChainCurrentTask();

    auto &mgr = TaskChainManager::instance();
    m_chainWorking     = mgr.isWorking();
    m_chainPaused      = mgr.isPaused();
    m_currentTaskName  = mgr.currentTask();
    m_currentGroupName = mgr.currentGroup();

    if (m_chainWorking || mgr.isLocalExecutionActive()) {
        m_execStatus = m_chainPaused ? Suspended : Running;
        setListsLocked(true);
    } else if (m_execStatus != Running && m_execStatus != Suspended) {
        m_execStatus = NotStarted;
        setListsLocked(false);
    }

    if (m_overlayTextLabel) {
        m_overlayTextLabel->setText(m_chainPaused ? QStringLiteral("任务链已暂停") : QStringLiteral("任务执行中"));
    }

    applyRuntimeSelection();
    updateBottomBar();

    if (m_listLocked || m_execStatus == Running || m_chainWorking) {
        setListsLocked(true);
        updateLockOverlayGeometry();
        QTimer::singleShot(0, this, [this]() {
            if (m_listLocked || m_execStatus == Running || m_chainWorking) {
                updateLockOverlayGeometry();
            }
            applyRuntimeSelection();
        });
    }
}

void TaskChainWidget::subscribeChainStatus() { if (m_runtimeCtrl) m_runtimeCtrl->subscribeTopics(); }
void TaskChainWidget::onChainStatusMessage(const QByteArray &) {}
void TaskChainWidget::subscribeChainCurrentTask() { if (m_runtimeCtrl) m_runtimeCtrl->subscribeTopics(); }
void TaskChainWidget::onChainCurrentTaskMessage(const QByteArray &) {}
void TaskChainWidget::updateLockOverlayGeometry() { if (m_runtimeCtrl) m_runtimeCtrl->updateLockOverlayGeometry(); }
void TaskChainWidget::resizeEvent(QResizeEvent *event) { QWidget::resizeEvent(event); updateLockOverlayGeometry(); }
void TaskChainWidget::setListsLocked(bool locked) { if (m_runtimeCtrl) m_runtimeCtrl->setListsLocked(locked); }
void TaskChainWidget::installLockFilter(QWidget *w, bool install) { if (m_runtimeCtrl) m_runtimeCtrl->installLockFilter(w, install); }
bool TaskChainWidget::eventFilter(QObject *obj, QEvent *event) { if (m_runtimeCtrl && m_runtimeCtrl->filterEvent(obj, event)) return true; return QWidget::eventFilter(obj, event); }
void TaskChainWidget::applyRuntimeSelection() { if (m_runtimeCtrl) m_runtimeCtrl->applyRuntimeSelection(); }
void TaskChainWidget::updateRuntimeCardHighlights(int currentTaskIdx, int currentGroupIdx) { if (m_runtimeCtrl) m_runtimeCtrl->updateRuntimeCardHighlights(currentTaskIdx, currentGroupIdx); }
bool TaskChainWidget::matchSubTaskName(int taskIdx, const QString &targetName) const { return m_runtimeCtrl ? m_runtimeCtrl->matchSubTaskName(taskIdx, targetName) : false; }
bool TaskChainWidget::matchGroupName(int groupIdx, const QString &targetName) const { return m_runtimeCtrl ? m_runtimeCtrl->matchGroupName(groupIdx, targetName) : false; }

QString TaskChainWidget::subTaskNameAt(int taskIdx) const
{
    const int idx = currentChainIndex();
    if (idx < 0 || idx >= m_chains.size()) return QString();
    if (taskIdx < 0 || taskIdx >= m_chains[idx].tasks.size()) return QString();

    const QJsonObject task = m_chains[idx].tasks[taskIdx].toObject();
    QString name = task.value("sub_task_name").toString();
    if (name.isEmpty()) name = task.value("name").toString();
    if (name.isEmpty()) name = QString("sub_task_%1").arg(taskIdx + 1);
    return name;
}

QString TaskChainWidget::groupNameAt(int groupIdx) const
{
    const QJsonArray groups = currentTaskGroups();
    if (groupIdx < 0 || groupIdx >= groups.size()) return QString();

    const QJsonObject group = groups[groupIdx].toObject();
    QString name = group.value("group_name").toString();
    if (name.isEmpty()) name = group.value("name").toString();
    if (name.isEmpty()) name = QString("group_%1").arg(groupIdx + 1);
    return name;
}

QString TaskChainWidget::stationName(const QString &idOrName) const { return TaskChainDataSync::stationName(this, idOrName); }
void TaskChainWidget::loadChainsFromCacheAndServer() { TaskChainDataSync::loadChainsFromCacheAndServer(this); }
void TaskChainWidget::scheduleCacheWrite() { TaskChainDataSync::scheduleCacheWrite(this); }
void TaskChainWidget::writeCache() { TaskChainDataSync::writeCache(this); }
void TaskChainWidget::loadMapStations() { TaskChainDataSync::loadMapStations(this); }
void TaskChainWidget::updateStatus(const QString &text) { TaskChainListSync::updateStatus(this, text); }

void TaskChainWidget::onChainNew() { TaskChainCrudHandler::onChainNew(this); }
void TaskChainWidget::onChainLoad() { TaskChainCrudHandler::onChainLoad(this); }
void TaskChainWidget::onChainSave() { TaskChainCrudHandler::onChainSave(this); }
void TaskChainWidget::onChainExport() { TaskChainCrudHandler::onChainExport(this); }
void TaskChainWidget::onChainItemChanged(QListWidgetItem *current) {
    TaskChainCrudHandler::onChainItemChanged(this, current);
    updateChainCardHighlight();
}
void TaskChainWidget::onTaskNew() { TaskChainCrudHandler::onTaskNew(this); }
void TaskChainWidget::onTaskDelete() { TaskChainCrudHandler::onTaskDelete(this); }
void TaskChainWidget::onTaskSelectAll() { TaskChainCrudHandler::onTaskSelectAll(this); }
void TaskChainWidget::onGroupAdd() { TaskChainCrudHandler::onGroupAdd(this); }
void TaskChainWidget::onGroupInsert() { TaskChainCrudHandler::onGroupInsert(this); }
void TaskChainWidget::onGroupDelete() { TaskChainCrudHandler::onGroupDelete(this); }
void TaskChainWidget::onGroupSelectAll() { TaskChainCrudHandler::onGroupSelectAll(this); }
void TaskChainWidget::onGroupSendSelected() { TaskChainActionExecutor::sendTaskChain(this, true); }
void TaskChainWidget::onGroupSendAll() { TaskChainActionExecutor::sendTaskChain(this, false); }

void TaskChainWidget::updateBottomBar() { TaskChainListSync::updateBottomBar(this); }
void TaskChainWidget::updateTaskSelectBtn() { TaskChainListSync::updateTaskSelectBtn(this); }
void TaskChainWidget::updateGroupSelectBtn() { TaskChainListSync::updateGroupSelectBtn(this); }
void TaskChainWidget::updateChainCardHighlight() { TaskChainListSync::updateChainCardHighlight(this); }

QJsonObject TaskChainWidget::exportChainToJson(const ChainData &chain, bool onlyChecked, bool forExecution) const
{
    return TaskChainJsonSerializer::exportChainToJson(chain, onlyChecked, forExecution, m_stations);
}

TaskChainWidget::ChainData TaskChainWidget::importChainFromJson(const QJsonObject &json) const
{
    return TaskChainJsonSerializer::importChainFromJson(json);
}

void TaskChainWidget::executeSingleAction(const QJsonObject &action) { TaskChainActionExecutor::executeSingleAction(this, action); }
void TaskChainWidget::onSuspendResume() { if (m_runtimeCtrl) m_runtimeCtrl->onSuspendResume(); }
void TaskChainWidget::onCancelChain() { if (m_runtimeCtrl) m_runtimeCtrl->onCancelChain(); }
void TaskChainWidget::onExecuteNext() { if (m_runtimeCtrl) m_runtimeCtrl->onExecuteNext(); }
void TaskChainWidget::reorderGroup(int fromIdx, int toIdx) { TaskChainClipboardHelper::reorderGroup(this, fromIdx, toIdx); }
void TaskChainWidget::reorderTask(int fromIdx, int toIdx) { TaskChainClipboardHelper::reorderTask(this, fromIdx, toIdx); }
void TaskChainWidget::copySelectedGroup() { TaskChainClipboardHelper::copySelectedGroup(this); }
void TaskChainWidget::pasteGroup() { TaskChainClipboardHelper::pasteGroup(this); }
void TaskChainWidget::updateGroupCardSelectionHighlight() { TaskChainClipboardHelper::updateGroupCardSelectionHighlight(this); }
void TaskChainWidget::keyPressEvent(QKeyEvent *event) { if (TaskChainClipboardHelper::handleKeyPress(this, event)) return; QWidget::keyPressEvent(event); }
