/**
 * @file    TaskLogStyle.h
 * @layer   GUIStyles
 * @brief   Custom proxy style, item delegate, and stylesheets for task execution log tree.
 * @deps    QProxyStyle, QStyledItemDelegate, QPainter
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QProxyStyle>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOption>
#include <QApplication>

namespace TaskLogStyle {

inline constexpr const char *kLogTreeStyleSheet =
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
    "}";

} // namespace TaskLogStyle

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
