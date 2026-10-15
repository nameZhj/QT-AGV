#include "CarWidget.h"
#include "network/ApiService.h"
#include "app/AppConfig.h"
#include "ui/common/LoadingDialog.h"
#include "ui/common/Toast.h"
#include "core/RobotInfoManager.h"
#include <QPushButton>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QTextEdit>
#include <QJsonDocument>
#include <QDateTime>

CarWidget::CarWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    loadMapOptions();
    getList();

    connect(&RobotInfoManager::instance(), &RobotInfoManager::robotInfoUpdated,
            this, [this](const QString &carId, const RobotInfo &info) {
        for (int row = 0; row < m_table->rowCount(); ++row) {
            auto *idItem = m_table->item(row, 0);
            if (idItem && idItem->text().trimmed() == carId.trimmed()) {
                auto *statusItem = m_table->item(row, 3);
                if (statusItem) {
                    statusItem->setText(info.statusText());
                    statusItem->setForeground(QBrush(QColor(info.statusColor())));
                }
                if (info.map_id != 0) {
                    auto *mapItem = m_table->item(row, 2);
                    if (mapItem && (mapItem->text().trimmed().isEmpty() || mapItem->text().startsWith("地图 ID:"))) {
                        QString mname;
                        for (const auto &m : m_mapOptions) {
                            QJsonObject obj = m.toObject();
                            if (jsonIdStr(obj.value("mapId")) == QString::number(info.map_id) ||
                                obj.value("mapId").toInt() == info.map_id) {
                                mname = obj.value("mapName").toString().trimmed();
                                break;
                            }
                        }
                        mapItem->setText(!mname.isEmpty() ? mname : QString::number(info.map_id));
                    }
                }
                break;
            }
        }
    });
}

void CarWidget::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    // ── Search bar ─────────────────────────────────────────────────────────
    auto *searchLayout = new QHBoxLayout;
    searchLayout->setSpacing(8);

    searchLayout->addWidget(new QLabel("所属地图:"));
    m_mapFilter = new QComboBox;
    m_mapFilter->setMinimumWidth(140);
    searchLayout->addWidget(m_mapFilter);

    searchLayout->addWidget(new QLabel("设备名称:"));
    m_nameFilter = new QLineEdit;
    m_nameFilter->setPlaceholderText("请输入设备名称");
    searchLayout->addWidget(m_nameFilter);

    searchLayout->addWidget(new QLabel("设备状态:"));
    m_statusFilter = new QComboBox;
    m_statusFilter->addItem("全部", "");
    m_statusFilter->addItem("正常", "0");
    m_statusFilter->addItem("停用", "2");
    searchLayout->addWidget(m_statusFilter);

    auto *searchBtn = new QPushButton("搜索");
    auto *resetBtn  = new QPushButton("重置");
    connect(searchBtn, &QPushButton::clicked, this, &CarWidget::onSearch);
    connect(resetBtn,  &QPushButton::clicked, this, &CarWidget::onReset);
    searchLayout->addWidget(searchBtn);
    searchLayout->addWidget(resetBtn);
    searchLayout->addStretch();

    mainLayout->addLayout(searchLayout);

    // ── Action buttons ─────────────────────────────────────────────────────
    auto *btnLayout = new QHBoxLayout;
    m_addBtn    = new QPushButton("新增");
    m_editBtn   = new QPushButton("修改");
    m_deleteBtn = new QPushButton("删除");
    m_editBtn->setEnabled(false);
    m_deleteBtn->setEnabled(false);
    connect(m_addBtn,    &QPushButton::clicked, this, &CarWidget::onAdd);
    connect(m_editBtn,   &QPushButton::clicked, this, &CarWidget::onEdit);
    connect(m_deleteBtn, &QPushButton::clicked, this, &CarWidget::onDelete);
    btnLayout->addWidget(m_addBtn);
    btnLayout->addWidget(m_editBtn);
    btnLayout->addWidget(m_deleteBtn);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    // ── Table ──────────────────────────────────────────────────────────────
    m_table = new QTableWidget;
    m_table->setColumnCount(8);
    m_table->setHorizontalHeaderLabels({"设备编号","设备名称","所属地图","在线状态","链接状态","设备状态","描述","操作"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &CarWidget::onTableSelectionChanged);
    mainLayout->addWidget(m_table);
}

