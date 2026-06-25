#ifndef TASKLOGWIDGET_H
#define TASKLOGWIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QJsonObject>
#include <QJsonArray>

class TaskLogWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TaskLogWidget(QWidget *parent = nullptr);

signals:
    void navigateRequested(const QString &target, const QString &id);

private slots:
    void onSearch();
    void onReset();
    void onTableSelectionChanged();

private:
    void setupUI();
    void getList();
    void showDetail(const QJsonObject &row);
    void showMap(const QJsonObject &row);

    QTableWidget *m_table = nullptr;
    QLineEdit *m_carNameFilter = nullptr;
    QLineEdit *m_taskNameFilter = nullptr;
    QComboBox *m_statusFilter = nullptr;
    QLineEdit *m_taskNumberFilter = nullptr;
    QLineEdit *m_messageFilter = nullptr;

    QJsonArray m_taskList;
    int m_pageNum = 1;
    int m_pageSize = 10;
    int m_total = 0;
};

#endif // TASKLOGWIDGET_H
