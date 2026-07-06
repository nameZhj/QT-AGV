#include "MapShowWidget.h"
#include "core/ApiService.h"
#include "core/AppConfig.h"
#include "core/MqttService.h"
#include "core/LoadingDialog.h"
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
    // Load robot SVG icon at 128×128 for crisp display
    QSvgRenderer renderer(QString(":/ic-robot1.svg"));
    if (renderer.isValid()) {
        m_robotPix = QPixmap(128, 128);
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

        // Vue: x1 = 0 - offsetX, y1 = 0 - offsetY
        // Vue: extent = [x1, y1, mapWidth+x1, mapHeight+y1]
        m_worldL = -m_offsetX;
        m_worldB = -m_offsetY;
        m_worldR = m_worldL + m_mapWidth;
        m_worldT = m_worldB + m_mapHeight;

        qDebug() << "===== Coordinate Parameters (Vue-aligned) =====";
        qDebug() << "offsetX:" << m_offsetX << "offsetY:" << m_offsetY;
        qDebug() << "mapWidth:" << m_mapWidth << "mapHeight:" << m_mapHeight;
        qDebug() << "worldL:" << m_worldL << "worldB:" << m_worldB;
        qDebug() << "worldR:" << m_worldR << "worldT:" << m_worldT;
        qDebug() << "worldW:" << (m_worldR - m_worldL) << "worldH:" << (m_worldT - m_worldB);

        QString mapHost = AppConfig::instance().mapHost();
        qDebug() << "mapHost:" << mapHost;

        if (!m_tilemapPath.isEmpty()) {
            // Vue: mapSrc + mapName+'_'+mapId+'/'+tilemapPath + '/{z}/small_image_{x}_{y}.png'
            // Qt:  mapHost + mapName+'_'+mapId+'/'+tilemapPath + '/thumbnail.png'
            QString imageUrl = QString("%1%2_%3/%4/thumbnail.png")
                .arg(mapHost, m_mapName, m_mapId, m_tilemapPath);
            qDebug() << "Thumbnail URL:" << imageUrl;

            ApiService::instance().downloadFile(imageUrl, [this](bool ok, const QByteArray &imgData) {
                if (!ok || imgData.isEmpty()) {
                    qDebug() << "ERROR: Thumbnail download failed";
                    m_mapInfoLabel->setText("地图图像下载失败");
                    return;
                }
                if (!m_mapImage.loadFromData(imgData)) {
                    qDebug() << "ERROR: Thumbnail parse failed";
                    m_mapInfoLabel->setText("地图图像解析失败");
                    return;
                }
                m_imgW = m_mapImage.width();
                m_imgH = m_mapImage.height();

                // 同步 worldInfo 给 MapCanvas，使其内置的设备定位绘制与其它页面一致
                if (m_mapView) {
                    m_mapView->setWorldInfo(m_worldL, m_worldB, m_worldR, m_worldT, m_imgW, m_imgH);
                }

                double mapW = m_worldR - m_worldL;
                double mapH = m_worldT - m_worldB;
                double resX = (m_imgW > 0) ? mapW / m_imgW : 0;
                double resY = (m_imgH > 0) ? mapH / m_imgH : 0;

                qDebug() << "===== Image Loaded =====";
                qDebug() << "imgW:" << m_imgW << "imgH:" << m_imgH;
                qDebug() << "resX:" << resX << "m/px, resY:" << resY << "m/px";
                qDebug() << "Aspect ratio - map:" << (mapW / mapH)
                         << "image:" << (double(m_imgW) / m_imgH);
                qDebug() << "World origin (0,0) -> scene:"
                         << worldToScene(0, 0);

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
        } else {
            qDebug() << "ERROR: tilemapPath is empty!";
        }
    });
}