void CarWidget::loadMapOptions()
{
    ApiService::instance().listMapAll(QJsonObject(), [this](int code, const QJsonObject &data) {
        if (code != 200) return;
        m_mapOptions = data.value("rows").toArray();
        m_mapFilter->clear();
        m_mapFilter->addItem("全部", "");
        for (const auto &item : m_mapOptions) {
            QJsonObject obj = item.toObject();
            m_mapFilter->addItem(obj.value("mapName").toString(), jsonIdStr(obj.value("mapId")));
        }

        // 若当前列表中有未解析出名字的地图，用最新加载的地图选项尝试匹配替换
        for (int row = 0; row < m_table->rowCount(); ++row) {
            auto *mapItem = m_table->item(row, 2);
            if (mapItem) {
                QString txt = mapItem->text().trimmed();
                bool ok = false;
                int mid = txt.toInt(&ok);
                if (ok && mid > 0) {
                    for (const auto &m : m_mapOptions) {
                        QJsonObject obj = m.toObject();
                        if (jsonIdStr(obj.value("mapId")) == QString::number(mid) ||
                            obj.value("mapId").toInt() == mid) {
                            mapItem->setText(obj.value("mapName").toString().trimmed());
                            break;
                        }
                    }
                }
            }
        }
    });
}

void CarWidget::getList()
{
    QJsonObject query;
    query["pageNum"]  = m_pageNum;
    query["pageSize"] = m_pageSize;
    if (!m_nameFilter->text().isEmpty()) query["carName"] = m_nameFilter->text();
    QString mapId = m_mapFilter->currentData().toString();
    if (!mapId.isEmpty()) query["mapId"] = mapId;
    QString status = m_statusFilter->currentData().toString();
    if (!status.isEmpty()) query["status"] = status;

    ApiService::instance().listCar(query, [this](int code, const QJsonObject &data) {
        if (code != 200) return;
        m_carList = data.value("rows").toArray();
        m_total   = data.value("total").toInt();
        m_table->setRowCount(m_carList.size());

        for (int i = 0; i < m_carList.size(); ++i) {
            QJsonObject car     = m_carList[i].toObject();
            QJsonObject carInfo = car.value("carInfo").toObject();

            QString cid = jsonIdStr(car.value("carId"));
            QString mapName = car.value("mapName").toString();
            QString statusText = carInfo.value("status").toString() == "0" ? "在线" : "离线";
            QColor statusCol = carInfo.value("status").toString() == "0" ? QColor("#22c55e") : QColor("#64748b");

            if (RobotInfoManager::instance().hasInfoForCar(cid)) {
                RobotInfo rInfo = RobotInfoManager::instance().infoForCar(cid);
                statusText = rInfo.statusText();
                statusCol = QColor(rInfo.statusColor());
                if (mapName.isEmpty() && rInfo.map_id != 0) {
                    QString mname;
                    for (const auto &m : m_mapOptions) {
                        QJsonObject obj = m.toObject();
                        if (jsonIdStr(obj.value("mapId")) == QString::number(rInfo.map_id) ||
                            obj.value("mapId").toInt() == rInfo.map_id) {
                            mname = obj.value("mapName").toString().trimmed();
                            break;
                        }
                    }
                    mapName = !mname.isEmpty() ? mname : QString::number(rInfo.map_id);
                }
            }

            m_table->setItem(i, 0, new QTableWidgetItem(cid));
            m_table->setItem(i, 1, new QTableWidgetItem(car.value("carName").toString()));
            m_table->setItem(i, 2, new QTableWidgetItem(mapName));
            auto *stItem = new QTableWidgetItem(statusText);
            stItem->setForeground(QBrush(statusCol));
            m_table->setItem(i, 3, stItem);
            m_table->setItem(i, 4, new QTableWidgetItem(
                // 参考 Vue index.vue: linkStatus==='0' 表示已链接, '1' 表示未链接
                car.value("linkStatus").toString() == "0" ? "已链接" : "未链接"));

            // Status combo
            auto *statusWidget = new QWidget;
            auto *statusLayout = new QHBoxLayout(statusWidget);
            statusLayout->setContentsMargins(4, 0, 4, 0);
            auto *statusCombo = new QComboBox;
            statusCombo->addItem("正常", "0");
            statusCombo->addItem("停用", "2");
            statusCombo->setCurrentIndex(car.value("status").toString() == "0" ? 0 : 1);
            connect(statusCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, [this, i](int) { onStatusChanged(i); });
            statusLayout->addWidget(statusCombo);
            m_table->setCellWidget(i, 5, statusWidget);

            m_table->setItem(i, 6, new QTableWidgetItem(car.value("remark").toString()));

            // Action buttons
            auto *opWidget = new QWidget;
            auto *opLayout = new QHBoxLayout(opWidget);
            opLayout->setContentsMargins(4, 2, 4, 2);
            opLayout->setSpacing(4);
            auto *editBtn = new QPushButton("修改");
            auto *delBtn  = new QPushButton("删除");
            auto *linkBtn = new QPushButton("链接");

            // 参考 Vue index.vue: 已链接(linkStatus=="0") 或设备停用(status!="0") 时禁用"链接"按钮
            bool isLinked  = car.value("linkStatus").toString() == "0";
            bool isEnabled = car.value("status").toString() == "0";
            linkBtn->setEnabled(!isLinked && isEnabled);
            if (isLinked) {
                linkBtn->setToolTip("设备已链接");
            } else if (!isEnabled) {
                linkBtn->setToolTip("设备已停用，无法链接");
            }

            connect(editBtn, &QPushButton::clicked, this, [this, i] {
                showEditDialog(m_carList[i].toObject());
            });
            connect(delBtn, &QPushButton::clicked, this, [this, i] {
                (void)i; onDelete();
            });
            connect(linkBtn, &QPushButton::clicked, this, [this, i] {
                showLinkDialog(m_carList[i].toObject());
            });
            opLayout->addWidget(editBtn);
            opLayout->addWidget(delBtn);
            opLayout->addWidget(linkBtn);
            m_table->setCellWidget(i, 7, opWidget);
        }
    });
}

