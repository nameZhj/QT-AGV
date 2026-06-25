#include "CreateMapWidget.h"
#include "core/AppConfig.h"
#include "core/MqttService.h"
#include "core/ApiService.h"
#include "core/LoadingDialog.h"
#include "core/Toast.h"
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QJsonDocument>
#include <QSplitter>
#include <QGroupBox>
#include <QPainter>
#include <QFrame>

// ── Style constants (secondary-page header, matches MapEditWidget/TaskEditWidget) ──
static const char *S_TOPBAR =
    "background:#1e2d3d; border-bottom:1px solid rgba(255,255,255,0.06);";

CreateMapWidget::CreateMapWidget(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);

    m_teleopTimer = new QTimer(this);
    m_teleopTimer->setInterval(100);
    connect(m_teleopTimer, &QTimer::timeout, this, &CreateMapWidget::onTeleopTick);

    setupUI();
}

void CreateMapWidget::setupUI()
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header bar: back button + title (secondary-page style) ─────────────
    QWidget *headerBar = new QWidget;
    headerBar->setFixedHeight(44);
    headerBar->setStyleSheet(S_TOPBAR);
    QHBoxLayout *hl = new QHBoxLayout(headerBar);
    hl->setContentsMargins(8, 4, 8, 4);
    hl->setSpacing(8);

    QPushButton *backBtn = new QPushButton("← 返回");
    backBtn->setStyleSheet(
        "QPushButton{color:white;border:none;border-radius:4px;"
        "padding:4px 12px;font-size:12px;background:transparent;}"
        "QPushButton:hover{background:rgba(255,255,255,18);}");
    backBtn->setFocusPolicy(Qt::NoFocus);
    connect(backBtn, &QPushButton::clicked, this, &CreateMapWidget::goBack);
    hl->addWidget(backBtn);

    QFrame *div = new QFrame; div->setFrameShape(QFrame::VLine);
    div->setStyleSheet("color:rgba(255,255,255,0.08);");
    hl->addWidget(div);

    QLabel *titleLabel = new QLabel("创建地图");
    titleLabel->setStyleSheet("color:#9ab8d8;font-weight:bold;font-size:13px;");
    hl->addWidget(titleLabel);
    hl->addStretch();

    root->addWidget(headerBar);

    // ── Content area (existing toolbar + map canvas/HUD) ────────────────────
    QWidget *content = new QWidget;
    QVBoxLayout *mainLayout = new QVBoxLayout(content);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    root->addWidget(content, 1);

    // 工具栏
    QHBoxLayout *toolbar = new QHBoxLayout;
    m_adaptiveBtn = new QPushButton("自适应建图");
    m_highBtn = new QPushButton("高阶建图");
    m_stopAdaptiveBtn = new QPushButton("停止自适应建图");
    m_resetAllBtn = new QPushButton("全局重置反光柱");
    m_resetLocalBtn = new QPushButton("局部重置反光柱");
    m_stopHighBtn = new QPushButton("停止高阶建图");
    m_cruiseBtn = new QPushButton("暂停巡航");

    for (auto btn : {m_adaptiveBtn, m_highBtn, m_stopAdaptiveBtn, m_resetAllBtn, m_resetLocalBtn, m_stopHighBtn, m_cruiseBtn}) {
        btn->setStyleSheet("QPushButton { padding: 6px 12px; background: #007bff; color: white; border: none; border-radius: 4px; }"
                           "QPushButton:hover { background: #0056b3; }"
                           "QPushButton:disabled { opacity: 0.5; }");
        btn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        // 不允许工具栏按钮抢占键盘焦点，否则鼠标点击后方向键会变成"切换焦点"而不是遥控
        btn->setFocusPolicy(Qt::NoFocus);
        toolbar->addWidget(btn);
    }
    toolbar->addStretch();

    connect(m_adaptiveBtn, &QPushButton::clicked, this, &CreateMapWidget::onStartAdaptiveClick);
    connect(m_highBtn, &QPushButton::clicked, this, &CreateMapWidget::onStartHighClick);
    connect(m_stopAdaptiveBtn, &QPushButton::clicked, this, &CreateMapWidget::onStopAdaptiveClick);
    connect(m_stopHighBtn, &QPushButton::clicked, this, &CreateMapWidget::onStopHighClick);
    connect(m_resetAllBtn, &QPushButton::clicked, this, &CreateMapWidget::onResetReflectorAll);
    connect(m_resetLocalBtn, &QPushButton::clicked, this, &CreateMapWidget::onResetReflectorLocal);
    connect(m_cruiseBtn, &QPushButton::clicked, this, &CreateMapWidget::onToggleCruiseClick);

    mainLayout->addLayout(toolbar);

    // 主内容区
    QSplitter *splitter = new QSplitter(Qt::Horizontal);

    // 地图画布
    m_mapCanvas = new MapCanvasWidget;
    m_mapCanvas->setStyleSheet("background: #1a1a2e;");
    splitter->addWidget(m_mapCanvas);

    // 右侧面板 (HUD + 遥控)
    QWidget *rightPanel = new QWidget;
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightPanel->setMaximumWidth(280);

    // HUD
    QGroupBox *hudGroup = new QGroupBox("设备状态");
    QFormLayout *hudLayout = new QFormLayout(hudGroup);
    m_linearLabel = new QLabel("-- m/s");
    m_angularLabel = new QLabel("-- rad/s");
    m_teleopLabel = new QLabel("已停用");
    m_brakeLabel = new QLabel("松开");
    hudLayout->addRow("线速度:", m_linearLabel);
    hudLayout->addRow("角速度:", m_angularLabel);
    hudLayout->addRow("遥控:", m_teleopLabel);
    hudLayout->addRow("刹车:", m_brakeLabel);
    rightLayout->addWidget(hudGroup);

    // 遥控面板
    QGroupBox *teleopGroup = new QGroupBox("遥控控制");
    QVBoxLayout *teleopLayout = new QVBoxLayout(teleopGroup);

    m_teleopEnableBtn = new QPushButton("启用遥控");
    m_teleopEnableBtn->setCheckable(true);
    m_teleopEnableBtn->setFocusPolicy(Qt::NoFocus);
    connect(m_teleopEnableBtn, &QPushButton::toggled, this, [this](bool checked) {
        m_teleopEnabled = checked;
        m_teleopEnableBtn->setText(checked ? "停用遥控" : "启用遥控");
        m_teleopLabel->setText(checked ? "已启用" : "已停用");
        // 通过 MQTT 发送遥控启用/停用
        QString agvId = AppConfig::instance().agvId();
        MqttService::instance().publish(
            QString("/rob/%1/teleop_enable").arg(agvId),
            checked ? "1" : "0");
    });
    teleopLayout->addWidget(m_teleopEnableBtn);

    // 方向按钮
    QGridLayout *dirLayout = new QGridLayout;
    m_forwardBtn = new QPushButton("前进");
    m_backwardBtn = new QPushButton("后退");
    m_leftBtn = new QPushButton("左转");
    m_rightBtn = new QPushButton("右转");
    m_brakeBtn = new QPushButton("刹车");
    m_brakeBtn->setStyleSheet("QPushButton { background: #f44336; color: white; padding: 8px; }");

    // 不允许遥控方向按钮抢占键盘焦点，否则点击后按方向键只会移动焦点框
    for (auto btn : {m_forwardBtn, m_backwardBtn, m_leftBtn, m_rightBtn, m_brakeBtn})
        btn->setFocusPolicy(Qt::NoFocus);

    dirLayout->addWidget(m_forwardBtn, 0, 1);
    dirLayout->addWidget(m_leftBtn, 1, 0);
    dirLayout->addWidget(m_brakeBtn, 1, 1);
    dirLayout->addWidget(m_rightBtn, 1, 2);
    dirLayout->addWidget(m_backwardBtn, 2, 1);

    // 遥控按钮 - 按下发送速度，松开发送0
    auto setupDirBtn = [](QPushButton *btn, double linear, double angular) {
        QObject::connect(btn, &QPushButton::pressed, [linear, angular]() {
            QString agvId = AppConfig::instance().agvId();
            QJsonObject cmd;
            cmd["linear_x"] = linear;
            cmd["angular_z"] = angular;
            MqttService::instance().publish(
                QString("/rob/%1/cmd_vel").arg(agvId),
                QJsonDocument(cmd).toJson());
        });
        QObject::connect(btn, &QPushButton::released, []() {
            QString agvId = AppConfig::instance().agvId();
            QJsonObject cmd;
            cmd["linear_x"] = 0.0;
            cmd["angular_z"] = 0.0;
            MqttService::instance().publish(
                QString("/rob/%1/cmd_vel").arg(agvId),
                QJsonDocument(cmd).toJson());
        });
    };
    setupDirBtn(m_forwardBtn, 0.3, 0.0);
    setupDirBtn(m_backwardBtn, -0.3, 0.0);
    setupDirBtn(m_leftBtn, 0.0, 0.5);
    setupDirBtn(m_rightBtn, 0.0, -0.5);

    connect(m_brakeBtn, &QPushButton::pressed, this, [this]() {
        m_brakeActive = true;
        m_brakeLabel->setText("按下");
        QString agvId = AppConfig::instance().agvId();
        MqttService::instance().publish(QString("/rob/%1/brake").arg(agvId), "1");
    });
    connect(m_brakeBtn, &QPushButton::released, this, [this]() {
        m_brakeActive = false;
        m_brakeLabel->setText("松开");
        QString agvId = AppConfig::instance().agvId();
        MqttService::instance().publish(QString("/rob/%1/brake").arg(agvId), "0");
    });

    teleopLayout->addLayout(dirLayout);

    QLabel *kbHint = new QLabel(
        "<small style='color:#7a8a99;'>"
        "键盘: ↑↓←→ 移动 &nbsp;|&nbsp; Space 刹车<br>"
        "Ctrl 加速 &nbsp;|&nbsp; P 升降上 &nbsp;|&nbsp; L 升降下"
        "</small>");
    kbHint->setTextFormat(Qt::RichText);
    kbHint->setWordWrap(true);
    kbHint->setAlignment(Qt::AlignCenter);
    teleopLayout->addWidget(kbHint);

    rightLayout->addWidget(teleopGroup);
    rightLayout->addStretch();

    splitter->addWidget(rightPanel);
    splitter->setSizes({800, 280});

    mainLayout->addWidget(splitter);
    updateToolbar();
}

void CreateMapWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    initMqtt();
    setFocus();
    m_teleopTimer->start();
}

void CreateMapWidget::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    m_teleopTimer->stop();
    m_keyForward = m_keyBack = m_keyLeft = m_keyRight = false;
    m_keyBrake = m_keyBoost = m_keyHoistUp = m_keyHoistDown = false;
    m_hasPublishedLinear = m_hasPublishedAngular = false;
    disconnectMqtt();
}

void CreateMapWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat()) { event->accept(); return; }
    switch (event->key()) {
    case Qt::Key_Up:    m_keyForward  = true; break;
    case Qt::Key_Down:  m_keyBack     = true; break;
    case Qt::Key_Left:  m_keyLeft     = true; break;
    case Qt::Key_Right: m_keyRight    = true; break;
    case Qt::Key_Space: m_keyBrake    = true; break;
    case Qt::Key_Control: m_keyBoost  = true; break;
    case Qt::Key_P:     m_keyHoistUp  = true; break;
    case Qt::Key_L:     m_keyHoistDown = true; break;
    default: QWidget::keyPressEvent(event); return;
    }
    event->accept();
    updateTeleopMotion();
}

void CreateMapWidget::keyReleaseEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat()) { event->accept(); return; }
    switch (event->key()) {
    case Qt::Key_Up:    m_keyForward  = false; break;
    case Qt::Key_Down:  m_keyBack     = false; break;
    case Qt::Key_Left:  m_keyLeft     = false; break;
    case Qt::Key_Right: m_keyRight    = false; break;
    case Qt::Key_Space: m_keyBrake    = false; break;
    case Qt::Key_Control: m_keyBoost  = false; break;
    case Qt::Key_P:     m_keyHoistUp  = false; break;
    case Qt::Key_L:     m_keyHoistDown = false; break;
    default: QWidget::keyReleaseEvent(event); return;
    }
    event->accept();
    updateTeleopMotion();
}

