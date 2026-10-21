/**
 * @file    RobotStatusUiFactory.cpp
 * @layer   GUILayer
 * @brief   Widget factory for status badges, rows, and tab placeholders in RobotStatusDialog.
 * @deps    RobotStatusUiFactory.h
 * @note    Refactored for AI readability; runtime behavior & visual styling invariant.
 */

#include "RobotStatusUiFactory.h"

#include <QLabel>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>

QLabel *RobotStatusUiFactory::createBadge(const QString &text, RobotStatusBadgeType type) {
  auto *b = new QLabel(text);
  b->setAlignment(Qt::AlignCenter);
  updateBadge(b, text, type);
  b->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  return b;
}

void RobotStatusUiFactory::updateBadge(QLabel *lbl, const QString &text, RobotStatusBadgeType type) {
  if (!lbl) return;
  lbl->setText(text);

  QString bg = "#65a30d"; // 绿底正常
  if (type == RobotStatusBadgeType::Orange) {
    bg = "#ea580c";     // 橙底警戒
  } else if (type == RobotStatusBadgeType::Red) {
    bg = "#dc2626";     // 红底报警
  } else if (type == RobotStatusBadgeType::Gray) {
    bg = "rgba(255, 255, 255, 0.12)"; // 灰色中性/未生效
  }

  lbl->setStyleSheet(QString(
      "background: %1; color: #ffffff; border-radius: 3px; padding: 2px 7px; font-size: 11px; font-weight: bold;"
  ).arg(bg));
}

void RobotStatusUiFactory::addRow(QGridLayout *grid, int row, const QString &label,
                                 const QString &value, QLabel **outValueLbl,
                                 const QString &valColor) {
  auto *l = new QLabel(label);
  l->setStyleSheet("color: #94a3b8; font-size: 12px;");
  l->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  grid->addWidget(l, row, 0);

  auto *v = new QLabel(value);
  v->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: 500;").arg(valColor));
  v->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  v->setTextInteractionFlags(Qt::TextSelectableByMouse);
  grid->addWidget(v, row, 1);

  if (outValueLbl) {
    *outValueLbl = v;
  }
}

void RobotStatusUiFactory::addBadgeRow(QGridLayout *grid, int row, const QString &label,
                                      const QString &badgeText, RobotStatusBadgeType type,
                                      QLabel **outBadgeLbl) {
  auto *l = new QLabel(label);
  l->setStyleSheet("color: #94a3b8; font-size: 12px;");
  l->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  grid->addWidget(l, row, 0);

  auto *b = createBadge(badgeText, type);
  grid->addWidget(b, row, 1, Qt::AlignLeft | Qt::AlignVCenter);

  if (outBadgeLbl) {
    *outBadgeLbl = b;
  }
}

void RobotStatusUiFactory::addMultiBadgeRow(QGridLayout *grid, int row, const QString &label,
                                           const QList<QPair<QString, RobotStatusBadgeType>> &badges,
                                           QList<QLabel*> *outBadgeList) {
  auto *l = new QLabel(label);
  l->setStyleSheet("color: #94a3b8; font-size: 12px;");
  l->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  grid->addWidget(l, row, 0);

  auto *hl = new QHBoxLayout;
  hl->setContentsMargins(0, 0, 0, 0);
  hl->setSpacing(6);
  for (const auto &pair : badges) {
    auto *b = createBadge(pair.first, pair.second);
    hl->addWidget(b);
    if (outBadgeList) {
      outBadgeList->append(b);
    }
  }
  hl->addStretch();
  grid->addLayout(hl, row, 1, Qt::AlignLeft | Qt::AlignVCenter);
}

QWidget *RobotStatusUiFactory::createPlaceholderTab(const QString &tabName) {
  auto *w = new QWidget;
  auto *l = new QVBoxLayout(w);
  l->setAlignment(Qt::AlignCenter);

  auto *lbl = new QLabel(QString("暂无「%1」相关数据").arg(tabName));
  lbl->setStyleSheet("color: #64748b; font-size: 14px; font-weight: 500;");
  l->addWidget(lbl);

  return w;
}
