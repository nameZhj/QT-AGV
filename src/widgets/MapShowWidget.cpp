#include "MapShowWidget.h"
#include "core/ApiService.h"
#include "core/AppConfig.h"
#include "core/MqttService.h"
#include "core/RobotInfoManager.h"
#include "core/LoadingDialog.h"
#include "core/MapPgmUtil.h"
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QMessageBox>
#include <QGraphicsPixmapItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsPolygonItem>
#include <QPainterPath>
#include <QWheelEvent>
#include <QJsonDocument>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QCheckBox>
#include <QRegularExpression>
#include <QSvgRenderer>
#include <QDebug>
#include <QGraphicsLineItem>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>

MapShowWidget::MapShowWidget(const QString &mapId, QWidget *parent)
    : QWidget(parent), m_mapId(mapId)
{
    // Load robot SVG icon at 180×330 (matching 60×110 aspect ratio) for crisp display
    QSvgRenderer renderer(QString(":/ic-robot1.svg"));
    if (renderer.isValid()) {
        m_robotPix = QPixmap(180, 330);
        m_robotPix.fill(Qt::transparent);
        QPainter rp(&m_robotPix);
        rp.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        renderer.render(&rp);
    }
    setupUI();
    loadMapData();
    loadRouteData();
    loadCarList();  // loadCarList callback 会调用 initMqtt()
}

MapShowWidget::~MapShowWidget()
{
    disconnectMqtt();
}

void MapShowWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // 顶部标题栏
    QHBoxLayout *headerLayout = new QHBoxLayout;
    QPushButton *backBtn = new QPushButton("返回");
    connect(backBtn, &QPushButton::clicked, this, &MapShowWidget::goBack);
    m_mapInfoLabel = new QLabel(QString("地图导航页面 - [%1]").arg(m_mapId));
    m_mapInfoLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    headerLayout->addWidget(backBtn);
    headerLayout->addWidget(m_mapInfoLabel);
    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);

    // 主体
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

    // 左侧地图
    QWidget *mapPanel = new QWidget;
    QVBoxLayout *mapLayout = new QVBoxLayout(mapPanel);
    mapLayout->setContentsMargins(0, 0, 0, 0);

    m_scene = new QGraphicsScene(this);
    m_mapView = new MapCanvas(m_scene, this);
    m_mapView->setPanWithLeftButton(true);
    m_mapView->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    m_mapView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    m_mapView->viewport()->installEventFilter(this);
    mapLayout->addWidget(m_mapView);

    // 使用 MapCanvas 统一的信号处理鼠标移动/点击(与其它页面一致)
    connect(m_mapView, &MapCanvas::sceneMoved, this, [this](const QPointF &sp) {
        QPointF wp = sceneToWorld(sp.x(), sp.y());
        m_coordLabel->setText(QString("X: %1  Y: %2").arg(wp.x(), 0, 'f', 2).arg(wp.y(), 0, 'f', 2));
    });
    connect(m_mapView, &MapCanvas::sceneClicked, this, [this](const QPointF &sp, Qt::MouseButton btn) {
        if (btn != Qt::LeftButton) return;
        if (m_drawStatus != Pose) return;
        QPointF wp = sceneToWorld(sp.x(), sp.y());
        QString carId = m_carCombo->currentData().toString();
        if (carId.isEmpty()) return;
        QJsonObject pose;
        pose["x"] = wp.x();
        pose["y"] = wp.y();
        pose["angle"] = 0;
        auto *loading = new LoadingDialog("正在初始化位姿...", this);
        loading->show();
        ApiService::instance().initPose(carId, pose, [this, loading](int code, const QJsonObject &) {
            loading->close();
            loading->deleteLater();
            if (code == 200) {
                QMessageBox::information(this, "成功", "初始化位姿成功");
            }
        });
    });

    // 坐标显示
    m_coordLabel = new QLabel("X: 0.00  Y: 0.00");
    m_coordLabel->setStyleSheet("background: rgba(0,0,0,150); color: white; padding: 4px 8px;");
    m_coordLabel->setParent(m_mapView);
    m_coordLabel->move(10, 10);
    m_coordLabel->raise();

    // 调试信息标签 (显示坐标转换参数)
    m_debugLabel = new QLabel("DEBUG: waiting for map data...");
    m_debugLabel->setStyleSheet(
        "background: rgba(0,0,0,180); color: #ffcc00; padding: 6px 10px; "
        "font-family: monospace; font-size: 11px;");
    m_debugLabel->setParent(m_mapView);
    m_debugLabel->move(10, 35);
    m_debugLabel->raise();

    splitter->addWidget(mapPanel);

    // 右侧信息面板
    QWidget *infoPanel = new QWidget;
    QVBoxLayout *infoLayout = new QVBoxLayout(infoPanel);
    infoLayout->setContentsMargins(4, 4, 4, 4);

    // 绘制模式
    QGroupBox *drawGroup = new QGroupBox("绘制模式");
    QVBoxLayout *drawLayout = new QVBoxLayout(drawGroup);
    m_drawStatusCombo = new QComboBox;
    m_drawStatusCombo->addItem("拖拽", Drag);
    m_drawStatusCombo->addItem("初始化", Pose);
    connect(m_drawStatusCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MapShowWidget::onDrawStatusChanged);
    drawLayout->addWidget(m_drawStatusCombo);
    infoLayout->addWidget(drawGroup);

    // 操作按钮
    QGroupBox *actionGroup = new QGroupBox("操作");
    QVBoxLayout *actionLayout = new QVBoxLayout(actionGroup);
    m_loadTaskBtn = new QPushButton("加载任务");
    connect(m_loadTaskBtn, &QPushButton::clicked, this, &MapShowWidget::onLoadTask);
    actionLayout->addWidget(m_loadTaskBtn);
    infoLayout->addWidget(actionGroup);

    // 设备信息
    QGroupBox *carGroup = new QGroupBox("当前设备信息");
    QFormLayout *carLayout = new QFormLayout(carGroup);

    m_carCombo = new QComboBox;
    connect(m_carCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MapShowWidget::onCarSelected);
    carLayout->addRow("选择设备:", m_carCombo);

    m_carNameLabel = new QLabel("-");
    m_batteryLabel = new QLabel("-");
    m_covarianceLabel = new QLabel("-");
    m_speedLabel = new QLabel("-");
    m_xLabel = new QLabel("-");
    m_yLabel = new QLabel("-");

    carLayout->addRow("设备名称:", m_carNameLabel);
    carLayout->addRow("电量:", m_batteryLabel);
    carLayout->addRow("置信度:", m_covarianceLabel);
    carLayout->addRow("速度:", m_speedLabel);
    carLayout->addRow("X坐标:", m_xLabel);
    carLayout->addRow("Y坐标:", m_yLabel);

    infoLayout->addWidget(carGroup);

    // 功能设置
    QGroupBox *settingGroup = new QGroupBox("功能设置");
    QVBoxLayout *settingLayout = new QVBoxLayout(settingGroup);
    QCheckBox *showTargetCheck = new QCheckBox("显示任务点");
    QCheckBox *showReflectiveCheck = new QCheckBox("显示反光柱");
    settingLayout->addWidget(showTargetCheck);
    settingLayout->addWidget(showReflectiveCheck);
    infoLayout->addWidget(settingGroup);

    // 当前任务
    QGroupBox *taskGroup = new QGroupBox("当前任务");
    QVBoxLayout *taskLayout = new QVBoxLayout(taskGroup);

    m_taskNameLabel = new QLabel("无任务");
    m_taskStatusLabel = new QLabel("-");
    taskLayout->addWidget(m_taskNameLabel);
    taskLayout->addWidget(m_taskStatusLabel);

    QHBoxLayout *taskBtnLayout = new QHBoxLayout;
    m_taskStartBtn = new QPushButton("执行");
    m_taskPauseBtn = new QPushButton("暂停");
    m_taskContinueBtn = new QPushButton("继续");
    m_taskStopBtn = new QPushButton("结束");
    m_taskRemoveBtn = new QPushButton("移除");

    m_taskStartBtn->setEnabled(false);
    m_taskPauseBtn->setEnabled(false);
    m_taskContinueBtn->setEnabled(false);
    m_taskStopBtn->setEnabled(false);
    m_taskRemoveBtn->setEnabled(false);

    connect(m_taskStartBtn, &QPushButton::clicked, this, [this]() {
        QString taskId = m_currentTask.value("taskId").toString();
        if (taskId.isEmpty()) return;
        auto *loading = new LoadingDialog("正在执行任务...", this);
        loading->show();
        ApiService::instance().startTask(taskId, [this, loading](int code, const QJsonObject &) {
            loading->close();
            loading->deleteLater();
            if (code == 200) loadCurrentTask();
        });
    });
    connect(m_taskPauseBtn, &QPushButton::clicked, this, [this]() {
        auto *loading = new LoadingDialog("正在暂停任务...", this);
        loading->show();
        ApiService::instance().commandTask(QJsonObject{{"command", "pause navigation"}}, [this, loading](int code, const QJsonObject &) {
            loading->close();
            loading->deleteLater();
            if (code == 200) loadCurrentTask();
        });
    });
    connect(m_taskContinueBtn, &QPushButton::clicked, this, [this]() {
        auto *loading = new LoadingDialog("正在继续任务...", this);
        loading->show();
        ApiService::instance().commandTask(QJsonObject{{"command", "continue navigation"}}, [this, loading](int code, const QJsonObject &) {
            loading->close();
            loading->deleteLater();
            if (code == 200) loadCurrentTask();
        });
    });
    connect(m_taskStopBtn, &QPushButton::clicked, this, [this]() {
        auto *loading = new LoadingDialog("正在结束任务...", this);
        loading->show();
        ApiService::instance().commandTask(QJsonObject{{"command", "stop navigation"}}, [this, loading](int code, const QJsonObject &) {
            loading->close();
            loading->deleteLater();
            if (code == 200) loadCurrentTask();
        });
    });
    connect(m_taskRemoveBtn, &QPushButton::clicked, this, [this]() {
        QString carId = m_carCombo->currentData().toString();
        auto *loading = new LoadingDialog("正在移除任务...", this);
        loading->show();
        ApiService::instance().removeTask(QJsonObject{{"carId", carId}}, [this, loading](int code, const QJsonObject &) {
            loading->close();
            loading->deleteLater();
            if (code == 200) loadCurrentTask();
        });
    });

    taskBtnLayout->addWidget(m_taskStartBtn);
    taskBtnLayout->addWidget(m_taskPauseBtn);
    taskBtnLayout->addWidget(m_taskContinueBtn);
    taskBtnLayout->addWidget(m_taskStopBtn);
    taskBtnLayout->addWidget(m_taskRemoveBtn);
    taskLayout->addLayout(taskBtnLayout);

    infoLayout->addWidget(taskGroup);

    infoLayout->addStretch();
    splitter->addWidget(infoPanel);
    splitter->setSizes({700, 250});

    mainLayout->addWidget(splitter);
}

