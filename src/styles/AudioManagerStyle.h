/**
 * @file    AudioManagerStyle.h
 * @layer   GUILayer
 * @brief   Dark theme stylesheet constants and custom painted vector icons for AudioManagerDialog.
 * @deps    none
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QPolygonF>

namespace AudioManagerStyle {

inline const char *kListStyle =
    "QListWidget {"
    "  background: #0b1220;"
    "  border: 1px solid #2b3648;"
    "  border-radius: 3px;"
    "  color: #cbd5e1;"
    "  font-size: 12px;"
    "  outline: none;"
    "  padding: 4px;"
    "}"
    "QListWidget::item {"
    "  height: 27px;"
    "  padding-left: 6px;"
    "  border-radius: 2px;"
    "}"
    "QListWidget::item:hover {"
    "  background: #16233a;"
    "}"
    "QListWidget::item:selected {"
    "  background: #1d4ed8;"
    "  color: #ffffff;"
    "  border-left: 3px solid #60a5fa;"
    "}"
    "QScrollBar:vertical {"
    "  background: #0b1220;"
    "  width: 8px;"
    "  margin: 0px;"
    "}"
    "QScrollBar::handle:vertical {"
    "  background: #334155;"
    "  border-radius: 4px;"
    "  min-height: 24px;"
    "}"
    "QScrollBar::handle:vertical:hover {"
    "  background: #475569;"
    "}"
    "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
    "  height: 0px;"
    "}";

inline const char *kButtonStyle =
    "QPushButton {"
    "  background: #1f2937;"
    "  color: #dbe4ee;"
    "  border: 1px solid #334155;"
    "  border-radius: 3px;"
    "  padding: 4px 14px;"
    "  font-size: 12px;"
    "}"
    "QPushButton:hover {"
    "  background: #26344a;"
    "  border-color: #3b82f6;"
    "}"
    "QPushButton:pressed {"
    "  background: #172033;"
    "}"
    "QPushButton:disabled {"
    "  color: #5b6675;"
    "  border-color: #243044;"
    "  background: #161d2a;"
    "}";

inline const char *kLinkButtonStyle =
    "QPushButton {"
    "  background: transparent;"
    "  color: #4da3ff;"
    "  border: none;"
    "  padding: 4px 6px;"
    "  font-size: 12px;"
    "}"
    "QPushButton:hover {"
    "  color: #8ec5ff;"
    "}";

inline const char *kIconButtonStyle =
    "QToolButton {"
    "  background: transparent;"
    "  border: none;"
    "  border-radius: 3px;"
    "}"
    "QToolButton:hover {"
    "  background: #22304a;"
    "}";

inline QIcon makeRefreshIcon()
{
    const int sz = 20;
    QPixmap pm(sz, sz);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    const QColor col("#a855f7");
    const double c = sz * 0.5;
    QRectF arc(c - sz * 0.32, c - sz * 0.32, sz * 0.64, sz * 0.64);
    p.setPen(QPen(col, 1.8, Qt::SolidLine, Qt::RoundCap));
    p.setBrush(Qt::NoBrush);
    p.drawArc(arc, 70 * 16, 250 * 16);

    p.setPen(Qt::NoPen);
    p.setBrush(col);
    QPolygonF tri;
    tri << QPointF(c + sz * 0.32, c - sz * 0.06)
        << QPointF(c + sz * 0.32, c + sz * 0.20)
        << QPointF(c + sz * 0.08, c + sz * 0.10);
    p.drawPolygon(tri);
    p.end();
    return QIcon(pm);
}

inline QIcon makeTrashIcon()
{
    const int sz = 20;
    QPixmap pm(sz, sz);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    const QColor col("#9aa1ad");
    p.setPen(QPen(col, 1.4, Qt::SolidLine, Qt::RoundCap));
    p.setBrush(Qt::NoBrush);
    p.drawLine(QPointF(sz * 0.18, sz * 0.28), QPointF(sz * 0.82, sz * 0.28));
    p.drawLine(QPointF(sz * 0.38, sz * 0.28), QPointF(sz * 0.42, sz * 0.16));
    p.drawLine(QPointF(sz * 0.42, sz * 0.16), QPointF(sz * 0.58, sz * 0.16));
    p.drawLine(QPointF(sz * 0.58, sz * 0.16), QPointF(sz * 0.62, sz * 0.28));
    p.drawRoundedRect(QRectF(sz * 0.26, sz * 0.28, sz * 0.48, sz * 0.54), 2, 2);
    p.drawLine(QPointF(sz * 0.40, sz * 0.38), QPointF(sz * 0.40, sz * 0.72));
    p.drawLine(QPointF(sz * 0.50, sz * 0.38), QPointF(sz * 0.50, sz * 0.72));
    p.drawLine(QPointF(sz * 0.60, sz * 0.38), QPointF(sz * 0.60, sz * 0.72));
    p.end();
    return QIcon(pm);
}

} // namespace AudioManagerStyle
