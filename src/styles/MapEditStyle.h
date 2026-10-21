/**
 * @file    MapEditStyle.h
 * @module  GUILayer
 * @brief   Static styling constants, vector icon generators, and tooltip helpers for map editing.
 * @deps    QColor, QPixmap, QIcon, QPainter, QToolTip, QEvent
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QString>
#include <QColor>
#include <QPixmap>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QRectF>
#include <QToolTip>
#include <QCursor>
#include <QWidget>
#include <QEvent>

namespace MapEditStyle {

// ==============================================================================
// === [Section: Static QSS Style Constants] ===
// ==============================================================================

inline constexpr const char *S_TOOLBAR =
    "background:#191e2b; border-right:1px solid #262f42;";

inline constexpr const char *S_TOOLBTN =
    "QPushButton{"
    "  background:transparent;"
    "  border:none; border-radius:6px; padding:0;"
    "}"
    "QPushButton:hover{"
    "  background:rgba(255,255,255,0.08);"
    "}"
    "QPushButton:checked{"
    "  background:#1296DB;"
    "}";

inline constexpr const char *S_TOPBAR =
    "background:#1b212f; border-bottom:1px solid #262e3f;";

inline constexpr const char *S_RIGHTPANEL =
    "background:#151822; border-left:1px solid #262e3f;";

inline constexpr const char *S_CARD =
    "QGroupBox{"
    "  font-weight:bold; font-size:11px; color:#5a789a;"
    "  border:1px solid #263348; border-radius:6px;"
    "  margin-top:10px; padding:4px; background:#1b2230;"
    "}"
    "QGroupBox::title{"
    "  subcontrol-origin:margin; left:8px;"
    "  padding:0 4px; background:#151822; color:#7090b0;"
    "}"
    "QLabel{background:transparent; color:#8fa6bf; font-size:12px;}"
    "QLineEdit,QComboBox,QDoubleSpinBox{"
    "  border:1px solid #2b384e; border-radius:4px;"
    "  padding:2px 6px; background:#131722; color:#cfddea; font-size:12px;"
    "}"
    "QLineEdit:focus,QComboBox:focus,QDoubleSpinBox:focus{border:1px solid #1296DB;}"
    "QComboBox QAbstractItemView{background:#171d2a;color:#cfddea;selection-background-color:#1296DB;border:1px solid #2b384e;}"
    "QCheckBox{color:#8fa6bf;font-size:12px;spacing:6px;}"
    "QCheckBox::indicator{width:14px;height:14px;border:1px solid #32435c;border-radius:3px;background:#131722;}"
    "QCheckBox::indicator:checked{background:#1296DB;border:1px solid #1296DB;}"
    "QPushButton{"
    "  border:1px solid #2b384e; border-radius:4px;"
    "  padding:4px 8px; font-size:12px; background:#1a2230; color:#8fa6bf;"
    "}"
    "QPushButton:hover{background:#253044; color:#fff;}";

inline constexpr const char *S_TABLE =
    "QTableWidget{"
    "  background:#1a2638; border:none; gridline-color:transparent;"
    "  color:#a8c0d8; font-size:12px; outline:0;"
    "}"
    "QHeaderView::section{"
    "  background:#131d2a; color:#4a7098;"
    "  border:none; border-bottom:1px solid rgba(255,255,255,0.07);"
    "  padding:5px 8px; font-size:11px; font-weight:bold;"
    "}"
    "QTableWidget::item{"
    "  padding:5px 8px;"
    "  border-bottom:1px solid rgba(255,255,255,0.04);"
    "}"
    "QTableWidget::item:selected{"
    "  background:rgba(41,128,185,0.38); color:white;"
    "}"
    "QTableWidget::item:hover:!selected{"
    "  background:rgba(100,160,220,0.10);"
    "}"
    "QScrollBar:vertical{background:#131d2a;width:6px;border:none;}"
    "QScrollBar::handle:vertical{background:#2a4060;border-radius:3px;min-height:20px;}"
    "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}";

// ==============================================================================
// === [Section: Tooltip Formatter & Vector Icon Helpers] ===
// ==============================================================================

inline QString ttHtml(const QString &title, const QString &body)
{
    QString b = body;
    b.replace('\n', QLatin1String("<br>"));
    return QString("<div style='min-width:150px;'>"
                   "<div style='font-size:13px;font-weight:600;color:#ddeeff;"
                   "margin-bottom:3px;'>%1</div>"
                   "<div style='color:#90adc8;font-size:11px;line-height:1.6;'>%2</div>"
                   "</div>").arg(title, b);
}

QPixmap drawToolIcon(const QString &name, const QColor &col, int sz = 22);

QIcon makeToolIcon(const QString &name);

// ==============================================================================
// === [Section: InstantTip Tooltip Event Filter] ===
// ==============================================================================

struct InstantTip : public QObject {
    using QObject::QObject;
    bool eventFilter(QObject *o, QEvent *e) override {
        if (e->type() == QEvent::Enter) {
            auto *w = qobject_cast<QWidget*>(o);
            if (w && !w->toolTip().isEmpty()) {
                QToolTip::showText(QCursor::pos() + QPoint(4, 18), w->toolTip(), w);
            }
        } else if (e->type() == QEvent::Leave) {
            QToolTip::hideText();
        }
        return false;
    }
};

} // namespace MapEditStyle

// Export to global scope for convenient unqualified usage in UI widgets
using namespace MapEditStyle;
