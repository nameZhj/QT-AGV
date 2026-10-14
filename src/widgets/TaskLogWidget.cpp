#include "TaskLogWidget.h"
#include "core/ApiService.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QPainter>
#include <QPainterPath>
#include <QProxyStyle>
#include <QStyleOption>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStyledItemDelegate>
#include <QApplication>

namespace {
// 自定义树控件绘制样式：定制青蓝色展开/折叠三角指示器，去除冗余连线
class LogTreeStyle : public QProxyStyle
{
public:
    void drawPrimitive(PrimitiveElement element, const QStyleOption *option,
                       QPainter *painter, const QWidget *widget = nullptr) const override
    {
        if (element == PE_IndicatorBranch) {
            if (option->state & State_Children) {
                painter->save();
                painter->setRenderHint(QPainter::Antialiasing);
                QRect r = option->rect;
                qreal size = 8.0;
                qreal cx = r.center().x();
                qreal cy = r.center().y();

                QPainterPath path;
                if (option->state & State_Open) {
                    // 向下指示器 v
                    path.moveTo(cx - size / 2.0, cy - size / 4.0);
                    path.lineTo(cx + size / 2.0, cy - size / 4.0);
                    path.lineTo(cx, cy + size / 2.0);
                    path.closeSubpath();
                } else {
                    // 向右指示器 >
                    path.moveTo(cx - size / 4.0, cy - size / 2.0);
                    path.lineTo(cx + size / 2.0, cy);
                    path.lineTo(cx - size / 4.0, cy + size / 2.0);
                    path.closeSubpath();
                }
                painter->setPen(Qt::NoPen);
                painter->setBrush(QColor("#00a8ff"));
                painter->drawPath(path);
                painter->restore();
            }
            return;
        }
        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }
};

// 自定义项委托：绘制文字时锁定节点预设前景色，彻底杜绝点击选中或悬停时文字变黑或变色
class LogItemDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        // 提取文本并清空 opt.text，让底层样式引擎仅绘制背景高亮和选中矩形
        const QString text = opt.text;
        opt.text.clear();

        const QWidget *widget = opt.widget;
        QStyle *style = widget ? widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

        // 精确保持原节点颜色绘制文字，绝不因点击或选中而变黑/变色
        if (!text.isEmpty()) {
            painter->save();
            painter->setFont(opt.font);

            const QVariant fg = index.data(Qt::ForegroundRole);
            const QColor textColor = fg.isValid() ? fg.value<QBrush>().color() : QColor("#d4d4d4");
            painter->setPen(textColor);

            QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &option, widget);
            if (!textRect.isValid()) {
                textRect = option.rect.adjusted(6, 0, -6, 0);
            }
            painter->drawText(textRect, opt.displayAlignment | Qt::TextSingleLine, text);
            painter->restore();
        }
    }
};
}

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
    m_tree->setStyleSheet(
        "QTreeWidget {"
        "    background-color: #1e1e1e;"
        "    color: #a8a8a8;"
        "    border: none;"
        "    outline: none;"
        "    font-family: 'Consolas', 'Courier New', 'Microsoft YaHei', monospace;"
        "    font-size: 13px;"
        "}"
        "QTreeWidget::item {"
        "    min-height: 25px;"
        "    padding-top: 1px;"
        "    padding-bottom: 1px;"
        "    border-bottom: 1px solid #2d2d2d;"
        "}"
        "QTreeWidget::item:hover {"
        "    background-color: #282a2d;"
        "}"
        "QTreeWidget::item:selected {"
        "    background-color: #141a24;"
        "}"
        "QHeaderView::section {"
        "    background-color: #252526;"
        "    color: #9e9e9e;"
        "    border: none;"
        "    border-bottom: 1px solid #333333;"
        "    padding: 6px 10px;"
        "    font-weight: normal;"
        "    font-size: 13px;"
        "    font-family: 'Consolas', 'Courier New', 'Microsoft YaHei', monospace;"
        "}"
        "QScrollBar:vertical {"
        "    background: #1e1e1e;"
        "    width: 8px;"
        "    margin: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "    background: #3e3e42;"
        "    min-height: 20px;"
        "    border-radius: 4px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "    background: #4e4e52;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "    height: 0px;"
        "}"
    );

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
