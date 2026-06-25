#include "MapWidget.h"
#include "core/ApiService.h"
#include "core/AppConfig.h"
#include "core/WebSocketService.h"
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QInputDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QFileDialog>
#include <QJsonDocument>
#include <QMenu>
#include <QAction>
#include <QProgressDialog>
#include <QHttpMultiPart>
#include <QFile>
#include <QDesktopServices>

MapWidget::MapWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    getList();

    connect(&WebSocketService::instance(), &WebSocketService::notificationReceived, this,
            [this](const QJsonObject &msg) {
        QString type = msg.value("type").toString();
        if (type == "pcd" || type == "agv") {
            QString status = msg.value("status").toString();
            if (status == "0" || status == "3" || msg.value("msgStatus").toString() == "error") {
                getList();
            }
        }
    });
}

void MapWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // 工具栏
    QHBoxLayout *toolbar = new QHBoxLayout;
    m_addBtn = new QPushButton("新增地图");
    m_addBtn->setStyleSheet("QPushButton { padding: 6px 16px; background: #007bff; color: white; border: none; border-radius: 4px; }");
    m_refreshBtn = new QPushButton("刷新");
    m_refreshBtn->setStyleSheet("QPushButton { padding: 6px 16px; }");
    connect(m_addBtn, &QPushButton::clicked, this, &MapWidget::onAdd);
    connect(m_refreshBtn, &QPushButton::clicked, this, &MapWidget::onRefresh);
    toolbar->addWidget(m_addBtn);
    toolbar->addWidget(m_refreshBtn);
    toolbar->addStretch();
    mainLayout->addLayout(toolbar);

    // 地图卡片列表
    m_listWidget = new QListWidget;
    m_listWidget->setViewMode(QListWidget::IconMode);
    m_listWidget->setIconSize(QSize(200, 200));
    m_listWidget->setGridSize(QSize(280, 320));
    m_listWidget->setResizeMode(QListWidget::Adjust);
    m_listWidget->setMovement(QListWidget::Static);
    m_listWidget->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(m_listWidget, &QListWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QListWidgetItem *item = m_listWidget->itemAt(pos);
        if (!item) return;
        int idx = item->data(Qt::UserRole).toInt();
        if (idx < 0 || idx >= m_mapList.size()) return;
        QJsonObject mapData = m_mapList[idx].toObject();

        QMenu menu;
        QAction *renameAct = menu.addAction("重命名地图");
        QAction *downloadAct = menu.addAction("下载地图");
        QAction *sourceAct = menu.addAction("下载源文件");
        QAction *sendAct = menu.addAction("下发地图资源至设备");
        QAction *deleteAct = menu.addAction("删除地图");
        QAction *editRouteAct = menu.addAction("编辑路网");
        QAction *navAct = menu.addAction("导航");

        bool disabled = mapData.value("delFlag").toString() != "0";
        renameAct->setEnabled(!disabled);
        downloadAct->setEnabled(!disabled);
        editRouteAct->setEnabled(!disabled);
        navAct->setEnabled(!disabled);

        QAction *selected = menu.exec(m_listWidget->mapToGlobal(pos));
        if (selected == renameAct) showRenameDialog(mapData);
        else if (selected == downloadAct) downloadMap(mapData);
        else if (selected == sourceAct) downloadSourceFiles(mapData);
        else if (selected == sendAct) sendMapToCar(mapData);
        else if (selected == deleteAct) deleteMap(mapData);
        else if (selected == editRouteAct) editMapRoute(mapData);
        else if (selected == navAct) navigateToMap(mapData);
    });

    connect(m_listWidget, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        int idx = item->data(Qt::UserRole).toInt();
        if (idx < 0 || idx >= m_mapList.size()) return;
        navigateToMap(m_mapList[idx].toObject());
    });

    mainLayout->addWidget(m_listWidget);
}