void CreateMapWidget::onTeleopTick()
{
    if (!m_teleopEnabled) return;

    updateTeleopMotion();

    // Hoist：按住期间持续发送（与速度去重逻辑无关）
    QString agvId = AppConfig::instance().agvId();
    if (m_keyHoistUp)
        MqttService::instance().publish(QString("/rob/%1/hoisting_status").arg(agvId), "1");
    else if (m_keyHoistDown)
        MqttService::instance().publish(QString("/rob/%1/hoisting_status").arg(agvId), "0");
}

// 参考 Vue TeleopControls：方向键速度按"值变化"去重发送（按下/松开各发一次，
// 持续按住不重复发送）；空格刹车则在按住期间每个 tick 都强制发0，松开后因目标值
// 仍是0、与上次发布值相同，去重逻辑会自然停止继续发送。
void CreateMapWidget::updateTeleopMotion()
{
    if (!m_teleopEnabled) return;

    QString agvId = AppConfig::instance().agvId();

    if (m_keyBrake) {
        MqttService::instance().publish(
            QString("/rob/%1/linear_speed").arg(agvId),
            QJsonDocument(QJsonObject{{"velocity", 0.0}}).toJson(QJsonDocument::Compact));
        MqttService::instance().publish(
            QString("/rob/%1/angular_speed").arg(agvId),
            QJsonDocument(QJsonObject{{"velocity", 0.0}}).toJson(QJsonDocument::Compact));
        m_lastPublishedLinear = 0.0;
        m_lastPublishedAngular = 0.0;
        m_hasPublishedLinear = m_hasPublishedAngular = true;

        m_brakeLabel->setText("按下(键盘)");
        m_linearLabel->setText("0.00 m/s");
        m_angularLabel->setText("0.00 rad/s");
        return;
    }
    m_brakeLabel->setText("松开");

    constexpr double kBase = 0.5;
    constexpr double kAngBase = 0.3;
    const double mult = m_keyBoost ? 2.0 : 1.0;

    double linear  = 0.0;
    double angular = 0.0;
    if (m_keyForward)  linear  =  kBase * mult;
    if (m_keyBack)     linear  = -kBase * mult;
    if (m_keyLeft)     angular =  kAngBase * mult;
    if (m_keyRight)    angular = -kAngBase * mult;

    const double linRounded = qRound(linear * 100.0) / 100.0;
    const double angRounded = qRound(angular * 100.0) / 100.0;

    if (!m_hasPublishedLinear || linRounded != m_lastPublishedLinear) {
        MqttService::instance().publish(
            QString("/rob/%1/linear_speed").arg(agvId),
            QJsonDocument(QJsonObject{{"velocity", linear}}).toJson(QJsonDocument::Compact));
        m_lastPublishedLinear = linRounded;
        m_hasPublishedLinear = true;
    }
    if (!m_hasPublishedAngular || angRounded != m_lastPublishedAngular) {
        MqttService::instance().publish(
            QString("/rob/%1/angular_speed").arg(agvId),
            QJsonDocument(QJsonObject{{"velocity", angular}}).toJson(QJsonDocument::Compact));
        m_lastPublishedAngular = angRounded;
        m_hasPublishedAngular = true;
    }

    m_linearLabel->setText(QString::number(linear, 'f', 2) + " m/s");
    m_angularLabel->setText(QString::number(angular, 'f', 2) + " rad/s");
}

