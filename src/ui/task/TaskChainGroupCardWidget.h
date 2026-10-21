/**
 * @file    TaskChainGroupCardWidget.h
 * @layer   GUI
 * @brief   Card widget representing a group item and its actions in TaskChain.
 * @deps    QWidget, QListWidgetItem, QJsonObject
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QWidget>
#include <QJsonObject>

class QListWidgetItem;
class QVBoxLayout;
class TaskChainWidget;

class TaskChainGroupCardWidget : public QWidget
{
    Q_OBJECT
public:
    static void buildGroupRowWidget(TaskChainWidget *owner,
                                    QListWidgetItem *item,
                                    int groupIndex,
                                    bool checked);

private:
    explicit TaskChainGroupCardWidget(TaskChainWidget *owner,
                                     QListWidgetItem *item,
                                     int groupIndex,
                                     bool checked,
                                     QWidget *parent = nullptr);

    void setupUI(int groupIndex, bool checked);
    void setupHeader(QVBoxLayout *vbox, int groupIndex, bool checked, const QString &groupName, const QString &actionType);
    void setupActions(QVBoxLayout *vbox, int groupIndex, const QJsonObject &group);

    void onEditActionParam(int groupIndex, int actionIndex, const QString &actionName);
    void onDeleteAction(int groupIndex, int actionIndex);
    void onDeleteGroup(int groupIndex);

    TaskChainWidget *m_owner = nullptr;
    QListWidgetItem *m_item = nullptr;
};