void MapWidget::getList()
{
    QJsonObject query;
    query["pageNum"] = m_pageNum;
    query["pageSize"] = m_pageSize;

    ApiService::instance().listMap(query, [this](int code, const QJsonObject &data) {
        if (code != 200) return;
        m_mapList = data.value("rows").toArray();
        m_total = data.value("total").toInt();

        m_listWidget->clear();
        for (int i = 0; i < m_mapList.size(); ++i) {
            QJsonObject map = m_mapList[i].toObject();
            QString name = map.value("mapName").toString();
            QString remark = map.value("remark").toString();
            QString delFlag = map.value("delFlag").toString();
            QString mapId = jsonIdStr(map.value("mapId"));
            QString tilemapPath = map.value("tilemapPath").toString();

            QListWidgetItem *item = new QListWidgetItem;
            item->setText(QString("%1\n%2").arg(name, remark));
            item->setData(Qt::UserRole, i);

            if (delFlag == "3") {
                item->setToolTip("地图正在处理中");
            } else if (delFlag == "4") {
                item->setToolTip("错误地图");
            }

            // 加载缩略图
            if (!tilemapPath.isEmpty() && delFlag != "3" && delFlag != "4") {
                QString mapHost = AppConfig::instance().mapHost();
                QString imageUrl = QString("%1%2_%3/%4/thumbnail.png")
                    .arg(mapHost, name, mapId, tilemapPath);
                ApiService::instance().downloadFile(imageUrl, [item](bool ok, const QByteArray &imgData) {
                    if (ok) {
                        QPixmap pixmap;
                        if (pixmap.loadFromData(imgData)) {
                            item->setIcon(pixmap.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                        }
                    }
                });
            }

            m_listWidget->addItem(item);
        }
    });
}

void MapWidget::onAdd()
{
    showAddDialog();
}

void MapWidget::onRefresh()
{
    getList();
}

void MapWidget::showAddDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle("添加地图");
    QFormLayout *form = new QFormLayout(&dlg);

    QLineEdit *nameEdit = new QLineEdit;
    QTextEdit *remarkEdit = new QTextEdit;
    remarkEdit->setMaximumHeight(60);

    // 文件选择
    QLineEdit *pgmPath = new QLineEdit;
    QPushButton *pgmBtn = new QPushButton("选择");
    QLineEdit *yamlPath = new QLineEdit;
    QPushButton *yamlBtn = new QPushButton("选择");
    QLineEdit *pbStreamPath = new QLineEdit;
    QPushButton *pbStreamBtn = new QPushButton("选择");
    QLineEdit *reflectorBinPath = new QLineEdit;
    QPushButton *reflectorBinBtn = new QPushButton("选择");
    QLineEdit *reflectorJsonPath = new QLineEdit;
    QPushButton *reflectorJsonBtn = new QPushButton("选择");

    pgmPath->setReadOnly(true);
    yamlPath->setReadOnly(true);
    pbStreamPath->setReadOnly(true);
    reflectorBinPath->setReadOnly(true);
    reflectorJsonPath->setReadOnly(true);

    connect(pgmBtn, &QPushButton::clicked, [pgmPath]() {
        QString f = QFileDialog::getOpenFileName(nullptr, "选择PGM文件", "", "PGM Files (*.pgm)");
        if (!f.isEmpty()) pgmPath->setText(f);
    });
    connect(yamlBtn, &QPushButton::clicked, [yamlPath]() {
        QString f = QFileDialog::getOpenFileName(nullptr, "选择YAML文件", "", "YAML Files (*.yaml)");
        if (!f.isEmpty()) yamlPath->setText(f);
    });
    connect(pbStreamBtn, &QPushButton::clicked, [pbStreamPath]() {
        QString f = QFileDialog::getOpenFileName(nullptr, "选择PBStream文件", "", "PBStream Files (*.pbstream)");
        if (!f.isEmpty()) pbStreamPath->setText(f);
    });
    connect(reflectorBinBtn, &QPushButton::clicked, [reflectorBinPath]() {
        QString f = QFileDialog::getOpenFileName(nullptr, "选择反光柱bin文件", "", "BIN Files (*.bin)");
        if (!f.isEmpty()) reflectorBinPath->setText(f);
    });
    connect(reflectorJsonBtn, &QPushButton::clicked, [reflectorJsonPath]() {
        QString f = QFileDialog::getOpenFileName(nullptr, "选择反光柱json文件", "", "JSON Files (*.json)");
        if (!f.isEmpty()) reflectorJsonPath->setText(f);
    });

    form->addRow("地图名称:", nameEdit);
    form->addRow("描述:", remarkEdit);
    form->addRow("PGM文件:", pgmPath);
    form->addRow("", pgmBtn);
    form->addRow("YAML文件:", yamlPath);
    form->addRow("", yamlBtn);
    form->addRow("PBStream文件:", pbStreamPath);
    form->addRow("", pbStreamBtn);
    form->addRow("反光柱bin文件:", reflectorBinPath);
    form->addRow("", reflectorBinBtn);
    form->addRow("反光柱json文件:", reflectorJsonPath);
    form->addRow("", reflectorJsonBtn);

    QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    btns->button(QDialogButtonBox::Ok)->setText("确定");
    btns->button(QDialogButtonBox::Cancel)->setText("取消");
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(btns);

    if (dlg.exec() == QDialog::Accepted) {
        if (nameEdit->text().isEmpty()) {
            QMessageBox::warning(this, "提示", "地图名称不能为空");
            return;
        }

        // 使用 addAgvMap API
        QJsonObject data;
        data["mapName"] = nameEdit->text();
        data["remark"] = remarkEdit->toPlainText();

        ApiService::instance().addAgvMap(data, [this](int code, const QJsonObject &) {
            if (code == 200) {
                QMessageBox::information(this, "成功", "添加成功");
                getList();
            }
        });
    }
}

