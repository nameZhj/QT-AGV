#include "TaskLogWidget.h"
#include "core/ApiService.h"
#include "core/AppConfig.h"
#include <QPushButton>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QLabel>
#include <QDateTimeEdit>
#include <QJsonDocument>

TaskLogWidget::TaskLogWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    getList();
}

void TaskLogWidget::setupUI()
{
    QWidget *rightWidget = new QWidget(this);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightWidget);

    // 搜索栏 — 两行，防止在小窗口下按钮被遮挡
    QVBoxLayout *searchAreaLayout = new QVBoxLayout;
    searchAreaLayout->setSpacing(6);

    QHBoxLayout *searchRow1 = new QHBoxLayout;
    searchRow1->addWidget(new QLabel("执行设备:"));
    m_carNameFilter = new QLineEdit;
    m_carNameFilter->setPlaceholderText("设备名称");
    searchRow1->addWidget(m_carNameFilter);

    searchRow1->addWidget(new QLabel("任务名称:"));
    m_taskNameFilter = new QLineEdit;
    m_taskNameFilter->setPlaceholderText("任务名称");
    searchRow1->addWidget(m_taskNameFilter);

    searchRow1->addWidget(new QLabel("任务状态:"));
    m_statusFilter = new QComboBox;
    m_statusFilter->addItem("全部", "");
    m_statusFilter->addItem("执行中", "0");
    m_statusFilter->addItem("已完成", "1");
    m_statusFilter->addItem("结束", "2");
    m_statusFilter->addItem("暂停", "3");
    searchRow1->addWidget(m_statusFilter);
    searchAreaLayout->addLayout(searchRow1);

    QHBoxLayout *searchRow2 = new QHBoxLayout;
    searchRow2->addWidget(new QLabel("任务编号:"));
    m_taskNumberFilter = new QLineEdit;
    m_taskNumberFilter->setPlaceholderText("任务编号");
    searchRow2->addWidget(m_taskNumberFilter);

    searchRow2->addWidget(new QLabel("任务消息:"));
    m_messageFilter = new QLineEdit;
    m_messageFilter->setPlaceholderText("任务消息");
    searchRow2->addWidget(m_messageFilter);

    searchRow2->addStretch();
    QPushButton *searchBtn = new QPushButton("搜索");
    QPushButton *resetBtn = new QPushButton("重置");
    connect(searchBtn, &QPushButton::clicked, this, &TaskLogWidget::onSearch);
    connect(resetBtn, &QPushButton::clicked, this, &TaskLogWidget::onReset);
    searchRow2->addWidget(searchBtn);
    searchRow2->addWidget(resetBtn);
    searchAreaLayout->addLayout(searchRow2);

    rightLayout->addLayout(searchAreaLayout);

    // 表格
    m_table = new QTableWidget;
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({"日志编号", "执行设备", "任务编号", "任务名称", "任务状态", "任务时间", "任务消息"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_table, &QTableWidget::itemClicked, this, [this](QTableWidgetItem *item) {
        int row = item->row();
        if (row >= 0 && row < m_taskList.size()) {
            showDetail(m_taskList[row].toObject());
        }
    });
    rightLayout->addWidget(m_table);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(rightWidget);
}

void TaskLogWidget::getList()
{
    QJsonObject query;
    query["pageNum"] = m_pageNum;
    query["pageSize"] = m_pageSize;
    if (!m_carNameFilter->text().isEmpty()) query["carName"] = m_carNameFilter->text();
    if (!m_taskNameFilter->text().isEmpty()) query["taskName"] = m_taskNameFilter->text();
    if (!m_taskNumberFilter->text().isEmpty()) query["taskNumber"] = m_taskNumberFilter->text();
    if (!m_messageFilter->text().isEmpty()) query["message"] = m_messageFilter->text();
    QString status = m_statusFilter->currentData().toString();
    if (!status.isEmpty()) query["status"] = status;

    ApiService::instance().listTasklog(query, [this](int code, const QJsonObject &data) {
        if (code != 200) return;
        m_taskList = data.value("rows").toArray();
        m_total = data.value("total").toInt();

        m_table->setRowCount(m_taskList.size());
        for (int i = 0; i < m_taskList.size(); ++i) {
            QJsonObject log = m_taskList[i].toObject();

            m_table->setItem(i, 0, new QTableWidgetItem(jsonIdStr(log.value("tasklogId"))));
            m_table->setItem(i, 1, new QTableWidgetItem(log.value("carName").toString()));
            m_table->setItem(i, 2, new QTableWidgetItem(log.value("taskNumber").toString()));
            m_table->setItem(i, 3, new QTableWidgetItem(log.value("taskName").toString()));

            // 状态
            QString status = log.value("status").toString();
            QString statusText;
            if (status == "0") statusText = "执行中";
            else if (status == "1") statusText = "已完成";
            else if (status == "2") statusText = "结束";
            else if (status == "3") statusText = "暂停";
            else statusText = status;
            m_table->setItem(i, 4, new QTableWidgetItem(statusText));

            // 时间
            QString timeRange = log.value("createTime").toString();
            if (!log.value("completedTime").toString().isEmpty()) {
                timeRange += " - " + log.value("completedTime").toString();
            }
            m_table->setItem(i, 5, new QTableWidgetItem(timeRange));
            m_table->setItem(i, 6, new QTableWidgetItem(log.value("message").toString()));
        }
    });
}