void CreateMapWidget::initMqtt()
{
    auto &mqtt = MqttService::instance();
    if (!mqtt.isConnected()) {
        QString ip = AppConfig::instance().serverIp();
        mqtt.connectToHost(ip, 1883);  // MqttService内部会映射到6016
    }

    QString agvId = AppConfig::instance().agvId();
    if (!agvId.isEmpty()) {
        QString base = QString("/rob/%1/").arg(agvId);
        mqtt.subscribeTopic(base + "mapping", 0, [this](const QString &t, const QByteArray &m) {
            onMqttMessage(t, m);
        });
        mqtt.subscribeTopic(base + "carto_pose", 0, [this](const QString &t, const QByteArray &m) {
            onMqttMessage(t, m);
        });
        mqtt.subscribeTopic(base + "markers", 0, [this](const QString &t, const QByteArray &m) {
            onMqttMessage(t, m);
        });
    }
}

void CreateMapWidget::disconnectMqtt()
{
    auto &mqtt = MqttService::instance();
    QString agvId = AppConfig::instance().agvId();
    if (mqtt.isConnected() && !agvId.isEmpty()) {
        mqtt.publish(QString("/rob/%1/navigationContinue").arg(agvId), "1");
        if (m_mappingActive) {
            mqtt.publish(QString("/rob/%1/cancel").arg(agvId), "1");
        } else if (m_highMappingActive) {
            mqtt.publish(QString("/rob/%1/cancel").arg(agvId), "2");
        }
    }

    // 取消订阅主题
    if (!agvId.isEmpty()) {
        QString base = QString("/rob/%1/").arg(agvId);
        mqtt.unsubscribeTopic(base + "mapping");
        mqtt.unsubscribeTopic(base + "carto_pose");
        mqtt.unsubscribeTopic(base + "markers");
    }
}