void CarWidget::onSearch()  { m_pageNum = 1; getList(); }

void CarWidget::onReset()
{
    m_nameFilter->clear();
    m_mapFilter->setCurrentIndex(0);
    m_statusFilter->setCurrentIndex(0);
    m_pageNum = 1;
    getList();
}

void CarWidget::onAdd()    { showAddDialog(); }
void CarWidget::onEdit()
{
    int row = m_table->currentRow();
    if (row >= 0) showEditDialog(m_carList[row].toObject());
}
void CarWidget::onDelete()
{
    int row = m_table->currentRow();
    if (row < 0) return;
    QJsonObject car = m_carList[row].toObject();
    if (car.value("status").toString() != "2") {
        QMessageBox::warning(this, "提示", "设备未停用，无法删除！");
        return;
    }
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("确认");
    msgBox.setText(QString("确认删除设备「%1」？").arg(car.value("carName").toString()));
    QPushButton *yesBtn = msgBox.addButton("确定", QMessageBox::YesRole);
    msgBox.addButton("取消", QMessageBox::NoRole);
    msgBox.exec();
    if (msgBox.clickedButton() == yesBtn) {
        auto *loading = new LoadingDialog("正在删除设备...", this);
        loading->show();
        ApiService::instance().delCar(jsonIdStr(car.value("carId")), [this, loading](int code, const QJsonObject &) {
            loading->close();
            loading->deleteLater();
            if (code == 200) { Toast::success(this, "删除成功"); getList(); }
        });
    }
}
void CarWidget::onLink()
{
    int row = m_table->currentRow();
    if (row >= 0) showLinkDialog(m_carList[row].toObject());
}

