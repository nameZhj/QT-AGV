/**
 * @file    TaskChainCrudHandler.cpp
 * @layer   GUILayer
 * @brief   Implementation of CRUD handlers for task chains, tasks, and groups.
 * @deps    TaskChainCrudHandler.h, TaskChainWidget.h, TaskChainListSync.h, TaskChainJsonSerializer.h, ApiService.h, Toast.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "TaskChainCrudHandler.h"
#include "TaskChainWidget.h"
#include "TaskChainListSync.h"
#include "TaskChainDataSync.h"
#include "TaskChainJsonSerializer.h"
#include "network/ApiService.h"
#include "ui/common/Toast.h"
#include <QDateTime>
#include <QFileDialog>
#include <QMessageBox>
#include <QListWidget>
#include <QListWidgetItem>
#include <algorithm>

void TaskChainCrudHandler::onChainNew(TaskChainWidget *w)
{
    if (!w) return;
    const QString base = QString("task_%1").arg(QDateTime::currentMSecsSinceEpoch());
    int seq = 1;
    for (const auto &c : w->m_chains) {
        if (c.name.startsWith(base + "_")) {
            bool ok = false;
            int n = c.name.mid(base.length() + 1).toInt(&ok);
            if (ok && n >= seq) seq = n + 1;
        }
    }
    QString name = QString("%1_%2").arg(base).arg(seq);

    TaskChainWidget::ChainData data;
    data.name = name;

    w->m_chains.append(data);
    w->refreshChainList();
    w->selectChain(w->m_chains.size() - 1);
    w->updateStatus(QStringLiteral("已新建空任务链: %1, 请继续新建任务").arg(name));
}

void TaskChainCrudHandler::onChainLoad(TaskChainWidget *w)
{
    if (!w) return;
    QString localDir = TaskChainJsonSerializer::taskChainLocalDir();
    QStringList filePaths = QFileDialog::getOpenFileNames(
        w, QStringLiteral("加载任务链 (支持单选或批量多选)"), localDir,
        QStringLiteral("JSON Files (*.json *.task);;所有文件 (*.*)"));
    if (filePaths.isEmpty()) return;

    QList<TaskChainWidget::ChainData> allLoadedChains;
    int successCount = 0;
    if (!TaskChainJsonSerializer::loadChainsFromFiles(filePaths, &allLoadedChains, &successCount)) {
        QMessageBox::warning(w, QStringLiteral("提示"), QStringLiteral("所选文件中未解析到有效的任务链数据"));
        return;
    }

    for (const auto &c : allLoadedChains) w->m_chains.append(c);
    w->refreshChainList();
    w->selectChain(w->m_chains.size() - 1);

    QString msg = QStringLiteral("已成功加载 %1 个文件，共 %2 条任务链").arg(successCount).arg(allLoadedChains.size());
    w->updateStatus(msg);
    Toast::success(w, msg);

    if (filePaths.size() == 1 && allLoadedChains.size() == 1) {
        ApiService::instance().saveTaskChain(filePaths.first(), [w](int code, const QJsonObject &) {
            if (code == 200 && w) w->updateStatus(QStringLiteral("任务链已加载并同步上传至服务器"));
        });
    } else {
        w->scheduleCacheWrite();
    }
}

void TaskChainCrudHandler::onChainExport(TaskChainWidget *w)
{
    if (!w) return;
    if (w->m_chains.isEmpty()) {
        QMessageBox::warning(w, QStringLiteral("提示"), QStringLiteral("任务链列表为空，无可导出数据"));
        return;
    }
    int curIdx = w->currentChainIndex();
    if (curIdx < 0 || curIdx >= w->m_chains.size()) {
        QMessageBox::warning(w, QStringLiteral("提示"), QStringLiteral("请先选择要导出的任务链"));
        return;
    }

    const auto &chain = w->m_chains[curIdx];
    QString defaultName = chain.name.trimmed();
    if (defaultName.isEmpty()) defaultName = QStringLiteral("task_chain");
    if (!defaultName.endsWith(".json", Qt::CaseInsensitive)) defaultName += ".json";
    QString defaultPath = TaskChainJsonSerializer::taskChainLocalDir() + "/" + defaultName;

    QString filePath = QFileDialog::getSaveFileName(w, QStringLiteral("导出当前任务链"), defaultPath, QStringLiteral("JSON Files (*.json)"));
    if (filePath.isEmpty()) return;
    if (!filePath.endsWith(".json", Qt::CaseInsensitive)) filePath += ".json";

    QString err;
    if (!TaskChainJsonSerializer::exportChainToFile(chain, filePath, w->m_stations, &err)) {
        QMessageBox::warning(w, QStringLiteral("错误"), QStringLiteral("无法创建或写入目标文件: %1").arg(err));
        return;
    }

    w->updateStatus(QStringLiteral("已成功导出任务链 [%1] 至: %2").arg(chain.name, filePath));
    Toast::success(w, QStringLiteral("已导出任务链 [%1]").arg(chain.name));
}

void TaskChainCrudHandler::onChainSave(TaskChainWidget *w)
{
    if (!w) return;
    if (w->m_chains.isEmpty()) {
        QMessageBox::warning(w, QStringLiteral("提示"), QStringLiteral("任务链列表为空, 无可保存数据"));
        return;
    }

    int localSavedCount = 0;
    TaskChainJsonSerializer::saveChainsToLocalDir(w->m_chains, w->m_stations, &localSavedCount);

    QJsonArray chains = TaskChainJsonSerializer::buildBatchSavePayload(w->m_chains, w->m_stations);
    w->updateStatus(QStringLiteral("正在保存任务链列表 (%1 条)...").arg(chains.size()));

    ApiService::instance().batchSaveTaskChain(chains,
        [w, count = chains.size(), localSavedCount](int code, const QJsonObject &resp) {
            if (!w) return;
            if (code == 200) {
                w->m_didSave = true;
                w->writeCache();
                w->updateStatus(QStringLiteral("已保存任务链: 本地 %1 条, 服务器 %2 条").arg(localSavedCount).arg(count));
                Toast::success(w, QStringLiteral("已成功保存 %1 条任务链到本地与服务器").arg(count));
            } else {
                Toast::error(w, QStringLiteral("本地已保存(%1条), 但服务器保存失败(%2): %3")
                    .arg(localSavedCount).arg(code).arg(resp.value("msg").toString(QStringLiteral("请求失败"))));
            }
        });
}

void TaskChainCrudHandler::onChainItemChanged(TaskChainWidget *w, QListWidgetItem *current)
{
    if (!w || !current) return;
    int idx = current->data(Qt::UserRole).toInt();
    w->selectChain(idx);
    w->updateBottomBar();
}

void TaskChainCrudHandler::onTaskNew(TaskChainWidget *w)
{
    if (!w) return;
    int idx = w->currentChainIndex();
    if (idx < 0) {
        QMessageBox::warning(w, QStringLiteral("提示"), QStringLiteral("请先选择或新建任务链"));
        return;
    }

    QString subName = QString("sub_task_%1").arg(w->m_chains[idx].tasks.size() + 1);
    QJsonObject task{{"sub_task_name", subName}, {"name", subName}, {"note", ""},
                     {"enable", false}, {"checked", false}, {"group_info", QJsonArray()}, {"groups", QJsonArray()}};
    w->m_chains[idx].tasks.append(task);
    w->refreshTaskList();
}

void TaskChainCrudHandler::onTaskDelete(TaskChainWidget *w)
{
    if (!w) return;
    int idx = w->currentChainIndex();
    if (idx < 0) return;

    auto selected = w->m_taskList->selectedItems();
    if (selected.isEmpty()) {
        QMessageBox::warning(w, QStringLiteral("提示"), QStringLiteral("请勾选需要删除的项"));
        return;
    }

    if (QMessageBox::question(w, QStringLiteral("确认"),
            QStringLiteral("确定要删除勾选的 【%1】 项吗?").arg(selected.size())) != QMessageBox::Yes) {
        return;
    }

    QList<int> rows;
    for (auto *item : selected) {
        rows << w->m_taskList->row(item);
    }
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (int row : rows) {
        if (row >= 0 && row < w->m_chains[idx].tasks.size()) {
            w->m_chains[idx].tasks.removeAt(row);
        }
    }
    w->refreshTaskList();
}

void TaskChainCrudHandler::onTaskSelectAll(TaskChainWidget *w)
{
    if (!w || !w->m_taskList) return;
    TaskChainListSync::setAllCheckBoxesChecked(w->m_taskList, !TaskChainListSync::allCheckBoxesChecked(w->m_taskList));
    w->updateTaskSelectBtn();
}

void TaskChainCrudHandler::onGroupAdd(TaskChainWidget *w)
{
    if (!w) return;
    int idx = w->currentChainIndex();
    if (idx < 0) {
        QMessageBox::warning(w, QStringLiteral("提示"), QStringLiteral("请先选择或新建任务链"));
        return;
    }
    if (w->m_currentTaskIndex < 0) {
        QMessageBox::warning(w, QStringLiteral("提示"), QStringLiteral("请先选择一个任务"));
        return;
    }

    QJsonArray groups = w->currentTaskGroups();
    QJsonObject group;
    QString gName = QString("group_%1").arg(groups.size() + 1);
    group["group_name"] = gName;
    group["name"]       = gName;
    group["enable"]     = false;
    group["checked"]    = false;
    group["actions"]    = QJsonArray();
    groups.append(group);
    w->setCurrentTaskGroups(groups);

    w->refreshGroupList();
}

void TaskChainCrudHandler::onGroupInsert(TaskChainWidget *w)
{
    if (!w) return;
    int idx = w->currentChainIndex();
    if (idx < 0) return;
    if (w->m_currentTaskIndex < 0) {
        QMessageBox::warning(w, QStringLiteral("提示"), QStringLiteral("请先选择一个任务"));
        return;
    }

    int row = w->m_groupList->currentRow();
    QJsonArray groups = w->currentTaskGroups();
    QJsonObject group;
    QString gName = QString("group_%1").arg(groups.size() + 1);
    group["group_name"] = gName;
    group["name"]       = gName;
    group["enable"]     = false;
    group["checked"]    = false;
    group["actions"]    = QJsonArray();

    if (row >= 0) {
        groups.insert(row, group);
    } else {
        groups.append(group);
    }
    w->setCurrentTaskGroups(groups);
    w->refreshGroupList();
}

void TaskChainCrudHandler::onGroupDelete(TaskChainWidget *w)
{
    if (!w) return;
    int idx = w->currentChainIndex();
    if (idx < 0 || w->m_currentTaskIndex < 0) return;

    auto selected = w->m_groupList->selectedItems();
    if (selected.isEmpty()) {
        QMessageBox::warning(w, QStringLiteral("提示"), QStringLiteral("请勾选需要删除的项"));
        return;
    }

    QList<int> rows;
    for (auto *item : selected) {
        rows << w->m_groupList->row(item);
    }
    std::sort(rows.begin(), rows.end(), std::greater<int>());

    QJsonArray groups = w->currentTaskGroups();
    for (int row : rows) {
        if (row >= 0 && row < groups.size()) {
            groups.removeAt(row);
        }
    }
    w->setCurrentTaskGroups(groups);
    w->refreshGroupList();
}

void TaskChainCrudHandler::onGroupSelectAll(TaskChainWidget *w)
{
    if (!w || !w->m_groupList) return;
    TaskChainListSync::setAllCheckBoxesChecked(w->m_groupList, !TaskChainListSync::allCheckBoxesChecked(w->m_groupList));
    w->updateGroupSelectBtn();
}