void MapShowWidget::loadMapData()
{
    ApiService::instance().getMap(m_mapId, [this](int code, const QJsonObject &data) {
        if (code != 200) return;
        QJsonObject mapInfo = data.value("data").toObject(data);
        m_mapName = mapInfo.value("mapName").toString();
        m_tilemapPath = mapInfo.value("tilemapPath").toString();
        m_mapInfoLabel->setText(QString("地图导航页面 - %1 [%2]").arg(m_mapName, m_mapId));

        // 打印完整API响应用于调试
        qDebug() << "===== MapShowWidget::loadMapData API Response =====";
        qDebug() << "mapId:" << m_mapId;
        qDebug() << "mapName:" << m_mapName;
        qDebug() << "tilemapPath:" << m_tilemapPath;
        qDebug() << "Full mapInfo keys:" << mapInfo.keys();
        for (const QString &key : mapInfo.keys()) {
            qDebug() << "  " << key << "=" << mapInfo.value(key);
        }

        // 提取坐标转换参数 - 严格对齐 Vue show.vue initMap()
        // Vue: this.offset = [rst.offsetX, rst.offsetY]
        // Vue: this.mapSize = [rst.mapWidth, rst.mapHeight]
        // Vue: x1 = 0 - offset[0], y1 = 0 - offset[1]
        // Vue: extent = [x1, y1, mapSize[0]+x1, mapSize[1]+y1]

        // 增强的数值解析：支持字符串和数字类型
        auto parseDouble = [](const QJsonObject &obj, const QString &key, double def) -> double {
            QJsonValue v = obj.value(key);
            if (v.isDouble()) return v.toDouble();
            if (v.isString()) {
                bool ok = false;
                double d = v.toString().toDouble(&ok);
                if (ok) return d;
            }
            return def;
        };

        m_offsetX = parseDouble(mapInfo, "offsetX", 0.0);
        m_offsetY = parseDouble(mapInfo, "offsetY", 0.0);
        m_mapWidth = parseDouble(mapInfo, "mapWidth", 0.0);
        m_mapHeight = parseDouble(mapInfo, "mapHeight", 0.0);

        double apiOriginX = -m_offsetX;
        double apiOriginY = -m_offsetY;
        double apiPhysW   = m_mapWidth;
        double apiPhysH   = m_mapHeight;

        QString mapHost = AppConfig::instance().mapHost();
        QString thumbUrl;
        if (!m_tilemapPath.isEmpty()) {
            thumbUrl = QString("%1%2_%3/%4/thumbnail.png").arg(mapHost, m_mapName, m_mapId, m_tilemapPath);
        }

        auto useThumbnailFallback = [this, thumbUrl, apiOriginX, apiOriginY, apiPhysW, apiPhysH]() {
            if (thumbUrl.isEmpty()) return;
            m_worldL = apiOriginX;
            m_worldB = apiOriginY;
            m_worldR = apiOriginX + apiPhysW;
            m_worldT = apiOriginY + apiPhysH;

            ApiService::instance().downloadFile(thumbUrl, [this](bool ok, const QByteArray &imgData) {
                if (!ok || imgData.isEmpty()) {
                    m_mapInfoLabel->setText("地图图像下载失败");
                    return;
                }
                if (!m_mapImage.loadFromData(imgData)) {
                    m_mapInfoLabel->setText("地图图像解析失败");
                    return;
                }
                m_imgW = m_mapImage.width();
                m_imgH = m_mapImage.height();

                if (m_mapView) {
                    m_mapView->setWorldInfo(m_worldL, m_worldB, m_worldR, m_worldT, m_imgW, m_imgH);
                }

                double resX = (m_imgW > 0) ? (m_worldR - m_worldL) / m_imgW : 0;
                m_mapInfoLabel->setText(
                    QString("%1[%2] img:%3x%4 分辨率:%5m/px 原点:(%6,%7)m")
                    .arg(m_mapName, m_mapId)
                    .arg((int)m_imgW).arg((int)m_imgH)
                    .arg(resX, 0, 'f', 4)
                    .arg(m_worldL, 0, 'f', 2).arg(m_worldB, 0, 'f', 2));

                renderMap();
                QTimer::singleShot(0, this, [this]{
                    if (!m_hasInitialFit && m_scene && m_mapView) {
                        m_mapView->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
                        m_hasInitialFit = true;
                    }
                });
            });
        };

        // 主路径：直接拉取并解析原始 carto_map.pgm + carto_map.yaml
        QString resBase = AppConfig::instance().agvMapResourceHost() + m_mapName + "_" + m_mapId + "/";
        QString yamlUrl = resBase + "carto_map.yaml";
        QString pgmUrl  = resBase + "carto_map.pgm";

        ApiService::instance().downloadFile(yamlUrl,
            [this, pgmUrl, useThumbnailFallback](bool yamlOk, const QByteArray &yamlRaw) {
                if (!yamlOk) { useThumbnailFallback(); return; }
                double resolution = 0, originX = 0, originY = 0; bool negate = false;
                if (!MapPgmUtil::parseMapYaml(QString::fromUtf8(yamlRaw), resolution, originX, originY, negate)
                    || resolution <= 0) {
                    useThumbnailFallback(); return;
                }
                ApiService::instance().downloadFile(pgmUrl,
                    [this, resolution, originX, originY, negate, useThumbnailFallback]
                    (bool pgmOk, const QByteArray &pgmRaw) {
                        if (!pgmOk) { useThumbnailFallback(); return; }
                        QImage img = MapPgmUtil::pgmToImage(pgmRaw, negate);
                        if (img.isNull()) { useThumbnailFallback(); return; }

                        m_mapImage = img;
                        m_imgW = img.width();
                        m_imgH = img.height();
                        m_worldL = originX;
                        m_worldB = originY;
                        m_worldR = m_worldL + img.width()  * resolution;
                        m_worldT = m_worldB + img.height() * resolution;

                        if (m_mapView) {
                            m_mapView->setWorldInfo(m_worldL, m_worldB, m_worldR, m_worldT, m_imgW, m_imgH);
                        }

                        m_mapInfoLabel->setText(
                            QString("%1[%2] img:%3x%4 分辨率:%5m/px 原点:(%6,%7)m")
                            .arg(m_mapName, m_mapId)
                            .arg((int)m_imgW).arg((int)m_imgH)
                            .arg(resolution, 0, 'f', 4)
                            .arg(m_worldL, 0, 'f', 2).arg(m_worldB, 0, 'f', 2));

                        renderMap();
                        QTimer::singleShot(0, this, [this]{
                            if (!m_hasInitialFit && m_scene && m_mapView) {
                                m_mapView->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
                                m_hasInitialFit = true;
                            }
                        });
                    });
            });
    });
}

