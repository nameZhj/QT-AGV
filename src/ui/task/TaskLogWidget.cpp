/**
 * @file    TaskLogWidget.cpp
 * @module  GUILayer
 * @brief   Implementation of real-time execution log tree widget.
 * @deps    TaskLogWidget.h, QTreeWidget
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "TaskLogWidget.h"
#include <QTreeWidget>
#include "network/ApiService.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include "TaskLogStyle.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

TaskLogWidget::TaskLogWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    refreshData();
}

void TaskLogWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── 树形列表（完全参考效果图，全屏纯树形结构展示） ──
    m_tree = new QTreeWidget(this);
    m_tree->setStyle(new LogTreeStyle);
    m_tree->setItemDelegate(new LogItemDelegate(m_tree));
    m_tree->setColumnCount(2);
    m_tree->setHeaderLabels({"Key / Time", "Value"});

    // 列宽与自适应
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tree->setColumnWidth(0, 380);
    m_tree->setIndentation(18);
    m_tree->setAnimated(true);
    m_tree->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setAlternatingRowColors(false);

    // 深色终端风格样式
    m_tree->setStyleSheet(TaskLogStyle::kLogTreeStyleSheet);

    mainLayout->addWidget(m_tree);
}

void TaskLogWidget::refreshData()
{
    ApiService::instance().getTaskChainLog([this](int code, const QJsonObject &resp) {
        if (code == 200) {
            const QJsonValue dataVal = resp.value("data");
            if (dataVal.isArray()) {
                loadLogData(dataVal.toArray());
            } else if (dataVal.isObject()) {
                loadLogData(QJsonArray{dataVal.toObject()});
            } else {
                m_tree->clear();
            }
            return;
        }
        m_tree->clear();
    });
}

void TaskLogWidget::loadLogData(const QJsonArray &data)
{
    m_tree->clear();
    for (const auto &val : data) {
        if (!val.isObject()) continue;
        const QJsonObject obj = val.toObject();
        if (obj.contains("rootNodes") && obj.value("rootNodes").isArray()) {
            const QJsonArray rootNodes = obj.value("rootNodes").toArray();
            for (const auto &rVal : rootNodes) {
                if (rVal.isObject()) {
                    parseJsonNode(rVal.toObject(), nullptr);
                }
            }
        } else {
            parseJsonNode(obj, nullptr);
        }
    }
}

void TaskLogWidget::parseJsonNode(const QJsonObject &nodeObj, QTreeWidgetItem *parentItem)
{
    // 提取除了 level, children, rootNodes 之外的业务键值
    QString keyText;
    QString valText;

    const int level = nodeObj.value("level").toInt(parentItem ? (parentItem->data(0, Qt::UserRole).toInt() + 1) : 0);

    for (auto it = nodeObj.constBegin(); it != nodeObj.constEnd(); ++it) {
        const QString k = it.key();
        if (k == "level" || k == "children" || k == "rootNodes") {
            continue;
        }

        const QJsonValue v = it.value();
        QString s;
        if (v.isString()) {
            s = v.toString();
        } else if (v.isDouble()) {
            s = QString::number(v.toDouble(), 'g', 15);
        } else if (v.isBool()) {
            s = v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        } else if (v.isNull()) {
            s = QString();
        } else if (v.isObject()) {
            s = QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
        } else if (v.isArray()) {
            s = QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
        }

        if (keyText.isEmpty()) {
            keyText = k;
            valText = s;
        } else {
            keyText += " | " + k;
            valText += " | " + s;
        }
    }

    // 若当前节点无其他字段（兜底处理）
    if (keyText.isEmpty()) {
        keyText = QStringLiteral("Node (L%1)").arg(level);
    }

    auto *item = parentItem ? new QTreeWidgetItem(parentItem) : new QTreeWidgetItem(m_tree);
    item->setText(0, keyText);
    item->setText(1, valText);
    item->setToolTip(0, keyText);
    item->setToolTip(1, valText);
    item->setData(0, Qt::UserRole, level);

    // 语义化终端着色：
    // - 包含“完成”：高亮亮绿色 (#38e168)
    // - 包含“取消/失败/错误”：亮红警示 (#f87171)
    // - TaskStart 节点：科技青蓝 (#00a8ff)
    // - 其他常规参数/数值：终端浅灰 (#d4d4d4)
    QColor color("#d4d4d4");
    const QString combined = keyText + " " + valText;
    if (combined.contains("完成")) {
        color = QColor("#38e168");
    } else if (combined.contains("取消") || combined.contains("失败") || combined.contains("错误")) {
        color = QColor("#f87171");
    } else if (keyText == "TaskStart" || combined.contains("【任务开始】")) {
        color = QColor("#00a8ff");
    }

    item->setForeground(0, color);
    item->setForeground(1, color);

    // 递归解析任意深度子节点 (无限层级支持)
    if (nodeObj.contains("children") && nodeObj.value("children").isArray()) {
        const QJsonArray children = nodeObj.value("children").toArray();
        for (const auto &cVal : children) {
            if (cVal.isObject()) {
                parseJsonNode(cVal.toObject(), item);
            }
        }
    }

    // 默认折叠子节点
    item->setExpanded(false);
}
