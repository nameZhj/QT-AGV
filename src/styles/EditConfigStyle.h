/**
 * @file    EditConfigStyle.h
 * @layer   GUILayer
 * @brief   Custom QStyledItemDelegate, vector toolbar icon painters, and QSS style constants for EditConfigWidget.
 * @deps    QStyledItemDelegate, QPainter, QIcon
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QStyledItemDelegate>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QIcon>
#include <QTimer>

class ParamItemDelegate : public QStyledItemDelegate
{
public:
    explicit ParamItemDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QWidget *editor = QStyledItemDelegate::createEditor(parent, option, index);
        if (QLineEdit *lineEdit = qobject_cast<QLineEdit*>(editor)) {
            lineEdit->setStyleSheet("QLineEdit { background: #1a2330; border: 1.5px solid #4a9eff; border-radius: 4px; color: #ffffff; padding: 4px 8px; font-size: 13px; selection-background-color: #1e4080; }");
            QTimer::singleShot(0, lineEdit, &QLineEdit::selectAll);
        }
        return editor;
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        QStyledItemDelegate::setEditorData(editor, index);
        if (QLineEdit *lineEdit = qobject_cast<QLineEdit*>(editor)) {
            lineEdit->selectAll();
        }
    }

    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override
    {
        if (QLineEdit *lineEdit = qobject_cast<QLineEdit*>(editor)) {
            QString text = lineEdit->text().trimmed();
            model->setData(index, text, Qt::EditRole);
        } else {
            QStyledItemDelegate::setModelData(editor, model, index);
        }
    }
};

namespace EditConfigStyle {

inline QPixmap drawCfgToolIcon(const QString &name, const QColor &col, int sz = 20) {
    QPixmap pm(sz, sz);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    double c = sz * 0.5;

    if (name == "save") {
        p.setPen(QPen(col, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPainterPath cp;
        cp.moveTo(sz * 0.14, sz * 0.50);
        cp.lineTo(sz * 0.42, sz * 0.80);
        cp.lineTo(sz * 0.86, sz * 0.20);
        p.drawPath(cp);
    } else if (name == "download") {
        p.setPen(QPen(col, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(c, sz * 0.10), QPointF(c, sz * 0.62));
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        QPolygonF arrow;
        arrow << QPointF(sz * 0.32, sz * 0.50) << QPointF(sz * 0.68, sz * 0.50)
              << QPointF(c, sz * 0.78);
        p.drawPolygon(arrow);
        p.setPen(QPen(col, 1.6));
        p.drawLine(QPointF(sz * 0.16, sz * 0.88), QPointF(sz * 0.84, sz * 0.88));
    } else if (name == "upload") {
        p.setPen(QPen(col, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(c, sz * 0.34), QPointF(c, sz * 0.88));
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        QPolygonF arrow;
        arrow << QPointF(sz * 0.32, sz * 0.44) << QPointF(sz * 0.68, sz * 0.44)
              << QPointF(c, sz * 0.16);
        p.drawPolygon(arrow);
        p.setPen(QPen(col, 1.6));
        p.drawLine(QPointF(sz * 0.16, sz * 0.10), QPointF(sz * 0.84, sz * 0.10));
    }

    p.end();
    return pm;
}

inline QIcon makeCfgToolIcon(const QString &name) {
    QIcon icon;
    icon.addPixmap(drawCfgToolIcon(name, QColor("#9ab0c8")), QIcon::Normal, QIcon::Off);
    icon.addPixmap(drawCfgToolIcon(name, QColor("white")), QIcon::Active, QIcon::Off);
    return icon;
}

inline const char *kToolBtnStyle =
    "QToolButton{"
    "  background:#252a35; color:#c8d0d8; border:1px solid #3a3f4a;"
    "  border-radius:4px; padding:4px 12px; font-size:11px; font-weight:600;"
    "}"
    "QToolButton:hover{background:#2d3340; border-color:#4a9eff;}"
    "QToolButton:pressed{background:#1e4080;}"
    "QToolButton:disabled{color:#445566; border-color:#2d3340;}";

inline const char *kToolBtnPrimary =
    "QToolButton{"
    "  background:#1e6fd4; color:#fff; border:none;"
    "  border-radius:4px; padding:4px 12px; font-size:11px; font-weight:600;"
    "}"
    "QToolButton:hover{background:#2980e8;}"
    "QToolButton:pressed{background:#155db8;}";

inline const char *kParamTableStyle =
    "QTableWidget::item { padding: 8px 10px; }"
    "QLineEdit {"
    "  background: #1a2330; border: 1px solid #4a9eff; border-radius: 4px;"
    "  color: #e0e8f0; padding: 4px 8px; font-size: 13px;"
    "  selection-background-color: #1e4080;"
    "}";

} // namespace EditConfigStyle
