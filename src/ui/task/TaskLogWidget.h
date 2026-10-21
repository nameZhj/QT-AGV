/**
 * @file    TaskLogWidget.h
 * @module  GUILayer
 * @brief   Real-time execution log tree widget with status icons and filtering.
 * @deps    QWidget, QTreeWidget
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QWidget>
#include <QJsonArray>
#include <QJsonObject>

class QTreeWidget;
class QTreeWidgetItem;

class TaskLogWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TaskLogWidget(QWidget *parent = nullptr);
    ~TaskLogWidget() override = default;

    // 刷新任务日志列表(由 MainWindow 在切换菜单时调用)
    void refreshData();

signals:
    void navigateRequested(const QString &target, const QString &id);

private:
    void setupUI();
    void loadLogData(const QJsonArray &data);
    void parseJsonNode(const QJsonObject &nodeObj, QTreeWidgetItem *parentItem = nullptr);

    QTreeWidget *m_tree = nullptr;
};
