#pragma once

#include <QString>
#include <QStringList>

class QWidget;
class QPushButton;
class QToolButton;
class QSpinBox;
class QDoubleSpinBox;
class QTableWidget;
class QLayout;
class QHBoxLayout;
class QLabel;

/**
 * @brief 通用 UI 控件与表单构建工厂，集中封装控件创建、样式与过滤器挂载
 */
class UiHelper
{
public:
    UiHelper() = delete;

    /**
     * @brief 创建标准整数步进器 (自动挂载滚轮防误触过滤器)
     */
    static QSpinBox* createSpinBox(int min, int max, int step = 1, int value = 0, QWidget *parent = nullptr);

    /**
     * @brief 创建标准浮点步进器 (自动挂载滚轮防误触过滤器)
     */
    static QDoubleSpinBox* createDoubleSpinBox(double min, double max, double step = 0.1, int decimals = 2, double value = 0.0, QWidget *parent = nullptr);

    /**
     * @brief 创建标准按钮 (设置手型光标与可选图标)
     */
    static QPushButton* createPushButton(const QString &text, const QString &iconPath = QString(), const QString &objectName = QString(), QWidget *parent = nullptr);

    /**
     * @brief 创建标准工具栏按钮
     */
    static QToolButton* createToolButton(const QString &text, const QString &iconPath = QString(), const QString &objectName = QString(), QWidget *parent = nullptr);

    /**
     * @brief 规范初始化 QTableWidget (表头、列宽自适应、选择模式、交替行底色)
     */
    static void setupStandardTable(QTableWidget *table, const QStringList &headers, bool alternatingRowColors = true);

    /**
     * @brief 创建带标签的表单水平布局行
     */
    static QHBoxLayout* createFormRow(const QString &labelText, QWidget *fieldWidget, int labelWidth = 100);
};