void CreateMapWidget::onMqttMessage(const QString &topic, const QByteArray &payload)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
    if (err.error != QJsonParseError::NoError) return;
    QJsonObject data = doc.object();

    if (topic.contains("/mapping")) {
        m_mapData = data;
        m_mapCanvas->setMapData(data);
        return;
    }
    if (topic.contains("/carto_pose")) {
        m_robotPose = data;
        m_mapCanvas->setRobotPose(data);
        double lv = data.value("linear_velocity").toDouble(0);
        double av = data.value("angular_velocity").toDouble(0);
        m_linearLabel->setText(QString::number(lv, 'f', 2) + " m/s");
        m_angularLabel->setText(QString::number(av, 'f', 2) + " rad/s");
        return;
    }
    if (topic.contains("/markers")) {
        if (doc.isArray()) m_poles = doc.array();
        else m_poles = data.value("markers").toArray();
        m_mapCanvas->setPoles(m_poles);
        return;
    }
}

void CreateMapWidget::updateToolbar()
{
    bool anyActive = m_mappingActive || m_highMappingActive;
    m_adaptiveBtn->setVisible(!anyActive);
    m_highBtn->setVisible(!anyActive);
    m_stopAdaptiveBtn->setVisible(m_mappingActive);
    m_resetAllBtn->setVisible(m_highMappingActive);
    m_resetLocalBtn->setVisible(m_highMappingActive);
    m_stopHighBtn->setVisible(m_highMappingActive);
    m_cruiseBtn->setText(m_cruiseSuspended ? "恢复巡航" : "暂停巡航");
}

void CreateMapWidget::onStartAdaptiveClick()
{
    QInputDialog dlg(this);
    dlg.setWindowTitle("请输入地图名称");
    dlg.setLabelText("地图名称:");
    dlg.setTextValue("");
    dlg.setOkButtonText("确定");
    dlg.setCancelButtonText("取消");
    bool ok = dlg.exec() == QDialog::Accepted;
    QString name = dlg.textValue();
    if (!ok || name.trimmed().isEmpty()) {
        if (ok) QMessageBox::warning(this, "提示", "地图名称不能为空");
        return;
    }
    auto *loading = new LoadingDialog("正在启动建图，请稍候...", window(), Qt::ApplicationModal);
    loading->show();
    ApiService::instance().startMapping(name.trimmed(), [this, loading](int code, const QJsonObject &) {
        loading->close();
        loading->deleteLater();
        if (code == 200) {
            m_mappingActive = true;
            updateToolbar();
            Toast::success(this, "建图已启动");
        } else {
            QMessageBox::warning(this, "错误", "启动建图失败");
        }
    });
}

void CreateMapWidget::onStopAdaptiveClick()
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("保存地图");
    msgBox.setText("是否保存当前自适应建图地图？");
    QPushButton *saveBtn = msgBox.addButton("保存", QMessageBox::YesRole);
    QPushButton *noSaveBtn = msgBox.addButton("不保存", QMessageBox::NoRole);
    QPushButton *cancelBtn = msgBox.addButton("取消", QMessageBox::RejectRole);
    msgBox.exec();
    auto *clicked = msgBox.clickedButton();
    if (clicked == cancelBtn) return;

    if (clicked == saveBtn) {
        auto *loading = new LoadingDialog("正在保存地图，请稍候...", window(), Qt::ApplicationModal);
        loading->show();
        ApiService::instance().saveMap([this, loading](int code, const QJsonObject &) {
            loading->close();
            loading->deleteLater();
            if (code == 200) {
                m_mappingActive = false;
                m_highMappingActive = false;
                m_mapCanvas->setHighMappingActive(false);
                updateToolbar();
                Toast::success(this, "地图保存成功");
            } else {
                QMessageBox::warning(this, "错误", "保存地图失败");
            }
        });
    } else {
        auto *loading = new LoadingDialog("正在取消建图，请稍候...", window(), Qt::ApplicationModal);
        loading->show();
        ApiService::instance().cancelMapping([this, loading](int code, const QJsonObject &) {
            loading->close();
            loading->deleteLater();
            if (code == 200) {
                m_mappingActive = false;
                m_highMappingActive = false;
                m_mapCanvas->setHighMappingActive(false);
                updateToolbar();
                Toast::success(this, "建图已取消");
            } else {
                QMessageBox::warning(this, "错误", "取消建图失败");
            }
        });
    }
}

