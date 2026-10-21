/**
 * @file    CarWidget.cpp
 * @module  GUILayer
 * @brief   Implementation of robot chassis control panel and telemetry dashboard.
 * @deps    CarWidget.h, CarTableHelper.h, CarDialogHelper.h, RobotInfoManager.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "CarWidget.h"
#include "CarTableHelper.h"
#include "CarDialogHelper.h"
#include "network/ApiService.h"
#include "app/AppConfig.h"
#include "ui/common/LoadingDialog.h"
#include "ui/common/Toast.h"
#include "ui/common/UiHelper.h"
#include "core/RobotInfoManager.h"
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QTextEdit>
#include <QJsonDocument>
#include <QDateTime>
#include <QPointer>

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
            if (!idItem || idItem->text().trimmed() != carId.trimmed()) {
                continue;
            }

            auto *statusItem = m_table->item(row, 3);
            if (statusItem) {
                statusItem->setText(info.statusText());
                statusItem->setForeground(QBrush(QColor(info.statusColor())));
            }

            if (info.map_id != 0) {
                auto *mapItem = m_table->item(row, 2);
                if (mapItem && (mapItem->text().trimmed().isEmpty() || mapItem->text().startsWith("地图 ID:"))) {
                    const QString mname = resolveMapNameById(info.map_id);
                    mapItem->setText(!mname.isEmpty() ? mname : QString::number(info.map_id));
                }
            }
            break;
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
    UiHelper::initStandardTable(m_table, {
        {"设备编号", QHeaderView::Stretch},
        {"设备名称", QHeaderView::Stretch},
        {"所属地图", QHeaderView::Stretch},
        {"在线状态", QHeaderView::Stretch},
        {"链接状态", QHeaderView::Stretch},
        {"设备状态", QHeaderView::Stretch},
        {"描述",     QHeaderView::Stretch},
        {"操作",     QHeaderView::Stretch}
    });
    m_table->horizontalHeader()->setStretchLastSection(true);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &CarWidget::onTableSelectionChanged);
    mainLayout->addWidget(m_table);
}

// === [Section: UI Setup & Data Loading] ===
QString CarWidget::resolveMapNameById(int mapId) const
{
    return CarTableHelper::resolveMapNameById(m_mapOptions, mapId);
}

void CarWidget::loadMapOptions()
{
    QPointer<CarWidget> self = this;
    ApiService::instance().listMapAll(QJsonObject(), [self](int code, const QJsonObject &data) {
        if (!self || code != 200) return;
        self->m_mapOptions = data.value(QStringLiteral("rows")).toArray();
        self->m_mapFilter->clear();
        self->m_mapFilter->addItem(QStringLiteral("全部"), QString());
        for (const auto &item : self->m_mapOptions) {
            const QJsonObject obj = item.toObject();
            self->m_mapFilter->addItem(obj.value(QStringLiteral("mapName")).toString(), jsonIdStr(obj.value(QStringLiteral("mapId"))));
        }

        // 若当前列表中有未解析出名字的地图，用最新加载的地图选项尝试匹配替换
        for (int row = 0; row < self->m_table->rowCount(); ++row) {
            auto *mapItem = self->m_table->item(row, 2);
            if (!mapItem) continue;

            bool ok = false;
            const int mid = mapItem->text().trimmed().toInt(&ok);
            if (ok && mid > 0) {
                const QString mname = self->resolveMapNameById(mid);
                if (!mname.isEmpty()) {
                    mapItem->setText(mname);
                }
            }
        }
    });
}

void CarWidget::populateCarRow(int row, const QJsonObject &car)
{
    CarTableHelper::populateCarRow(this, m_table, row, car, m_mapOptions);
}

void CarWidget::getList()
{
    QJsonObject query;
    query[QStringLiteral("pageNum")]  = m_pageNum;
    query[QStringLiteral("pageSize")] = m_pageSize;
    if (!m_nameFilter->text().isEmpty()) query[QStringLiteral("carName")] = m_nameFilter->text();
    const QString mapId = m_mapFilter->currentData().toString();
    if (!mapId.isEmpty()) query[QStringLiteral("mapId")] = mapId;
    const QString status = m_statusFilter->currentData().toString();
    if (!status.isEmpty()) query[QStringLiteral("status")] = status;

    QPointer<CarWidget> self = this;
    ApiService::instance().listCar(query, [self](int code, const QJsonObject &data) {
        if (!self || code != 200) return;
        self->m_carList = data.value(QStringLiteral("rows")).toArray();
        self->m_total   = data.value(QStringLiteral("total")).toInt();
        self->m_table->setRowCount(self->m_carList.size());

        for (int i = 0; i < self->m_carList.size(); ++i) {
            self->populateCarRow(i, self->m_carList[i].toObject());
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
    CarTableHelper::handleDelete(this);
}
void CarWidget::onLink()
{
    int row = m_table->currentRow();
    if (row >= 0 && row < m_carList.size()) showLinkDialog(m_carList[row].toObject());
}

void CarWidget::onTableSelectionChanged()
{
    bool sel = m_table->currentRow() >= 0;
    m_editBtn->setEnabled(sel);
    m_deleteBtn->setEnabled(sel);
}

void CarWidget::onStatusChanged(int row)
{
    CarTableHelper::handleStatusChanged(this, row);
}

void CarWidget::showAddDialog()
{
    CarDialogHelper::showAddDialog(this);
}

void CarWidget::showEditDialog(const QJsonObject &carData)
{
    CarDialogHelper::showEditDialog(this, carData);
}

void CarWidget::showLinkDialog(const QJsonObject &carData)
{
    CarDialogHelper::showLinkDialog(this, carData);
}

void CarWidget::checkCertification(const QString &ckey)
{
    CarDialogHelper::checkCertification(this, ckey);
}
