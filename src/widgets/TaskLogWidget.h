#ifndef TASKLOGWIDGET_H
#define TASKLOGWIDGET_H

#include <QWidget>
#include <QTreeWidget>

class TaskLogWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TaskLogWidget(QWidget *parent = nullptr);
    ~TaskLogWidget() override = default;

    // 刷新任务日志列表(由 MainWindow 在切换菜单时调用)
    void refreshData();

signals:
    void navigateRequested(const QString &target, const QString &id);

private:
    void setupUI();
    void buildSampleData();

    QTreeWidget *m_tree = nullptr;
};

#endif // TASKLOGWIDGET_H