void CreateMapWidget::onStartHighClick()
{
    QMessageBox confirmBox(this);
    confirmBox.setWindowTitle("提示");
    confirmBox.setText("确定要开始高阶建图吗？");
    QPushButton *okBtn     = confirmBox.addButton("确定", QMessageBox::YesRole);
    QPushButton *cancelBtn = confirmBox.addButton("取消", QMessageBox::RejectRole);
    confirmBox.exec();
    if (confirmBox.clickedButton() != okBtn) { Q_UNUSED(cancelBtn); return; }

    auto *loading = new LoadingDialog("正在启动高阶建图，请稍候...", window(), Qt::ApplicationModal);
    loading->show();
    ApiService::instance().startReflectorMapping([this, loading](int code, const QJsonObject &) {
        loading->close();
        loading->deleteLater();
        if (code == 200) {
            m_highMappingActive = true;
            m_mapCanvas->setHighMappingActive(true);
            updateToolbar();
            Toast::success(this, "高阶建图已启动");
        } else {
            QMessageBox::warning(this, "错误", "启动高阶建图失败");
        }
    });
}

void CreateMapWidget::onStopHighClick()
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("保存高阶地图");
    msgBox.setText("是否保存当前高阶建图地图？");
    QPushButton *saveBtn = msgBox.addButton("保存", QMessageBox::YesRole);
    QPushButton *noSaveBtn = msgBox.addButton("不保存", QMessageBox::NoRole);
    QPushButton *cancelBtn = msgBox.addButton("取消", QMessageBox::RejectRole);
    msgBox.exec();
    auto *clicked = msgBox.clickedButton();
    if (clicked == cancelBtn) return;

    if (clicked == saveBtn) {
        auto *loading = new LoadingDialog("正在停止高阶建图，请稍候...", window(), Qt::ApplicationModal);
        loading->show();
        ApiService::instance().stopReflectorMapping([this, loading](int code, const QJsonObject &) {
            loading->close();
            loading->deleteLater();
            if (code == 200) {
                m_highMappingActive = false;
                m_mappingActive = false;
                updateToolbar();
                Toast::success(this, "高阶地图保存成功");
            } else {
                QMessageBox::warning(this, "错误", "停止高阶建图失败");
            }
        });
    } else {
        auto *loading = new LoadingDialog("正在取消高阶建图，请稍候...", window(), Qt::ApplicationModal);
        loading->show();
        ApiService::instance().cancelReflectorMapping([this, loading](int code, const QJsonObject &) {
            loading->close();
            loading->deleteLater();
            if (code == 200) {
                m_highMappingActive = false;
                m_mappingActive = false;
                updateToolbar();
                Toast::success(this, "高阶建图已取消");
            } else {
                QMessageBox::warning(this, "错误", "取消高阶建图失败");
            }
        });
    }
}

void CreateMapWidget::onResetReflectorAll()
{
    auto *loading = new LoadingDialog("正在全局重置反光柱...", this);
    loading->show();
    ApiService::instance().resetReflectorAll([this, loading](int code, const QJsonObject &) {
        loading->close();
        loading->deleteLater();
        if (code == 200) Toast::success(this, "全局重置反光柱成功");
        else QMessageBox::warning(this, "错误", "全局重置反光柱失败");
    });
}

void CreateMapWidget::onResetReflectorLocal()
{
    auto *loading = new LoadingDialog("正在局部重置反光柱...", this);
    loading->show();
    ApiService::instance().resetReflectorLocal([this, loading](int code, const QJsonObject &) {
        loading->close();
        loading->deleteLater();
        if (code == 200) Toast::success(this, "局部重置反光柱成功");
        else QMessageBox::warning(this, "错误", "局部重置反光柱失败");
    });
}

void CreateMapWidget::onToggleCruiseClick()
{
    if (!m_cruiseSuspended) {
        auto *loading = new LoadingDialog("正在暂停巡航...", this);
        loading->show();
        ApiService::instance().suspendWork([this, loading](int code, const QJsonObject &) {
            loading->close();
            loading->deleteLater();
            if (code == 200) {
                m_cruiseSuspended = true;
                updateToolbar();
                Toast::success(this, "巡航已暂停");
            } else {
                QMessageBox::warning(this, "错误", "暂停巡航失败");
            }
        });
    } else {
        auto *loading = new LoadingDialog("正在恢复巡航...", this);
        loading->show();
        ApiService::instance().continueWork([this, loading](int code, const QJsonObject &) {
            loading->close();
            loading->deleteLater();
            if (code == 200) {
                m_cruiseSuspended = false;
                updateToolbar();
                Toast::success(this, "巡航已恢复");
            } else {
                QMessageBox::warning(this, "错误", "恢复巡航失败");
            }
        });
    }
}