void MapShowWidget::loadRouteData()
{
    ApiService::instance().loadEditedMap([this](int code, const QJsonObject &data) {
        if (code != 200) return;
        m_points = QJsonArray();
        m_lines  = QJsonArray();
        m_curves = QJsonArray();

        QString jsonStr = data.value("json").toString();
        if (jsonStr.isEmpty()) {
            renderMap();
            return;
        }
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        if (!doc.isObject()) return;
        QJsonArray features = doc.object().value("features").toArray();

        for (const auto &fv : features) {
            QJsonObject f = fv.toObject();
            QJsonObject props = f.value("properties").toObject();
            QJsonObject geom  = f.value("geometry").toObject();
            QString gtype = geom.value("type").toString();
            QJsonArray coords = geom.value("coordinates").toArray();
            QString ftype = props.value("featureType").toString();

            if (gtype == "Point" && (ftype == "point" || ftype == "charge" || ftype == "highPrecision" || ftype.isEmpty())) {
                QJsonObject pt;
                pt["id"] = props.value("id").toString();
                pt["x"] = coords[0].toDouble();
                pt["y"] = coords[1].toDouble();
                pt["name"] = props.value("name").toString();
                pt["type"] = ftype.isEmpty() ? "point" : ftype;
                if (props.contains("yaw")) pt["yaw"] = props.value("yaw").toDouble();
                else if (props.contains("robotYaw")) pt["yaw"] = props.value("robotYaw").toDouble();
                else if (props.contains("theta")) pt["yaw"] = props.value("theta").toDouble() * M_PI / 180.0;
                m_points.append(pt);
            } else if (gtype == "LineString" && ftype == "line") {
                QJsonObject ln;
                ln["id"] = props.value("id").toString();
                ln["x1"] = coords[0].toArray()[0].toDouble();
                ln["y1"] = coords[0].toArray()[1].toDouble();
                ln["x2"] = coords[1].toArray()[0].toDouble();
                ln["y2"] = coords[1].toArray()[1].toDouble();
                ln["name"] = props.value("name").toString();
                m_lines.append(ln);
            } else if (gtype == "LineString" && ftype == "curve") {
                QJsonObject cv;
                cv["id"] = props.value("id").toString();
                cv["x1"] = coords[0].toArray()[0].toDouble();
                cv["y1"] = coords[0].toArray()[1].toDouble();
                cv["x2"] = coords[2].toArray()[0].toDouble();
                cv["y2"] = coords[2].toArray()[1].toDouble();
                cv["cx"] = coords[1].toArray()[0].toDouble();
                cv["cy"] = coords[1].toArray()[1].toDouble();
                cv["name"] = props.value("name").toString();
                m_curves.append(cv);
            }
        }
        renderMap();
    });
}