void CarWidget::onTableSelectionChanged()
{
    bool sel = m_table->currentRow() >= 0;
    m_editBtn->setEnabled(sel);
    m_deleteBtn->setEnabled(sel);
}

void CarWidget::onStatusChanged(int row)
{
    QJsonObject car = m_carList[row].toObject();
    auto *w     = m_table->cellWidget(row, 5);
    auto *combo = w->findChild<QComboBox *>();
    if (!combo) return;

    QString newStatus = combo->currentData().toString();
    QString text = newStatus == "0" ? "启用" : "停用";
    QMessageBox msgBox2(this);
    msgBox2.setWindowTitle("确认");
    msgBox2.setText(QString("确认要「%1」设备「%2」吗？").arg(text, car.value("carName").toString()));
    QPushButton *yesBtn2 = msgBox2.addButton("确定", QMessageBox::YesRole);
    msgBox2.addButton("取消", QMessageBox::NoRole);
    msgBox2.exec();
    if (msgBox2.clickedButton() == yesBtn2) {
        auto *loading = new LoadingDialog("正在更改设备状态...", this);
        loading->show();
        ApiService::instance().changeCarStatus(
            jsonIdStr(car.value("carId")), jsonIdStr(car.value("mapId")), newStatus,
            [this, loading, row, newStatus](int code, const QJsonObject &) {
                loading->close();
                loading->deleteLater();
                if (code == 200) {
                    Toast::success(this, "状态已更新");
                } else {
                    auto *w2 = m_table->cellWidget(row, 5);
                    auto *c2 = w2->findChild<QComboBox *>();
                    if (c2) c2->setCurrentIndex(newStatus == "0" ? 1 : 0);
                }
            });
    }
}

void CarWidget::showAddDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle("添加设备");
    auto *form = new QFormLayout(&dlg);

    auto *mapCombo = new QComboBox;
    for (const auto &item : m_mapOptions)
        mapCombo->addItem(item.toObject().value("mapName").toString(),
                          jsonIdStr(item.toObject().value("mapId")));

    auto *nameEdit    = new QLineEdit;
    auto *ipPortEdit  = new QLineEdit;
    ipPortEdit->setPlaceholderText("示例: 192.168.1.100:8888");
    auto *certKeyEdit = new QLineEdit;
    certKeyEdit->setPlaceholderText("请输入认证 Key");

    auto *robotTypeCombo = new QComboBox;
    robotTypeCombo->addItem("室内", "0");
    robotTypeCombo->addItem("室外", "1");

    auto *machineryCombo = new QComboBox;
    machineryCombo->addItem("普通", "0");
    machineryCombo->addItem("顶升", "1");
    machineryCombo->addItem("潜伏", "2");

    auto *remarkEdit = new QTextEdit;
    remarkEdit->setMaximumHeight(60);

    form->addRow("地图名称:", mapCombo);
    form->addRow("设备名称:", nameEdit);
    form->addRow("通讯地址:", ipPortEdit);
    form->addRow("认证 Key:", certKeyEdit);
    form->addRow("类型:",     robotTypeCombo);
    form->addRow("机械属性:", machineryCombo);
    form->addRow("描述:",     remarkEdit);

    connect(certKeyEdit, &QLineEdit::editingFinished, this, [this, certKeyEdit] {
        checkCertification(certKeyEdit->text().trimmed());
    });

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    btns->button(QDialogButtonBox::Ok)->setText("确定");
    btns->button(QDialogButtonBox::Cancel)->setText("取消");
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(btns);

    if (dlg.exec() == QDialog::Accepted) {
        if (nameEdit->text().isEmpty() || ipPortEdit->text().isEmpty()) {
            QMessageBox::warning(this, "提示", "设备名称和通讯地址不能为空");
            return;
        }
        if (!m_certKeyStatus) {
            QMessageBox::warning(this, "提示", "认证失败，请检查认证 Key");
            return;
        }
        QJsonObject data;
        data["carName"]         = nameEdit->text();
        data["ipPort"]          = ipPortEdit->text();
        data["mapId"]           = mapCombo->currentData().toString();
        data["robotType"]       = robotTypeCombo->currentData().toString();
        data["machinery"]       = machineryCombo->currentData().toString();
        data["remark"]          = remarkEdit->toPlainText();
        data["certificationId"] = m_certId;

        auto *loading = new LoadingDialog("正在新增设备...", this);
        loading->show();
        ApiService::instance().addCar(data, [this, loading](int code, const QJsonObject &) {
            loading->close();
            loading->deleteLater();
            if (code == 200) { Toast::success(this, "新增成功"); getList(); }
        });
    }
}

