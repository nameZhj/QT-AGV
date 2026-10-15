#include "UiHelper.h"
#include "SpinBoxWheelFilter.h"
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QToolButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QIcon>

QSpinBox* UiHelper::createSpinBox(int min, int max, int step, int value, QWidget *parent) {
    auto *sb = new QSpinBox(parent);
    sb->setRange(min, max);
    sb->setSingleStep(step);
    sb->setValue(value);
    sb->setFocusPolicy(Qt::StrongFocus);
    sb->installEventFilter(new SpinBoxWheelFilter(sb));
    return sb;
}

QDoubleSpinBox* UiHelper::createDoubleSpinBox(double min, double max, double step, int decimals, double value, QWidget *parent) {
    auto *sb = new QDoubleSpinBox(parent);
    sb->setRange(min, max);
    sb->setSingleStep(step);
    sb->setDecimals(decimals);
    sb->setValue(value);
    sb->setFocusPolicy(Qt::StrongFocus);
    sb->installEventFilter(new SpinBoxWheelFilter(sb));
    return sb;
}

QPushButton* UiHelper::createPushButton(const QString &text, const QString &iconPath, const QString &objectName, QWidget *parent) {
    auto *btn = new QPushButton(text, parent);
    if (!iconPath.isEmpty()) {
        btn->setIcon(QIcon(iconPath));
    }
    if (!objectName.isEmpty()) {
        btn->setObjectName(objectName);
    }
    btn->setCursor(Qt::PointingHandCursor);
    return btn;
}

QToolButton* UiHelper::createToolButton(const QString &text, const QString &iconPath, const QString &objectName, QWidget *parent) {
    auto *btn = new QToolButton(parent);
    btn->setText(text);
    if (!iconPath.isEmpty()) {
        btn->setIcon(QIcon(iconPath));
    }
    if (!objectName.isEmpty()) {
        btn->setObjectName(objectName);
    }
    btn->setCursor(Qt::PointingHandCursor);
    return btn;
}

void UiHelper::setupStandardTable(QTableWidget *table, const QStringList &headers, bool alternatingRowColors) {
    if (!table) return;
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setHighlightSections(false);
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(alternatingRowColors);
    table->setShowGrid(true);
}

QHBoxLayout* UiHelper::createFormRow(const QString &labelText, QWidget *fieldWidget, int labelWidth) {
    auto *layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto *label = new QLabel(labelText);
    if (labelWidth > 0) {
        label->setFixedWidth(labelWidth);
    }
    layout->addWidget(label);
    if (fieldWidget) {
        layout->addWidget(fieldWidget, 1);
    }
    return layout;
}
