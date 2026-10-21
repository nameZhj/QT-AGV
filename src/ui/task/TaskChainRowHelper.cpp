/**
 * @file    TaskChainRowHelper.cpp
 * @layer   GUI
 * @brief   Row widget factory implementations for task chain and sub-task cards.
 * @deps    TaskChainRowHelper.h, TaskChainWidget.h, TaskChainDragWidgets.h, ApiService.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "TaskChainRowHelper.h"
#include "TaskChainWidget.h"
#include "TaskChainDragWidgets.h"
#include "TaskChainStyle.h"
#include "network/ApiService.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QCheckBox>
#include <QTextEdit>
#include <QInputDialog>
#include <QMessageBox>
#include <QMouseEvent>

class ChainCardWidget : public QWidget
{
public:
    explicit ChainCardWidget(int index, std::function<void(int)> onSelect, QWidget *parent = nullptr)
        : QWidget(parent), m_index(index), m_onSelect(std::move(onSelect))
    {
        setAttribute(Qt::WA_StyledBackground, true);
    }
    void setIndex(int idx) { m_index = idx; }
    int index() const { return m_index; }
protected:
    void mousePressEvent(QMouseEvent *event) override {
        QWidget::mousePressEvent(event);
        if (event->button() == Qt::LeftButton && m_onSelect) {
            m_onSelect(m_index);
        }
    }
private:
    int m_index;
    std::function<void(int)> m_onSelect;
};

void TaskChainRowHelper::buildChainRowWidget(TaskChainWidget *owner,
                                             QListWidgetItem *item,
                                             int chainIndex)
{
    if (!owner || !item || chainIndex < 0 || chainIndex >= owner->m_chains.size()) return;
    const QString &name = owner->m_chains[chainIndex].name;

    auto *card = new ChainCardWidget(chainIndex, [owner](int idx) {
        owner->selectChain(idx);
    }, owner->m_chainList);
    card->setObjectName("chainCardRoot");

    auto *mainHBox = new QHBoxLayout(card);
    mainHBox->setContentsMargins(0, 0, 8, 0);
    mainHBox->setSpacing(8);

    auto *indicator = new QWidget(card);
    indicator->setObjectName("chainIndicator");
    indicator->setFixedWidth(4);
    indicator->setStyleSheet("background-color: transparent; border-top-left-radius: 4px; border-bottom-left-radius: 4px;");
    mainHBox->addWidget(indicator);

    auto *contentVBox = new QVBoxLayout;
    contentVBox->setContentsMargins(2, 6, 0, 6);
    contentVBox->setSpacing(2);

    auto *btnRow = new QHBoxLayout;
    btnRow->setContentsMargins(0, 0, 0, 0);
    btnRow->setSpacing(4);
    btnRow->addStretch(1);

    const QString btnQss =
        "QToolButton { background: #22262d; border: 1px solid #363d48; border-radius: 4px; padding: 2px; }"
        "QToolButton:hover { background: #2e343f; border-color: #556071; }"
        "QToolButton:pressed { background: #181b20; border-color: #00a8ff; }";

    auto *exportBtn = new QToolButton(card);
    exportBtn->setIcon(QIcon(":/chain-export.svg"));
    exportBtn->setIconSize(QSize(15, 15));
    exportBtn->setToolTip(QStringLiteral("导出当前任务链到指定位置"));
    exportBtn->setAutoRaise(true);
    exportBtn->setFixedSize(24, 24);
    exportBtn->setCursor(Qt::PointingHandCursor);
    exportBtn->setStyleSheet(btnQss);
    btnRow->addWidget(exportBtn);

    auto *renameBtn = new QToolButton(card);
    renameBtn->setIcon(QIcon(":/chain-rename.svg"));
    renameBtn->setIconSize(QSize(15, 15));
    renameBtn->setToolTip(QStringLiteral("重命名任务链"));
    renameBtn->setAutoRaise(true);
    renameBtn->setFixedSize(24, 24);
    renameBtn->setCursor(Qt::PointingHandCursor);
    renameBtn->setStyleSheet(btnQss);
    btnRow->addWidget(renameBtn);

    auto *deleteBtn = new QToolButton(card);
    deleteBtn->setIcon(QIcon(":/chain-delete.svg"));
    deleteBtn->setIconSize(QSize(15, 15));
    deleteBtn->setToolTip(QStringLiteral("删除任务链"));
    deleteBtn->setAutoRaise(true);
    deleteBtn->setFixedSize(24, 24);
    deleteBtn->setCursor(Qt::PointingHandCursor);
    deleteBtn->setStyleSheet(btnQss);
    btnRow->addWidget(deleteBtn);

    contentVBox->addLayout(btnRow);

    auto *nameRow = new QHBoxLayout;
    nameRow->setContentsMargins(0, 0, 0, 0);
    auto *nameLabel = new QLabel(name, card);
    nameLabel->setObjectName("chainNameLabel");
    nameLabel->setStyleSheet("color: #d6dbe2; font-size: 12px; font-weight: bold; background: transparent;");
    nameLabel->setToolTip(name);
    nameRow->addWidget(nameLabel);
    nameRow->addStretch(1);
    contentVBox->addLayout(nameRow);

    mainHBox->addLayout(contentVBox, 1);

    QObject::connect(exportBtn, &QToolButton::clicked, owner, [owner, chainIndex]() {
        owner->selectChain(chainIndex);
        owner->onChainExport();
    });

    QObject::connect(renameBtn, &QToolButton::clicked, owner, [owner, chainIndex, nameLabel]() {
        if (chainIndex < 0 || chainIndex >= owner->m_chains.size()) return;
        bool ok = false;
        QString newName = QInputDialog::getText(owner, QStringLiteral("重命名任务链"),
            QStringLiteral("输入新名称:"), QLineEdit::Normal, owner->m_chains[chainIndex].name, &ok);
        if (ok && !newName.trimmed().isEmpty()) {
            owner->m_chains[chainIndex].name = newName.trimmed();
            nameLabel->setText(newName.trimmed());
            nameLabel->setToolTip(newName.trimmed());
            owner->scheduleCacheWrite();
        }
    });

    QObject::connect(deleteBtn, &QToolButton::clicked, owner, [owner, chainIndex]() {
        if (chainIndex < 0 || chainIndex >= owner->m_chains.size()) return;
        if (QMessageBox::question(owner, QStringLiteral("确认"),
                QStringLiteral("确定删除 [%1] 任务链吗？").arg(owner->m_chains[chainIndex].name)) != QMessageBox::Yes)
            return;

        QString chainName = owner->m_chains[chainIndex].name;
        owner->m_chains.removeAt(chainIndex);
        owner->refreshChainList();
        if (owner->m_chains.isEmpty()) {
            owner->m_currentChainIndex = -1;
            owner->m_currentTaskIndex = -1;
            owner->refreshTaskList();
            owner->refreshGroupList();
        } else {
            int nextIndex = qBound(0, chainIndex, owner->m_chains.size() - 1);
            owner->selectChain(nextIndex);
        }

        QString fileName = chainName;
        if (!fileName.endsWith(".json", Qt::CaseInsensitive)) fileName += ".json";
        ApiService::instance().deleteTaskChain(fileName, [owner, fileName](int code, const QJsonObject &) {
            if (code == 200 && owner) {
                owner->updateStatus(QStringLiteral("已删除服务端任务链文件: %1").arg(fileName));
            }
        });
    });

    item->setSizeHint(QSize(0, 56));
    owner->m_chainList->setItemWidget(item, card);
}

void TaskChainRowHelper::buildTaskRowWidget(TaskChainWidget *owner,
                                           QListWidgetItem *item,
                                           int taskIndex,
                                           const QJsonObject &task)
{
    if (!owner || !item) return;
    QString subTaskName = task.value("sub_task_name").toString();
    if (subTaskName.isEmpty()) subTaskName = task.value("name").toString();
    if (subTaskName.isEmpty()) subTaskName = QString("sub_task_%1").arg(taskIndex + 1);

    QString note = task.value("note").toString();
    bool checked = task.contains("enable") ? task.value("enable").toBool() : task.value("checked").toBool();

    auto *row = new QWidget;
    row->setObjectName("taskCardRoot");
    row->setStyleSheet(kTaskCardRoot);
    row->setAcceptDrops(true);
    row->installEventFilter(new CardLongPressDragFilter("application/x-taskchain-task-reorder", taskIndex, row));
    auto *vbox = new QVBoxLayout(row);
    vbox->setContentsMargins(4, 4, 4, 4);
    vbox->setSpacing(2);

    auto *topRow = new QHBoxLayout;
    topRow->setSpacing(6);
    auto *checkBox = new QCheckBox;
    checkBox->setChecked(checked);
    checkBox->setStyleSheet(kCheckBoxStyle);
    topRow->addWidget(checkBox);

    auto *nameLabel = new QLabel(subTaskName);
    nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    nameLabel->setStyleSheet("color:#00bcd4; font-size:13px; font-weight:bold; background:transparent;");
    topRow->addWidget(nameLabel);
    topRow->addStretch();
    vbox->addLayout(topRow);

    auto *noteEdit = new QTextEdit(note);
    noteEdit->setPlaceholderText(QStringLiteral("备注..."));
    noteEdit->setMinimumHeight(50);
    noteEdit->setStyleSheet(kNoteEditStyle);

    int idx = owner->currentChainIndex();
    QObject::connect(checkBox, &QCheckBox::toggled, owner, [owner, idx, taskIndex](bool state) {
        if (idx < 0 || idx >= owner->m_chains.size()) return;
        if (taskIndex < 0 || taskIndex >= owner->m_chains[idx].tasks.size()) return;
        QJsonObject t = owner->m_chains[idx].tasks[taskIndex].toObject();
        t["enable"]  = state;
        t["checked"] = state;
        owner->m_chains[idx].tasks[taskIndex] = t;
        owner->updateTaskSelectBtn();
        owner->scheduleCacheWrite();
    });

    QObject::connect(noteEdit, &QTextEdit::textChanged, owner, [owner, idx, taskIndex, noteEdit, item, nameLabel]() {
        if (idx < 0 || idx >= owner->m_chains.size()) return;
        if (taskIndex < 0 || taskIndex >= owner->m_chains[idx].tasks.size()) return;
        QJsonObject t = owner->m_chains[idx].tasks[taskIndex].toObject();
        t["note"] = noteEdit->toPlainText();
        owner->m_chains[idx].tasks[taskIndex] = t;
        owner->scheduleCacheWrite();

        int lineCount = noteEdit->document()->size().height() / noteEdit->fontMetrics().lineSpacing();
        int h = qMax(60, qMin(200, lineCount * noteEdit->fontMetrics().lineSpacing() + 14));
        if (noteEdit->height() != h) {
            noteEdit->setFixedHeight(h);
            item->setSizeHint(QSize(0, nameLabel->height() + h + 32));
        }
    });
    vbox->addWidget(noteEdit);

    int initH = qMax(60, noteEdit->document()->size().height() > 0 ?
        (int)(noteEdit->document()->size().height() + 14) : 60);
    item->setSizeHint(QSize(0, nameLabel->sizeHint().height() + initH + 32));
    owner->m_taskList->setItemWidget(item, row);
}