void MapShowWidget::loadRouteData()
{
    QJsonObject query;
    query["mapId"] = m_mapId;
    ApiService::instance().loadEditedMap(query, [this](int code, const QJsonObject &data) {
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

            if (gtype == "Point" && ftype == "point") {
                QJsonObject pt;
                pt["id"] = props.value("id").toString();
                pt["x"] = coords[0].toDouble();
                pt["y"] = coords[1].toDouble();
                pt["name"] = props.value("name").toString();
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
        bgItem->setZValue(-100);
    }

    // 渲染调试标记 (原点十字 + 范围边框)
    renderDebugMarkers();
    updateDebugLabel();

    // 渲染路网点 (世界坐标→场景像素坐标)
    for (const auto &item : m_points) {
        QJsonObject pt = item.toObject();
        QPointF sp = worldToScene(pt.value("x").toDouble(), pt.value("y").toDouble());

        QGraphicsEllipseItem *ellipse = m_scene->addEllipse(sp.x() - 4, sp.y() - 4, 8, 8,
            QPen(Qt::blue, 1), QBrush(Qt::blue));
        ellipse->setZValue(10);
        ellipse->setOpacity(0.6);

        QGraphicsTextItem *label = m_scene->addText(pt.value("name").toString());
        label->setPos(sp.x() + 6, sp.y() - 8);
        label->setZValue(11);
        label->setOpacity(0.7);
    }

    // 渲染线 (世界坐标→场景像素坐标)
    for (const auto &item : m_lines) {
        QJsonObject ln = item.toObject();
        QPointF a = worldToScene(ln.value("x1").toDouble(), ln.value("y1").toDouble());
        QPointF b = worldToScene(ln.value("x2").toDouble(), ln.value("y2").toDouble());
        QGraphicsLineItem *line = m_scene->addLine(a.x(), a.y(), b.x(), b.y(), QPen(Qt::green, 1));
        line->setZValue(5);
        line->setOpacity(0.5);
    }

    // 渲染曲线 (世界坐标→场景像素坐标)
    for (const auto &item : m_curves) {
        QJsonObject cv = item.toObject();
        QPointF p1 = worldToScene(cv.value("x1").toDouble(), cv.value("y1").toDouble());
        QPointF pc = worldToScene(cv.value("cx").toDouble(), cv.value("cy").toDouble());
        QPointF p2 = worldToScene(cv.value("x2").toDouble(), cv.value("y2").toDouble());
        QPainterPath path;
        path.moveTo(p1);
        path.quadTo(pc, p2);
        QGraphicsPathItem *pathItem = m_scene->addPath(path, QPen(Qt::magenta, 1));
        pathItem->setZValue(5);
        pathItem->setOpacity(0.5);
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

    // 图标显示尺寸(场景单位=图像像素)
    const double iconSz  = 14.0;   // 显示尺寸（场景像素）
    const double srcSz   = 128.0;  // 预渲染尺寸
    const double pivot   = srcSz / 2.0;  // 旋转/缩放锚点 = 中心

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

    QGraphicsPixmapItem *marker;
    if (!m_robotPix.isNull()) {
        marker = new QGraphicsPixmapItem(m_robotPix);
        marker->setScale(iconSz / srcSz);
        marker->setTransformOriginPoint(pivot, pivot);
        marker->setRotation(screenAngle);
        // 缩放轴心(pivot)在场景中的位置 = pos + pivot → 令其等于 sp
        marker->setPos(sp.x() - pivot, sp.y() - pivot);
    } else {
        // fallback: 简单三角形 (尖端朝上=北)
        const double half = iconSz / 2.0;
        QPolygonF tri;
        tri << QPointF(0, -half) << QPointF(-half * 0.6, half * 0.6) << QPointF(half * 0.6, half * 0.6);
        QTransform t;
        t.rotate(screenAngle);
        tri = t.map(tri);
        QPolygonF translated;
        for (const QPointF &pt : tri) translated << pt + sp;
        auto *polyItem = m_scene->addPolygon(translated, QPen(Qt::red, 2), QBrush(Qt::red));
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
    // 订阅所有设备位置信息 (对应 Vue: /rob/{carId}/info)
    for (const auto &item : m_carList) {
        QJsonObject car = item.toObject();
        QString carId = jsonIdStr(car.value("carId"));
        QString topic = QString("/rob/%1/info").arg(carId);
        MqttService::instance().subscribeTopic(topic, 0,
            [this, carId](const QString &, const QByteArray &payload) {
                onMqttMessage(QString("/rob/%1/info").arg(carId), payload);
            });
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

    if (topic.contains("/info")) {
        QString carId;
        // 从topic提取carId: /rob/{carId}/info
        QRegularExpression re("/rob/(.+)/info");
        QRegularExpressionMatch match = re.match(topic);
        if (match.hasMatch()) carId = match.captured(1);

        if (!carId.isEmpty() && m_carInfos.contains(carId)) {
            CarInfo &info = m_carInfos[carId];
            // MQTT /rob/{id}/info 字段使用下划线命名: car_x, car_y, car_yaw
            info.x = data.value("car_x").toDouble();
            info.y = data.value("car_y").toDouble();
            info.angle = data.value("car_yaw").toDouble();
            info.speed = data.value("speed").toDouble();
            info.battery = data.value("battery").toDouble();
            info.status = data.value("status").toString();
            info.taskId = jsonIdStr(data.value("task_id"));

            // 打印MQTT原始数据用于调试 (前5次)
            static int mqttLogCount = 0;
            if (mqttLogCount < 5) {
                qDebug() << "===== MQTT /rob/" << carId << "/info =====";
                qDebug() << "  car_x:" << info.x << "car_y:" << info.y
                         << "car_yaw:" << info.angle;
                qDebug() << "  battery:" << info.battery << "speed:" << info.speed;
                QPointF sp = worldToScene(info.x, info.y);
                qDebug() << "  -> scene: (" << sp.x() << "," << sp.y() << ")";
                mqttLogCount++;
            }

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
                    m_covarianceLabel->setText(QString::number(cov, 'f', 1) + "%");
                    if (cov <= 40) {
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
