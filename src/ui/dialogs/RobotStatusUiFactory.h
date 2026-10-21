/**
 * @file    RobotStatusUiFactory.h
 * @layer   GUILayer
 * @brief   Widget factory for status badges, rows, and tab placeholders in RobotStatusDialog.
 * @deps    QLabel, QGridLayout, QPair, QList
 * @note    Refactored for AI readability; runtime behavior & visual styling invariant.
 */

#pragma once

#include <QString>
#include <QList>
#include <QPair>

class QLabel;
class QGridLayout;
class QWidget;

enum class RobotStatusBadgeType {
    Green,   // 绿色正常
    Orange,  // 橙色警戒
    Red,     // 红色告警
    Gray     // 灰色中性/未生效/--
};

class RobotStatusUiFactory
{
public:
    static QLabel *createBadge(const QString &text, RobotStatusBadgeType type);
    static void updateBadge(QLabel *lbl, const QString &text, RobotStatusBadgeType type);

    static void addRow(QGridLayout *grid, int row, const QString &label, const QString &value,
                       QLabel **outValueLbl = nullptr, const QString &valColor = "#e2e8f0");
    static void addBadgeRow(QGridLayout *grid, int row, const QString &label, const QString &badgeText,
                            RobotStatusBadgeType type, QLabel **outBadgeLbl = nullptr);
    static void addMultiBadgeRow(QGridLayout *grid, int row, const QString &label,
                                 const QList<QPair<QString, RobotStatusBadgeType>> &badges,
                                 QList<QLabel*> *outBadgeList = nullptr);

    static QWidget *createPlaceholderTab(const QString &tabName);
};