void TaskLogWidget::onSearch()
{
    m_pageNum = 1;
    getList();
}

void TaskLogWidget::onReset()
{
    m_carNameFilter->clear();
    m_taskNameFilter->clear();
    m_taskNumberFilter->clear();
    m_messageFilter->clear();
    m_statusFilter->setCurrentIndex(0);
    m_pageNum = 1;
    getList();
}

void TaskLogWidget::onTableSelectionChanged()
{
}

void TaskLogWidget::showDetail(const QJsonObject &row)
{
    QString tasklogId = jsonIdStr(row.value("tasklogId"));
    ApiService::instance().getTasklog(tasklogId, [this, row](int code, const QJsonObject &data) {
        if (code != 200) return;

        QJsonObject form = data.value("data").toObject(data);

        QDialog dlg(this);
        dlg.setWindowTitle("操作日志详细");
        QFormLayout *formLayout = new QFormLayout(&dlg);

        formLayout->addRow("设备名称:", new QLabel(form.value("carName").toString()));
        formLayout->addRow("任务名称:", new QLabel(form.value("taskName").toString()));
        formLayout->addRow("任务编号:", new QLabel(form.value("taskNumber").toString()));

        QString status = form.value("status").toString();
        QString statusText = status == "0" ? "执行中" : status == "1" ? "已完成" : status == "2" ? "结束" : "暂停";
        formLayout->addRow("任务状态:", new QLabel(statusText));
        formLayout->addRow("任务消息:", new QLabel(form.value("message").toString()));
        formLayout->addRow("创建时间:", new QLabel(form.value("createTime").toString()));
        formLayout->addRow("创建者:", new QLabel(form.value("createBy").toString()));
        formLayout->addRow("结束时间:", new QLabel(form.value("completedTime").toString()));
        formLayout->addRow("结束者:", new QLabel(form.value("completedBy").toString()));

        // 获取循环次数
        QString taskId = jsonIdStr(form.value("taskId"));
        if (!taskId.isEmpty()) {
            ApiService::instance().getTaskInfo(taskId, [formLayout](int code, const QJsonObject &taskData) {
                if (code == 200) {
                    QString taskJson = taskData.value("task_json").toString();
                    if (!taskJson.isEmpty()) {
                        QJsonDocument doc = QJsonDocument::fromJson(taskJson.toUtf8());
                        int execCount = doc.object().value("taskInfo").toObject().value("executionCount").toInt(0);
                        int currentCount = doc.object().value("taskInfo").toObject().value("currentCount").toInt(0);
                        formLayout->insertRow(4, "循环次数:", new QLabel(QString("%1/%2").arg(currentCount).arg(execCount)));
                    }
                }
            });
        }

        QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Close);
        btns->button(QDialogButtonBox::Close)->setText("关闭");
        connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::accept);
        formLayout->addRow(btns);

        dlg.exec();
    });
}

void TaskLogWidget::showMap(const QJsonObject &row)
{
    QString taskId = jsonIdStr(row.value("taskId"));
    if (taskId.isEmpty()) return;
    ApiService::instance().getTask(taskId, [this](int code, const QJsonObject &data) {
        if (code != 200) return;
        QJsonObject task = data.value("data").toObject();
        QString mapId = jsonIdStr(task.value("mapId"));
        emit navigateRequested("task_edit_map", mapId);
    });
}