void MapShowWidget::loadCarList()
{
    ApiService::instance().listCarAll([this](int code, const QJsonObject &data) {
        if (code != 200) return;
        m_carList = data.value("rows").toArray();
        m_carCombo->clear();
        for (const auto &item : m_carList) {
            QJsonObject car = item.toObject();
            QString carId = jsonIdStr(car.value("carId"));
            QString carName = car.value("carName").toString();
            m_carCombo->addItem(carName, carId);

            CarInfo info;
            info.carId = carId;
            info.carName = carName;
            info.online = m_onlineCarIds.contains(carId);
            m_carInfos[carId] = info;
        }
        initMqtt();
    });
}

void MapShowWidget::updateCarList()
{
    // 更新设备在线状态
    for (auto it = m_carInfos.begin(); it != m_carInfos.end(); ++it) {
        it->online = m_onlineCarIds.contains(it->carId);
    }
    // 更新下拉框显示 (在线设备标记)
    for (int i = 0; i < m_carCombo->count(); ++i) {
        QString carId = m_carCombo->itemData(i).toString();
        QString name = m_carInfos[carId].carName;
        if (m_onlineCarIds.contains(carId)) {
            m_carCombo->setItemText(i, name + " [在线]");
        } else {
            m_carCombo->setItemText(i, name + " [离线]");
        }
    }
}

void MapShowWidget::loadCurrentTask()
{
    QString carId = m_carCombo->currentData().toString();
    if (carId.isEmpty()) return;

    ApiService::instance().getCurrentTask(QJsonObject{{"carId", carId}}, [this](int code, const QJsonObject &data) {
        if (code != 200) return;
        QJsonObject taskData = data.value("data").toObject(data);
        if (taskData.isEmpty()) {
            m_currentTask = QJsonObject();
            m_taskNameLabel->setText("无任务");
            m_taskStatusLabel->setText("-");
        } else {
            m_currentTask = taskData;
            m_taskNameLabel->setText("任务: " + taskData.value("taskName").toString());
            QString status = taskData.value("status").toString();
            QString statusText;
            if (status == "0") statusText = "执行中";
            else if (status == "1") statusText = "已完成";
            else if (status == "2") statusText = "失败";
            else if (status == "3") statusText = "暂停";
            else if (status == "4") statusText = "未开始";
            else statusText = status;
            m_taskStatusLabel->setText(statusText);
        }
        updateTaskButtons();
    });
}

void MapShowWidget::updateTaskButtons()
{
    QString status = m_currentTask.value("status").toString();
    m_taskStartBtn->setEnabled(status == "1" || status == "2" || status == "4");
    m_taskPauseBtn->setEnabled(status == "0");
    m_taskContinueBtn->setEnabled(status == "3");
    m_taskStopBtn->setEnabled(status == "0" || status == "3");
    m_taskRemoveBtn->setEnabled(status == "1" || status == "2" || status == "4");
}

QPointF MapShowWidget::worldToScene(double wx, double wy) const
{
    // 严格对齐 Vue/OpenLayers 投影变换
    // Vue: extent = [worldL, worldB, worldR, worldT]
    // OpenLayers 内部: fraction_x = (wx - worldL) / (worldR - worldL)
    //                 fraction_y = (wy - worldB) / (worldT - worldB)
    // Qt场景(Y轴向下): sceneX = fraction_x * imgW
    //                  sceneY = (1 - fraction_y) * imgH = (worldT - wy) / mapH * imgH
    double mapW = m_worldR - m_worldL, mapH = m_worldT - m_worldB;
    if (m_imgW <= 0 || m_imgH <= 0 || mapW <= 0 || mapH <= 0) return {wx, wy};
    return { (wx - m_worldL) / mapW * m_imgW,
             (m_worldT - wy) / mapH * m_imgH };
}

QPointF MapShowWidget::sceneToWorld(double sx, double sy) const
{
    double mapW = m_worldR - m_worldL, mapH = m_worldT - m_worldB;
    if (m_imgW <= 0 || m_imgH <= 0 || mapW <= 0 || mapH <= 0) return {sx, sy};
    return { sx / m_imgW * mapW + m_worldL,
             m_worldT - sy / m_imgH * mapH };
}