void CarWidget::showEditDialog(const QJsonObject &carData)
{
    QDialog dlg(this);
    dlg.setWindowTitle("修改设备");
    auto *form = new QFormLayout(&dlg);

    auto *mapCombo = new QComboBox;
    for (const auto &item : m_mapOptions)
        mapCombo->addItem(item.toObject().value("mapName").toString(),
                          jsonIdStr(item.toObject().value("mapId")));

    auto *nameEdit   = new QLineEdit(carData.value("carName").toString());
    auto *ipPortEdit = new QLineEdit(carData.value("ipPort").toString());
    auto *remarkEdit = new QTextEdit(carData.value("remark").toString());
    remarkEdit->setMaximumHeight(60);

    form->addRow("地图名称:", mapCombo);
    form->addRow("设备名称:", nameEdit);
    form->addRow("通讯地址:", ipPortEdit);
    form->addRow("描述:",     remarkEdit);

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    btns->button(QDialogButtonBox::Ok)->setText("确定");
    btns->button(QDialogButtonBox::Cancel)->setText("取消");
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(btns);

    if (dlg.exec() == QDialog::Accepted) {
        QJsonObject data;
        data["carId"]   = carData.value("carId");
        data["carName"] = nameEdit->text();
        data["ipPort"]  = ipPortEdit->text();
        data["mapId"]   = mapCombo->currentData().toString();
        data["remark"]  = remarkEdit->toPlainText();
        // 与 Vue car/details.vue 的 submitForm 一致，显式刷新为当前时间
        data["updateTime"] = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

        auto *loading = new LoadingDialog("正在保存设备信息...", this);
        loading->show();
        ApiService::instance().updateCar(data, [this, loading](int code, const QJsonObject &) {
            loading->close();
            loading->deleteLater();
            if (code == 200) { Toast::success(this, "修改成功"); getList(); }
        });
    }
}

void CarWidget::showLinkDialog(const QJsonObject &carData)
{
    QDialog dlg(this);
    dlg.setWindowTitle("链接设备");
    auto *form = new QFormLayout(&dlg);

    auto *certKeyEdit = new QLineEdit;
    certKeyEdit->setPlaceholderText("请输入认证 Key");
    form->addRow("认证 Key:", certKeyEdit);

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    btns->button(QDialogButtonBox::Ok)->setText("确定");
    btns->button(QDialogButtonBox::Cancel)->setText("取消");
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(btns);

    if (dlg.exec() == QDialog::Accepted) {
        QJsonObject data;
        data["carId"]   = carData.value("carId");
        data["carName"] = carData.value("carName");
        data["mapId"]   = carData.value("mapId");
        QJsonObject cert;
        cert["certificationKey"] = certKeyEdit->text().trimmed();
        data["certification"] = cert;

        auto *loading = new LoadingDialog("正在链接设备...", this);
        loading->show();
        ApiService::instance().linkCar(data, [this, loading](int code, const QJsonObject &) {
            loading->close();
            loading->deleteLater();
            if (code == 200) Toast::success(this, "已申请链接");
        });
    }
}

void CarWidget::checkCertification(const QString &ckey)
{
    if (ckey.isEmpty() || ckey == m_certOldKey) return;
    m_certId.clear();
    ApiService::instance().checkCertification(ckey, [this, ckey](int code, const QJsonObject &data) {
        m_certKeyStatus = (code == 200);
        if (code == 200) m_certId = jsonIdStr(data.value("certificationId"));
        m_certOldKey = ckey;
    });
}
