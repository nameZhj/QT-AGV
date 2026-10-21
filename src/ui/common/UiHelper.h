/**
 * @file    UiHelper.h
 * @module  GUILayer
 * @brief   Standardized widget factory for buttons, spin boxes, and formatted table headers.
 * @deps    QWidget, QPushButton, QSpinBox
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QString>
#include <QStringList>

class QWidget;
class QPushButton;
class QToolButton;
class QSpinBox;
class QDoubleSpinBox;
class QTableWidget;
class QHBoxLayout;
#include <QHeaderView>
#include <QList>

struct TableColumnSpec {
    QString title;
    QHeaderView::ResizeMode resizeMode = QHeaderView::ResizeToContents;
    int fixedWidth = 0;
};

/**
 * @brief 通用 UI 控件与表单构建工厂，集中封装控件创建、样式与过滤器挂载
 */
class UiHelper
{
public:
    UiHelper() = delete;

    // === [Section: SpinBox Factory Methods] ===
    static QSpinBox* createSpinBox(int min, int max, int step = 1, int value = 0, QWidget *parent = nullptr);
    static QDoubleSpinBox* createDoubleSpinBox(double min, double max, double step = 0.1, int decimals = 2, double value = 0.0, QWidget *parent = nullptr);

    // === [Section: Button Factory Methods] ===
    static QPushButton* createPushButton(const QString &text, const QString &iconPath = QString(), const QString &objectName = QString(), QWidget *parent = nullptr);
    static QToolButton* createToolButton(const QString &text, const QString &iconPath = QString(), const QString &objectName = QString(), QWidget *parent = nullptr);

    // === [Section: Table Setup & Layout Helpers] ===
    static void setupStandardTable(QTableWidget *table, const QStringList &headers, bool alternatingRowColors = true);
    static void setupTableColumns(QTableWidget *table, const QList<TableColumnSpec> &columns);
    static void initStandardTable(QTableWidget *table, const QList<TableColumnSpec> &columns, bool alternatingRowColors = true);
    static QHBoxLayout* createFormRow(const QString &labelText, QWidget *fieldWidget, int labelWidth = 100);
};