void MapShowWidget::renderDebugMarkers()
{
    // 渲染世界原点(0,0)的红色十字标记 - 用于诊断坐标偏移
    QPointF origin = worldToScene(0.0, 0.0);
    auto *originH = m_scene->addLine(origin.x() - 15, origin.y(), origin.x() + 15, origin.y(),
                                      QPen(QColor(255, 0, 0, 200), 2));
    originH->setZValue(200);
    auto *originV = m_scene->addLine(origin.x(), origin.y() - 15, origin.x(), origin.y() + 15,
                                      QPen(QColor(255, 0, 0, 200), 2));
    originV->setZValue(200);
    auto *originLabel = m_scene->addText("(0,0)");
    originLabel->setPos(origin.x() + 8, origin.y() - 20);
    originLabel->setDefaultTextColor(QColor(255, 0, 0));
    originLabel->setZValue(201);

    // 渲染地图范围边框(蓝色虚线)
    QPointF tl = worldToScene(m_worldL, m_worldT);
    QPointF br = worldToScene(m_worldR, m_worldB);
    QRectF extentRect(QPointF(tl.x(), tl.y()), QPointF(br.x(), br.y()));
    auto *extentItem = m_scene->addRect(extentRect,
                                         QPen(QColor(0, 100, 255, 150), 1, Qt::DashLine));
    extentItem->setZValue(199);
}

void MapShowWidget::updateDebugLabel()
{
    if (!m_debugLabel) return;
    double mapW = m_worldR - m_worldL, mapH = m_worldT - m_worldB;
    double resX = (m_imgW > 0) ? mapW / m_imgW : 0;
    QString text = QString(
        "DEBUG | offset:(%1,%2) mapSize:(%3x%4) img:%5x%6 res:%7m/px\n"
        "world:[%8,%9]-[%10,%11] origin->scene:(%12,%13)")
        .arg(m_offsetX, 0, 'f', 2).arg(m_offsetY, 0, 'f', 2)
        .arg(m_mapWidth, 0, 'f', 1).arg(m_mapHeight, 0, 'f', 1)
        .arg((int)m_imgW).arg((int)m_imgH)
        .arg(resX, 0, 'f', 4)
        .arg(m_worldL, 0, 'f', 2).arg(m_worldB, 0, 'f', 2)
        .arg(m_worldR, 0, 'f', 2).arg(m_worldT, 0, 'f', 2);
    QPointF org = worldToScene(0, 0);
    text = text.arg(org.x(), 0, 'f', 1).arg(org.y(), 0, 'f', 1);
    m_debugLabel->setText(text);
}