void MapWidget::showRenameDialog(const QJsonObject &mapData)
{
    QDialog dlg(this);
    dlg.setWindowTitle("修改地图信息");
    QFormLayout *form = new QFormLayout(&dlg);

    QLineEdit *nameEdit = new QLineEdit(mapData.value("mapName").toString());
    QTextEdit *remarkEdit = new QTextEdit(mapData.value("remark").toString());
    remarkEdit->setMaximumHeight(60);

    form->addRow("地图名称:", nameEdit);
    form->addRow("描述:", remarkEdit);

    QDialogButtonBox *btns2 = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    btns2->button(QDialogButtonBox::Ok)->setText("确定");
    btns2->button(QDialogButtonBox::Cancel)->setText("取消");
    connect(btns2, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns2, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(btns2);

    if (dlg.exec() == QDialog::Accepted) {
        QJsonObject data;
        data["mapId"] = mapData.value("mapId");
        data["mapName"] = nameEdit->text();
        data["remark"] = remarkEdit->toPlainText();

        ApiService::instance().updateMap(data, [this](int code, const QJsonObject &) {
            if (code == 200) {
                QMessageBox::information(this, "成功", "修改成功");
                getList();
            }
        });
    }
}

void MapWidget::deleteMap(const QJsonObject &mapData)
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("确认");
    msgBox.setText(QString("是否确认删除地图\"%1\"？").arg(mapData.value("mapName").toString()));
    QPushButton *yesBtn = msgBox.addButton("确定", QMessageBox::YesRole);
    msgBox.addButton("取消", QMessageBox::NoRole);
    msgBox.exec();
    if (msgBox.clickedButton() == yesBtn) {
        ApiService::instance().delMap(jsonIdStr(mapData.value("mapId")), [this](int code, const QJsonObject &) {
            if (code == 200) {
                QMessageBox::information(this, "成功", "删除成功");
                getList();
            }
        });
    }
}

void MapWidget::downloadMap(const QJsonObject &mapData)
{
    QString mapName = mapData.value("mapName").toString();
    QString mapId = jsonIdStr(mapData.value("mapId"));
    // 下载地图切片和路网
    QString url = AppConfig::instance().mapHost() + mapName + "_" + mapId;
    QDesktopServices::openUrl(QUrl(url));
}

void MapWidget::downloadSourceFiles(const QJsonObject &mapData)
{
    QString mapType = mapData.value("mapType").toString();
    if (mapType == "0") {
        // 室外地图，下载PCD
        ApiService::instance().downloadPcd(QJsonObject{{"mapId", jsonIdStr(mapData.value("mapId"))}}, [this](int code, const QJsonObject &data) {
            if (code == 200) {
                QString pcdPath = data.value("pcdPath").toString();
                if (!pcdPath.isEmpty()) {
                    QString url = AppConfig::instance().pcdHost() + pcdPath;
                    QDesktopServices::openUrl(QUrl(url));
                }
            }
        });
    } else {
        // AGV建图，下载资源文件
        QString mapName = mapData.value("mapName").toString();
        QString mapId = jsonIdStr(mapData.value("mapId"));
        QString url = AppConfig::instance().mapHost() + mapName + "_" + mapId;
        QDesktopServices::openUrl(QUrl(url));
    }
}

void MapWidget::sendMapToCar(const QJsonObject &mapData)
{
    QJsonObject params;
    params["mapId"] = mapData.value("mapId");
    ApiService::instance().sendMap2Car(params, [this](int code, const QJsonObject &) {
        if (code == 200) {
            QMessageBox::information(this, "成功", "已下发地图资源至设备");
        }
    });
}

void MapWidget::navigateToMap(const QJsonObject &mapData)
{
    // 对应 handleShow -> 导航到地图展示页面
    // 在 Qt 中，发出信号让 MainWindow 切换到地图展示视图
    QString mapId = jsonIdStr(mapData.value("mapId"));
    emit navigateRequested("map_show", mapId);
}

void MapWidget::editMapRoute(const QJsonObject &mapData)
{
    // 对应 handleEdit -> 编辑路网
    QString mapId = jsonIdStr(mapData.value("mapId"));
    emit navigateRequested("map_edit", mapId);
}
