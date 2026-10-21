/**
 * @file    TaskChainClipboardHelper.cpp
 * @layer   GUI
 * @brief   Implementation of task/group clipboard operations (copy/paste) and reordering.
 * @deps    TaskChainClipboardHelper.h, TaskChainWidget.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "TaskChainClipboardHelper.h"
#include "TaskChainWidget.h"
#include "ui/common/Toast.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QKeyEvent>
#include <QLineEdit>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QListWidget>
#include <QListWidgetItem>

void TaskChainClipboardHelper::reorderGroup(TaskChainWidget *owner, int fromIdx, int toIdx)
{
    if (!owner) return;
    QJsonArray groups = owner->currentTaskGroups();
    const int count = groups.size();
    if (fromIdx < 0 || fromIdx >= count || toIdx < 0 || toIdx >= count || fromIdx == toIdx) return;

    const QJsonValue item = groups.takeAt(fromIdx);
    groups.insert(toIdx, item);

    owner->setCurrentTaskGroups(groups);
    owner->refreshGroupList();
    if (owner->m_groupList && toIdx >= 0 && toIdx < owner->m_groupList->count()) {
        owner->m_groupList->setCurrentRow(toIdx);
    }
    owner->updateStatus(QStringLiteral("组已重新排序: 第 %1 组 -> 第 %2 组").arg(fromIdx + 1).arg(toIdx + 1));
}

void TaskChainClipboardHelper::reorderTask(TaskChainWidget *owner, int fromIdx, int toIdx)
{
    if (!owner) return;
    const int cIdx = owner->currentChainIndex();
    if (cIdx < 0 || cIdx >= owner->m_chains.size()) return;
    auto &tasks = owner->m_chains[cIdx].tasks;
    const int count = tasks.size();
    if (fromIdx < 0 || fromIdx >= count || toIdx < 0 || toIdx >= count || fromIdx == toIdx) return;

    const QJsonValue item = tasks.takeAt(fromIdx);
    tasks.insert(toIdx, item);

    owner->refreshTaskList();
    if (owner->m_taskList && toIdx >= 0 && toIdx < owner->m_taskList->count()) {
        owner->m_taskList->setCurrentRow(toIdx);
    }
    owner->updateStatus(QStringLiteral("任务已重新排序: 第 %1 项 -> 第 %2 项").arg(fromIdx + 1).arg(toIdx + 1));
}

void TaskChainClipboardHelper::copySelectedGroup(TaskChainWidget *owner)
{
    if (!owner || owner->m_listLocked || owner->m_execStatus == TaskChainWidget::Running) return;
    if (owner->m_currentTaskIndex < 0) {
        Toast::info(owner, QStringLiteral("请先选择任务"));
        return;
    }

    const QJsonArray groups = owner->currentTaskGroups();
    if (groups.isEmpty()) {
        Toast::info(owner, QStringLiteral("当前任务没有可复制的组"));
        return;
    }

    QList<int> targetIndices;
    if (owner->m_groupList) {
        const auto selItems = owner->m_groupList->selectedItems();
        if (selItems.size() > 1) {
            for (auto *it : selItems) {
                const int row = owner->m_groupList->row(it);
                if (row >= 0 && row < groups.size() && !targetIndices.contains(row)) {
                    targetIndices.append(row);
                }
            }
        }
        if (targetIndices.isEmpty()) {
            const int curRow = owner->m_groupList->currentRow();
            if (curRow >= 0 && curRow < groups.size()) targetIndices.append(curRow);
        }
    }

    if (targetIndices.isEmpty()) {
        Toast::info(owner, QStringLiteral("请先用鼠标点击选中要复制的组"));
        return;
    }

    std::sort(targetIndices.begin(), targetIndices.end());
    QJsonArray copiedArray;
    QStringList copiedNames;
    for (int idx : targetIndices) {
        const QJsonObject gObj = groups[idx].toObject();
        copiedArray.append(gObj);
        QString gName = gObj.value("group_name").toString();
        if (gName.isEmpty()) gName = gObj.value("name").toString();
        if (gName.isEmpty()) gName = QString("group_%1").arg(idx + 1);
        copiedNames.append(gName);
    }

    owner->m_copiedGroups = copiedArray;

    QJsonObject clipObj;
    clipObj["type"]   = "taskchain_groups";
    clipObj["groups"] = copiedArray;
    const QByteArray clipData = QJsonDocument(clipObj).toJson(QJsonDocument::Compact);

    auto *mime = new QMimeData;
    mime->setData("application/x-taskchain-groups", clipData);
    mime->setText(QString::fromUtf8(clipData));
    QApplication::clipboard()->setMimeData(mime);

    Toast::success(owner, QStringLiteral("已复制选中的组: %1").arg(copiedNames.join(", ")));
    owner->updateStatus(QStringLiteral("已复制组: %1 (可按 Ctrl+V 粘贴)").arg(copiedNames.join(", ")));
}

void TaskChainClipboardHelper::pasteGroup(TaskChainWidget *owner)
{
    if (!owner || owner->m_listLocked || owner->m_execStatus == TaskChainWidget::Running) return;
    if (owner->m_currentTaskIndex < 0) {
        Toast::info(owner, QStringLiteral("请先选择要粘贴到的任务"));
        return;
    }

    QJsonArray groupsToPaste = owner->m_copiedGroups;
    if (groupsToPaste.isEmpty()) {
        const QMimeData *mime = QApplication::clipboard()->mimeData();
        if (mime) {
            QByteArray data;
            if (mime->hasFormat("application/x-taskchain-groups")) data = mime->data("application/x-taskchain-groups");
            else if (mime->hasText()) data = mime->text().toUtf8();
            if (!data.isEmpty()) {
                const QJsonDocument doc = QJsonDocument::fromJson(data);
                if (doc.isObject() && doc.object().value("type").toString() == "taskchain_groups") {
                    groupsToPaste = doc.object().value("groups").toArray();
                } else if (doc.isArray()) {
                    groupsToPaste = doc.array();
                }
            }
        }
    }

    if (groupsToPaste.isEmpty()) {
        Toast::info(owner, QStringLiteral("剪贴板中没有可粘贴的组，请先按 Ctrl+C 复制组"));
        return;
    }

    QJsonArray curGroups = owner->currentTaskGroups();
    int insertPos = curGroups.size();
    if (owner->m_groupList) {
        const int curRow = owner->m_groupList->currentRow();
        if (curRow >= 0 && curRow < curGroups.size()) insertPos = curRow + 1;
    }

    int maxGroupNum = 0;
    for (const auto &gVal : curGroups) {
        QString n = gVal.toObject().value("group_name").toString();
        if (n.isEmpty()) n = gVal.toObject().value("name").toString();
        if (n.startsWith("group_")) {
            bool ok = false;
            int num = n.mid(6).toInt(&ok);
            if (ok && num > maxGroupNum) maxGroupNum = num;
        }
    }
    if (maxGroupNum < curGroups.size()) maxGroupNum = curGroups.size();

    QStringList pastedNames;
    const int firstPastedRow = insertPos;
    for (int i = 0; i < groupsToPaste.size(); ++i) {
        QJsonObject g = groupsToPaste[i].toObject();
        maxGroupNum++;
        const QString newName = QString("group_%1").arg(maxGroupNum);
        g["group_name"] = newName;
        g["name"]       = newName;
        g["enable"]     = false;
        g["checked"]    = false;
        curGroups.insert(insertPos + i, g);
        pastedNames.append(newName);
    }

    owner->setCurrentTaskGroups(curGroups);
    owner->refreshGroupList();

    if (owner->m_groupList && firstPastedRow >= 0 && firstPastedRow < owner->m_groupList->count()) {
        owner->m_groupList->setCurrentRow(firstPastedRow);
        owner->m_groupList->scrollToItem(owner->m_groupList->item(firstPastedRow));
    }
    updateGroupCardSelectionHighlight(owner);

    Toast::success(owner, QStringLiteral("已粘贴组: %1").arg(pastedNames.join(", ")));
    owner->updateStatus(QStringLiteral("已粘贴 %1 个组: %2").arg(pastedNames.size()).arg(pastedNames.join(", ")));
}

void TaskChainClipboardHelper::updateGroupCardSelectionHighlight(TaskChainWidget *owner)
{
    if (!owner || !owner->m_groupList || owner->m_execStatus == TaskChainWidget::Running || owner->m_chainWorking || owner->m_listLocked) return;
    const int curRow = owner->m_groupList->currentRow();
    for (int i = 0; i < owner->m_groupList->count(); ++i) {
        auto *item = owner->m_groupList->item(i);
        if (!item) continue;
        QWidget *w = owner->m_groupList->itemWidget(item);
        if (!w) continue;
        w->setStyleSheet(i == curRow && curRow >= 0
            ? "#groupCardRoot { background: rgba(0, 188, 212, 0.15); border: 2px solid #00bcd4; border-radius: 4px; }"
            : "#groupCardRoot { background: transparent; border: 1px solid transparent; border-radius: 4px; }");
    }
}

bool TaskChainClipboardHelper::handleKeyPress(TaskChainWidget *owner, QKeyEvent *event)
{
    if (!owner || !event) return false;
    if (!owner->m_listLocked && owner->m_execStatus != TaskChainWidget::Running) {
        const bool isCopy  = event->matches(QKeySequence::Copy) || (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_C);
        const bool isPaste = event->matches(QKeySequence::Paste) || (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_V);
        if (isCopy || isPaste) {
            QWidget *fw = QApplication::focusWidget();
            if (!fw || (!qobject_cast<QLineEdit*>(fw) && !qobject_cast<QTextEdit*>(fw) && !qobject_cast<QPlainTextEdit*>(fw))) {
                if (isCopy) copySelectedGroup(owner); else pasteGroup(owner);
                event->accept();
                return true;
            }
        }
    }
    return false;
}