void MapShowWidget::renderMap()
{
    m_scene->clear();

    // 渲染底图
    if (!m_mapImage.isNull()) {
        QGraphicsPixmapItem *bgItem = m_scene->addPixmap(QPixmap::fromImage(m_mapImage));
        bgItem->setTransformationMode(Qt::SmoothTransformation);
        bgItem->setZValue(-100);
    }

    // 渲染调试标记 (原点十字 + 范围边框)
    renderDebugMarkers();
    updateDebugLabel();

    double pxPerMeterX = (m_imgW > 0 && (m_worldR > m_worldL)) ? (m_imgW / (m_worldR - m_worldL)) : 20.0;
    double pxPerMeterY = (m_imgH > 0 && (m_worldT > m_worldB)) ? (m_imgH / (m_worldT - m_worldB)) : 20.0;

    // 渲染路网点 (世界坐标→场景像素坐标)
    for (const auto &item : m_points) {
        QJsonObject pt = item.toObject();
        QPointF sp = worldToScene(pt.value("x").toDouble(), pt.value("y").toDouble());
        QString id = pt.value("id").toString();
        QString name = pt.value("name").toString();
        if (name.isEmpty()) name = id;

        double rectW = 0, rectH = 0;
        QPainterPath crossPath;
        QPainterPath path = MapPgmUtil::createPointPath(pxPerMeterX, pxPerMeterY, &rectW, &rectH, &crossPath, 0.0);

        QFont f("Segoe UI");
        double ptSize = qBound(4.0, rectH * 0.35, 7.0);
        f.setPointSizeF(ptSize);
        f.setBold(true);
        QFontMetrics fm(f);
        int tw = fm.horizontalAdvance(name);

        QString type = pt.value("type").toString("point");
        QColor fillCol(210, 230, 250, 235);
        QColor borderCol(41, 128, 185, 240);
        if (type == "charge") {
            fillCol = QColor(254, 243, 199, 235);
            borderCol = QColor(217, 119, 6, 240);
        } else if (type == "highPrecision") {
            fillCol = QColor(254, 226, 226, 235);
            borderCol = QColor(220, 38, 38, 240);
        }

        auto *itemRect = m_scene->addPath(path, QPen(borderCol, 0.8), QBrush(fillCol));
        itemRect->setPos(sp);
        itemRect->setZValue(10);

        // 内部红色实线精确定位小十字 (居中位于 (0,0)，与路线精准交汇，不遮挡文字)
        QPen crossPen(QColor("#E74C3C"), 0.8, Qt::SolidLine);
        crossPen.setCapStyle(Qt::FlatCap);
        auto *crossItem = m_scene->addPath(crossPath, crossPen);
        crossItem->setParentItem(itemRect);
        crossItem->setPos(0, 0);
        crossItem->setAcceptedMouseButtons(Qt::NoButton);
        crossItem->setFlag(QGraphicsItem::ItemIsSelectable, false);

        // 若配置了车头朝向角，绘制微型方向指针
        double yawVal = pt.value("yaw").toDouble(0.0);
        if (yawVal != 0.0) {
            double angleDeg = 90.0 - yawVal * 180.0 / M_PI;
            double rad = qDegreesToRadians(angleDeg);
            double pr = rectW / 2.0 + 2.5;
            QPointF pHead(std::sin(rad) * pr, -std::cos(rad) * pr);
            QPolygonF ptr;
            QPointF norm(-std::cos(rad) * 2.0, -std::sin(rad) * 2.0);
            ptr << pHead + QPointF(std::sin(rad) * 3.5, -std::cos(rad) * 3.5)
                << pHead + norm
                << pHead - norm;
            auto *pItem = m_scene->addPolygon(ptr, QPen(Qt::NoPen), QBrush(borderCol));
            pItem->setParentItem(itemRect);
            pItem->setAcceptedMouseButtons(Qt::NoButton);
            pItem->setFlag(QGraphicsItem::ItemIsSelectable, false);
        }

        // 站点名称：直接绘制在矩形框下部，水平居中，与上方红色十字完全分离无遮挡，深色高对比度，尺寸紧凑工整
        auto *label = m_scene->addText(name, f);
        label->document()->setDocumentMargin(0);
        label->setDefaultTextColor(QColor("#1A1A1A"));
        label->setParentItem(itemRect);
        double textY = (rectH >= 12.0) ? (rectH * 0.16) : (rectH / 2.0 + 1.0);
        label->setPos(-tw / 2.0, textY);
        label->setAcceptedMouseButtons(Qt::NoButton);
        label->setFlag(QGraphicsItem::ItemIsSelectable, false);
    }

    // 渲染线 (世界坐标→场景像素坐标)
    QPen linePen(QColor("#1296DB"), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    for (const auto &item : m_lines) {
        QJsonObject ln = item.toObject();
        QPointF a = worldToScene(ln.value("x1").toDouble(), ln.value("y1").toDouble());
        QPointF b = worldToScene(ln.value("x2").toDouble(), ln.value("y2").toDouble());
        int direct = ln.value("direct").toInt(10);
        QGraphicsLineItem *line = m_scene->addLine(a.x(), a.y(), b.x(), b.y(), linePen);
        line->setZValue(5);

        // 线路导向标 (单向或双向)
        QPointF dir = b - a;
        double len = std::hypot(dir.x(), dir.y());
        if (len > 1e-3) {
            dir /= len;
            QPointF mid = (a + b) / 2.0;
            QList<QPolygonF> arrows = MapPgmUtil::createDirectionArrows(mid, dir, direct, len);
            for (const auto &poly : arrows) {
                auto *arr = m_scene->addPolygon(poly, QPen(Qt::NoPen), QBrush(linePen.color()));
                arr->setParentItem(line);
                arr->setZValue(6);
                arr->setAcceptedMouseButtons(Qt::NoButton);
            }
        }
    }

    // 渲染曲线 (世界坐标→场景像素坐标)
    QPen curvePen(QColor("#9B59B6"), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    for (const auto &item : m_curves) {
        QJsonObject cv = item.toObject();
        int direct = cv.value("direct").toInt(10);
        QJsonArray ctrlArr = cv.value("controlPoints").toArray();
        QPainterPath path;
        QList<QPointF> scenePts;
        if (ctrlArr.size() >= 2) {
            for (const auto &pv : ctrlArr) {
                QJsonObject pt = pv.toObject();
                scenePts.append(worldToScene(pt.value("x").toDouble(), pt.value("y").toDouble()));
            }
            path = MapPgmUtil::getBezierPath(scenePts);
        } else {
            QPointF p1 = worldToScene(cv.value("x1").toDouble(), cv.value("y1").toDouble());
            QPointF pc = worldToScene(cv.value("cx").toDouble(), cv.value("cy").toDouble());
            QPointF p2 = worldToScene(cv.value("x2").toDouble(), cv.value("y2").toDouble());
            scenePts.append(p1); scenePts.append(pc); scenePts.append(p2);
            path.moveTo(p1);
            path.quadTo(pc, p2);
        }
        QGraphicsPathItem *pathItem = m_scene->addPath(path, curvePen);
        pathItem->setZValue(5);

        if (scenePts.size() >= 2) {
            double cLen = std::hypot(scenePts.last().x() - scenePts.first().x(),
                                     scenePts.last().y() - scenePts.first().y());
            QList<QPolygonF> arrows = MapPgmUtil::createBezierDirectionArrows(scenePts, direct, cLen);
            for (const auto &poly : arrows) {
                auto *arr = m_scene->addPolygon(poly, QPen(Qt::NoPen), QBrush(curvePen.color()));
                arr->setParentItem(pathItem);
                arr->setZValue(6);
                arr->setAcceptedMouseButtons(Qt::NoButton);
            }
        }
    }

    // scene->clear() already deleted all items; null out dangling marker pointers
    for (auto it = m_carInfos.begin(); it != m_carInfos.end(); ++it)
        it->marker = nullptr;

    // 渲染设备位置
    for (auto it = m_carInfos.begin(); it != m_carInfos.end(); ++it) {
        CarInfo &info = it.value();
        if (info.x != 0 || info.y != 0) {
            updateCarPosition(info.carId, info.x, info.y, info.angle);
        }
    }

    m_scene->setSceneRect(0, 0, m_imgW, m_imgH);
    if (!m_hasInitialFit && m_mapView) {
        m_mapView->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
        m_hasInitialFit = true;
    }
}

void MapShowWidget::updateCarPosition(const QString &carId, double x, double y, double angle)
{
    if (!m_carInfos.contains(carId)) return;
    CarInfo &info = m_carInfos[carId];

    // 移除旧标记 (removeItem takes it out of the scene; we then own and delete it)
    if (info.marker) {
        m_scene->removeItem(info.marker);
        delete info.marker;
        info.marker = nullptr;
    }

    // 设备位置：世界坐标(米)→场景像素坐标
    // Vue: robotFeature.getGeometry().setCoordinates([x, y]) - 直接使用世界坐标
    // Qt: 需要通过worldToScene转换到场景像素坐标
    QPointF sp = worldToScene(x, y);

    // 首次接收位置时打印调试信息
    static QSet<QString> loggedCars;
    if (!loggedCars.contains(carId)) {
        qDebug() << "===== First Position Update for" << carId << "=====";
        qDebug() << "World coord: (" << x << "," << y << ") angle:" << angle;
        qDebug() << "Scene coord: (" << sp.x() << "," << sp.y() << ")";
        qDebug() << "Image size:" << m_imgW << "x" << m_imgH;
        qDebug() << "World extent: L=" << m_worldL << "B=" << m_worldB
                 << "R=" << m_worldR << "T=" << m_worldT;
        loggedCars.insert(carId);
    }

    // 地图比例尺计算: ppm (pixels per meter)
    double mapW = m_worldR - m_worldL;
    double mapH = m_worldT - m_worldB;
    double ppm = 20.0; // 默认备用比例尺 0.05m/px
    if (m_imgW > 0 && mapW > 0 && m_imgH > 0 && mapH > 0) {
        ppm = ((double)m_imgW / mapW + (double)m_imgH / mapH) / 2.0;
    } else if (m_imgW > 0 && mapW > 0) {
        ppm = (double)m_imgW / mapW;
    }

    // 真实尺寸: 车长 1.0m (纵向/车头朝向), 车宽 0.5m (横向)
    const double kRobotLength = 1.0;
    const double kRobotWidth  = 0.5;
    double sceneW = kRobotWidth * ppm;
    double sceneH = kRobotLength * ppm;

    // 旋转角度计算 - 严格对齐 Vue show.vue drawCar()
    // Vue: rotation = Math.PI - yaw (OpenLayers Icon.rotation, CW, 默认图标朝左/西)
    // Qt: setRotation(CW), 我们的SVG图标(ic-robot1.svg)默认朝上/北
    //
    // yaw约定 (ROS标准): atan2(dy, dx), CCW from +X(east)
    //   yaw=0 → 东, yaw=π/2 → 北, yaw=π → 西, yaw=-π/2 → 南
    //
    // Qt图标(朝上/北)需要旋转到yaw方向:
    //   yaw=0(东) → 需旋转90°CW → setRotation(90)
    //   yaw=π/2(北) → 需旋转0° → setRotation(0)
    //   yaw=π(西) → 需旋转-90° → setRotation(-90)
    //   公式: rotation = 90 - yawDeg
    double yawRad = (qAbs(angle) > M_PI * 2) ? angle * M_PI / 180.0 : angle;
    double yawDeg = yawRad * 180.0 / M_PI;
    double screenAngle = 90.0 - yawDeg;

    QGraphicsItem *marker = nullptr;
    if (!m_robotPix.isNull()) {
        const double srcW   = m_robotPix.width();
        const double srcH   = m_robotPix.height();
        const double pivotX = srcW / 2.0;
        const double pivotY = srcH / 2.0;
        const double scale  = (srcW > 0) ? (sceneW / srcW) : 1.0;

        auto *pix = new QGraphicsPixmapItem(m_robotPix);
        pix->setScale(scale);
        pix->setTransformOriginPoint(pivotX, pivotY);
        pix->setRotation(screenAngle);
        pix->setPos(sp.x() - pivotX, sp.y() - pivotY);
        marker = pix;
    } else {
        // fallback: 110*60 矩形 (车头浅蓝)
        double halfW = sceneW / 2.0;
        double halfH = sceneH / 2.0;
        QPolygonF rectPoly;
        rectPoly << QPointF(-halfW, -halfH)
                 << QPointF(halfW, -halfH)
                 << QPointF(halfW, halfH)
                 << QPointF(-halfW, halfH);
        QTransform t;
        t.rotate(screenAngle);
        rectPoly = t.map(rectPoly);
        QPolygonF translated;
        for (const QPointF &pt : rectPoly) translated << pt + sp;
        auto *polyItem = m_scene->addPolygon(translated,
            QPen(QColor(56, 189, 248), 1.5),
            QBrush(QColor(30, 41, 59, 220)));
        polyItem->setZValue(50);
        info.marker = polyItem;
        info.x = x; info.y = y; info.angle = angle;
        return;
    }
    m_scene->addItem(marker);
    marker->setZValue(50);
    info.marker = marker;
    info.x = x;
    info.y = y;
    info.angle = angle;
}

void MapShowWidget::initMqtt()
{
    auto infoCb = [this](const QString &t, const QByteArray &p) {
        onMqttMessage(t, p);
    };

    // 订阅标准 /info 话题
    MqttService::instance().subscribeTopic("/info", 0, infoCb);
    MqttService::instance().subscribeTopic("info", 0, infoCb);
    MqttService::instance().subscribeTopic("/rob/info", 0, infoCb);

    // 订阅所有设备位置信息 (对应 Vue: /rob/{carId}/info)
    for (const auto &item : m_carList) {
        QJsonObject car = item.toObject();
        QString carId = jsonIdStr(car.value("carId"));
        QString topic = QString("/rob/%1/info").arg(carId);
        MqttService::instance().subscribeTopic(topic, 0, infoCb);
    }

    // 订阅设备在线状态 (对应 Vue: /rob/online)
    MqttService::instance().subscribeTopic("/rob/online", 0,
        [this](const QString &, const QByteArray &payload) {
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
            if (err.error != QJsonParseError::NoError) return;
            QJsonObject data = doc.object();
            // data 包含在线设备列表
            QJsonArray onlineCars = data.value("onlineCars").toArray();
            m_onlineCarIds.clear();
            for (const auto &id : onlineCars) {
                m_onlineCarIds.insert(jsonIdStr(id));
            }
            // 更新设备列表显示
            updateCarList();
        });
}

void MapShowWidget::disconnectMqtt()
{
    MqttService::instance().unsubscribeTopic("/info");
    MqttService::instance().unsubscribeTopic("info");
    MqttService::instance().unsubscribeTopic("/rob/info");
    for (const auto &item : m_carList) {
        QJsonObject car = item.toObject();
        QString carId = jsonIdStr(car.value("carId"));
        MqttService::instance().unsubscribeTopic(QString("/rob/%1/info").arg(carId));
    }
    MqttService::instance().unsubscribeTopic("/rob/online");
}

void MapShowWidget::onMqttMessage(const QString &topic, const QByteArray &payload)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
    if (err.error != QJsonParseError::NoError) return;

    QJsonObject data = doc.object();
    QJsonObject dataObj = data.value("data").toObject();
    QJsonObject poseObj = data.value("pose").toObject();

    if (topic.contains("info")) {
        RobotInfoManager::instance().updateFromPayload(topic, payload);

        QString carId;
        // 从topic提取carId: /rob/{carId}/info
        QRegularExpression re("/rob/(.+)/info");
        QRegularExpressionMatch match = re.match(topic);
        if (match.hasMatch()) carId = match.captured(1);

        if (carId.isEmpty()) {
            carId = jsonIdStr(data.value("carId"));
            if (carId.isEmpty()) carId = jsonIdStr(data.value("car_id"));
            if (carId.isEmpty() && !dataObj.isEmpty()) {
                carId = jsonIdStr(dataObj.value("carId"));
                if (carId.isEmpty()) carId = jsonIdStr(dataObj.value("car_id"));
            }
            if (carId.isEmpty()) carId = AppConfig::instance().agvId();
            if (carId.isEmpty() && m_carInfos.size() == 1) {
                carId = m_carInfos.firstKey();
            }
        }

        auto dv = [&](const QStringList &keys, double def = 0) -> double {
            for (const QString &k : keys) {
                if (data.contains(k)) {
                    QJsonValue v = data.value(k);
                    if (!v.isUndefined() && !v.isNull()) return v.toDouble(def);
                }
                if (!dataObj.isEmpty() && dataObj.contains(k)) {
                    QJsonValue v = dataObj.value(k);
                    if (!v.isUndefined() && !v.isNull()) return v.toDouble(def);
                }
                if (!poseObj.isEmpty() && poseObj.contains(k)) {
                    QJsonValue v = poseObj.value(k);
                    if (!v.isUndefined() && !v.isNull()) return v.toDouble(def);
                }
            }
            return def;
        };

        if (!carId.isEmpty()) {
            if (!m_carInfos.contains(carId)) {
                CarInfo newInfo;
                newInfo.carId = carId;
                newInfo.carName = data.value("carName").toString(carId);
                m_carInfos[carId] = newInfo;
            }
            CarInfo &info = m_carInfos[carId];
            info.x = dv({"car_x", "carX", "x", "current_x", "posX"});
            info.y = dv({"car_y", "carY", "y", "current_y", "posY"});
            info.angle = dv({"car_yaw", "carYaw", "yaw", "angle", "theta"});
            info.speed = dv({"speed", "linear_x", "linearX", "v"});
            double rawBat = dv({"battery", "power", "bat", "soc"}, -1.0);
            if (rawBat >= 0.0 && rawBat <= 1.0) {
                info.battery = rawBat * 100.0;
            } else {
                info.battery = rawBat;
            }
            info.status = data.value("status").toString();
            info.taskId = jsonIdStr(data.value("task_id"));

            // 更新地图位置
            updateCarPosition(carId, info.x, info.y, info.angle);
            updateDebugLabel();

            // 更新信息面板
            if (m_carCombo->currentData().toString() == carId) {
                m_carNameLabel->setText(info.carName);
                m_batteryLabel->setText(QString::number(info.battery, 'f', 1) + "%");
                m_speedLabel->setText(QString::number(info.speed, 'f', 2) + " m/s");
                QPointF dbgPt = worldToScene(info.x, info.y);
                m_xLabel->setText(QString("%1→%2px").arg(info.x,0,'f',2).arg((int)dbgPt.x()));
                m_yLabel->setText(QString("%1→%2px").arg(info.y,0,'f',2).arg((int)dbgPt.y()));

                double cov = data.value("covariance_score").toDouble(-1);
                if (cov >= 0) {
                    double covVal = (cov > 1.0) ? cov / 100.0 : cov;
                    m_covarianceLabel->setText(QString::number(covVal, 'f', 2));
                    if (covVal <= 0.4) {
                        m_covarianceLabel->setStyleSheet("color: red;");
                    } else {
                        m_covarianceLabel->setStyleSheet("");
                    }
                } else {
                    m_covarianceLabel->setText("未知");
                }
            }
        }
    }
}

void MapShowWidget::onDrawStatusChanged(int status)
{
    m_drawStatus = static_cast<DrawStatus>(status);
    if (m_drawStatus == Drag) {
        m_mapView->setDragMode(QGraphicsView::ScrollHandDrag);
    } else {
        m_mapView->setDragMode(QGraphicsView::NoDrag);
    }
}

void MapShowWidget::onLoadTask()
{
    // 加载任务 - 弹出任务选择对话框
    QString carId = m_carCombo->currentData().toString();
    if (carId.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先选择设备");
        return;
    }

    QJsonObject query;
    query["carId"] = carId;
    query["mapId"] = m_mapId;

    ApiService::instance().listTask(query, [this](int code, const QJsonObject &data) {
        if (code != 200) return;
        QJsonArray tasks = data.value("rows").toArray();

        if (tasks.isEmpty()) {
            QMessageBox::information(this, "提示", "没有可加载的任务");
            return;
        }

        QDialog dlg(this);
        dlg.setWindowTitle("选择任务");
        QVBoxLayout *layout = new QVBoxLayout(&dlg);

        QTableWidget *taskTable = new QTableWidget;
        taskTable->setColumnCount(3);
        taskTable->setHorizontalHeaderLabels({"任务编号", "任务名称", "状态"});
        taskTable->setRowCount(tasks.size());
        for (int i = 0; i < tasks.size(); ++i) {
            QJsonObject task = tasks[i].toObject();
            taskTable->setItem(i, 0, new QTableWidgetItem(task.value("taskNumber").toString()));
            taskTable->setItem(i, 1, new QTableWidgetItem(task.value("taskName").toString()));
            taskTable->setItem(i, 2, new QTableWidgetItem(task.value("status").toString()));
        }
        layout->addWidget(taskTable);

        QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        btns->button(QDialogButtonBox::Ok)->setText("确定");
        btns->button(QDialogButtonBox::Cancel)->setText("取消");
        connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        layout->addWidget(btns);

        if (dlg.exec() == QDialog::Accepted) {
            int row = taskTable->currentRow();
            if (row >= 0) {
                QString taskId = jsonIdStr(tasks[row].toObject().value("taskId"));
                auto *loading = new LoadingDialog("正在执行任务...", this);
                loading->show();
                ApiService::instance().startTask(taskId, [this, loading](int code, const QJsonObject &) {
                    loading->close();
                    loading->deleteLater();
                    if (code == 200) {
                        QMessageBox::information(this, "成功", "任务已加载");
                    }
                });
            }
        }
    });
}

void MapShowWidget::onCarSelected(int index)
{
    if (index < 0) return;
    QString carId = m_carCombo->currentData().toString();
    if (m_carInfos.contains(carId)) {
        CarInfo &info = m_carInfos[carId];
        m_carNameLabel->setText(info.carName);
        m_batteryLabel->setText(QString::number(info.battery, 'f', 1) + "%");
        m_speedLabel->setText(QString::number(info.speed, 'f', 2) + " m/s");
        m_xLabel->setText(QString::number(info.x, 'f', 2));
        m_yLabel->setText(QString::number(info.y, 'f', 2));
    }
    loadCurrentTask();
}

bool MapShowWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_mapView->viewport()) {
        // MouseMove / Wheel / 点击 已由 MapCanvas 统一信号处理，这里不再拦截
    }
    return QWidget::eventFilter(obj, event);
}
