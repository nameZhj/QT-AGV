#include "CreateMapWidget.h"
#include "core/AppConfig.h"
#include "core/MqttService.h"
#include "core/ApiService.h"
#include "core/LoadingDialog.h"
#include "core/Toast.h"
#include "core/TeleopController.h"
#include "DirectionPadWidget.h"
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

// 检查 API 返回是否报错: msg 值为 "failed" (字符串) 或 false (布尔) 时表示失败
// 校验接口返回是否有错误，如果有则返回详细错误提示，成功则返回空字符串
// 支持校验格式:
// 1. 根级: {code: 200, msg: "failed"/false, error_info: "..."}
// 2. data嵌套: {data: {msg: "failed", info: "建图失败，已启动导航模式", error_info: "建图进程 启动失败: 未知错误"}}
static QString checkApiError(const QJsonObject &res)
{
    QJsonObject dataObj;
    if (res.value("data").isObject()) {
        dataObj = res.value("data").toObject();
    }

    auto isFailedVal = [](const QJsonValue &v) -> bool {
        if (v.isString()) {
            QString s = v.toString().trimmed();
            return (s.compare("failed", Qt::CaseInsensitive) == 0 ||
                    s.compare("error", Qt::CaseInsensitive) == 0 ||
                    s.compare("fail", Qt::CaseInsensitive) == 0);
        } else if (v.isBool()) {
            return !v.toBool(); // false 表示失败
        }
        return false;
    };

    bool isFailed = false;
    // 检查根级 msg 以及 data 内部的 msg 是否为 failed/false
    if (isFailedVal(res.value("msg")) || isFailedVal(dataObj.value("msg"))) {
        isFailed = true;
    }

    // 检查 success 字段 (如有)
    if (res.contains("success") && !res.value("success").toBool()) isFailed = true;
    if (dataObj.contains("success") && !dataObj.value("success").toBool()) isFailed = true;

    // 检查 error_info 是否非空
    QString errInfoStr;
    if (dataObj.value("error_info").isString()) {
        errInfoStr = dataObj.value("error_info").toString().trimmed();
    } else if (res.value("error_info").isString()) {
        errInfoStr = res.value("error_info").toString().trimmed();
    }
    if (!errInfoStr.isEmpty() && errInfoStr.compare("none", Qt::CaseInsensitive) != 0) {
        isFailed = true;
    }

    if (!isFailed) return QString();

    // 提取 info 提示
    QString infoStr;
    if (dataObj.value("info").isString()) {
        infoStr = dataObj.value("info").toString().trimmed();
    } else if (res.value("info").isString()) {
        infoStr = res.value("info").toString().trimmed();
    }

    // 组合完整的错误提示: "info: error_info"
    if (!infoStr.isEmpty() && !errInfoStr.isEmpty() && infoStr != errInfoStr) {
        return QString("%1: %2").arg(infoStr, errInfoStr);
    }
    if (!errInfoStr.isEmpty()) return errInfoStr;
    if (!infoStr.isEmpty())    return infoStr;

    // 尝试 message / msg 字段
    QString msgStr = res.value("message").toString().trimmed();
    if (msgStr.isEmpty()) msgStr = dataObj.value("message").toString().trimmed();
    if (!msgStr.isEmpty()) return msgStr;

    return QStringLiteral("操作失败");
}

// ── Style constants (secondary-page header, matches MapEditWidget/TaskEditWidget) ──
static const char *S_TOPBAR =
    "background:#1e2d3d; border-bottom:1px solid rgba(255,255,255,0.06);";

CreateMapWidget::CreateMapWidget(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);

    setupUI();

    // 同步全局 TeleopController 状态到本页"启用遥控"按钮与 HUD
    connect(&TeleopController::instance(), &TeleopController::activeChanged,
            this, &CreateMapWidget::onTeleopActiveChanged);
    connect(&TeleopController::instance(), &TeleopController::brakeChanged,
            this, &CreateMapWidget::onTeleopBrakeChanged);
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
    connect(backBtn, &QPushButton::clicked, this, [this]() {
        if (!tryExitMapping()) return;
        emit goBack();
    });
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

    // 方向控制盘 (左下角常驻，对标参考图1)
    m_directionPad = new DirectionPadWidget(m_mapCanvas);
    m_directionPad->move(16, qMax(16, m_mapCanvas->height() - m_directionPad->height() - 16));
    m_directionPad->raise();
    m_mapCanvas->installEventFilter(this);

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
    // 同步初始状态（避免与全局 TeleopController 不一致）
    m_teleopEnableBtn->setChecked(TeleopController::instance().isActive());
    m_teleopEnableBtn->setText(TeleopController::instance().isActive() ? "停用遥控" : "启用遥控");
    m_teleopLabel->setText(TeleopController::instance().isActive() ? "已启用" : "已停用");
    // 点击委托给全局 TeleopController；状态回流由 onTeleopActiveChanged 同步
    connect(m_teleopEnableBtn, &QPushButton::clicked, this, [this]() {
        // 阻止 toggled 默认行为：以 TeleopController 为单一真源
        bool target = !TeleopController::instance().isActive();
        if (!MqttService::instance().isConnected()) {
            Toast::error(this, "MQTT 未连接，无法启用遥控");
            // 立即回滚勾选状态
            QMetaObject::invokeMethod(this, [this]() {
                m_teleopEnableBtn->setChecked(TeleopController::instance().isActive());
            }, Qt::QueuedConnection);
            return;
        }
        if (AppConfig::instance().agvId().isEmpty()) {
            Toast::error(this, "未选择设备，无法启用遥控");
            QMetaObject::invokeMethod(this, [this]() {
                m_teleopEnableBtn->setChecked(TeleopController::instance().isActive());
            }, Qt::QueuedConnection);
            return;
        }
        TeleopController::instance().setActive(target);
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

    // 遥控按钮 - 统一委托给全局 TeleopController (消除影子逻辑与多协议冲突)
    auto setupDirBtn = [](QPushButton *btn, int key) {
        QObject::connect(btn, &QPushButton::pressed, [key]() {
            TeleopController::instance().setVirtualKey(key, true);
        });
        QObject::connect(btn, &QPushButton::released, [key]() {
            TeleopController::instance().setVirtualKey(key, false);
        });
    };
    setupDirBtn(m_forwardBtn, Qt::Key_W);
    setupDirBtn(m_backwardBtn, Qt::Key_S);
    setupDirBtn(m_leftBtn, Qt::Key_A);
    setupDirBtn(m_rightBtn, Qt::Key_D);
    setupDirBtn(m_brakeBtn, Qt::Key_Space);

    teleopLayout->addLayout(dirLayout);

    QLabel *kbHint = new QLabel(
        "<small style='color:#7a8a99;'>"
        "键盘: W A S D 移动 &nbsp;|&nbsp; Space 刹车<br>"
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
    // 闲置状态(非建图中)不显示地图，避免上次建图的残留画面
    if (!m_mappingActive && !m_highMappingActive) {
        m_mapCanvas->setMapData(QJsonObject());
    }

    if (m_directionPad) {
        m_directionPad->move(16, qMax(16, m_mapCanvas->height() - m_directionPad->height() - 16));
        m_directionPad->raise();
    }
}

bool CreateMapWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_mapCanvas && event->type() == QEvent::Resize) {
        if (m_directionPad) {
            m_directionPad->move(16, qMax(16, m_mapCanvas->height() - m_directionPad->height() - 16));
            m_directionPad->raise();
        }
    }
    return QWidget::eventFilter(watched, event);
}

void CreateMapWidget::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    // 注：全局键盘遥控由 TeleopController 管理，不随页面隐藏而停止
    disconnectMqtt();
}

void CreateMapWidget::onTeleopActiveChanged(bool active)
{
    if (!m_teleopEnableBtn) return;
    // 单一真源：TeleopController；按钮状态以它为准
    QSignalBlocker blocker(m_teleopEnableBtn);
    m_teleopEnableBtn->setChecked(active);
    m_teleopEnableBtn->setText(active ? "停用遥控" : "启用遥控");
    m_teleopLabel->setText(active ? "已启用" : "已停用");
}

void CreateMapWidget::onTeleopBrakeChanged(bool brake)
{
    if (m_brakeLabel) m_brakeLabel->setText(brake ? "按下(键盘)" : "松开");
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
    ApiService::instance().startMapping(name.trimmed(), [this, loading](int code, const QJsonObject &res) {
        loading->close();
        loading->deleteLater();
        if (code == 200) {
            QString err = checkApiError(res);
            if (!err.isEmpty()) {
                QMessageBox::warning(this, "错误", err);
                return;
            }
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
        ApiService::instance().saveMap([this, loading](int code, const QJsonObject &res) {
            loading->close();
            loading->deleteLater();
            if (code == 200) {
                QString err = checkApiError(res);
                if (!err.isEmpty()) {
                    QMessageBox::warning(this, "错误", err);
                    return;
                }
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
        ApiService::instance().cancelMapping([this, loading](int code, const QJsonObject &res) {
            loading->close();
            loading->deleteLater();
            if (code == 200) {
                QString err = checkApiError(res);
                if (!err.isEmpty()) {
                    QMessageBox::warning(this, "错误", err);
                    return;
                }
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
    ApiService::instance().startReflectorMapping([this, loading](int code, const QJsonObject &res) {
        loading->close();
        loading->deleteLater();
        if (code == 200) {
            QString err = checkApiError(res);
            if (!err.isEmpty()) {
                QMessageBox::warning(this, "错误", err);
                return;
            }
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
        ApiService::instance().stopReflectorMapping([this, loading](int code, const QJsonObject &res) {
            loading->close();
            loading->deleteLater();
            if (code == 200) {
                QString err = checkApiError(res);
                if (!err.isEmpty()) {
                    QMessageBox::warning(this, "错误", err);
                    return;
                }
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
        ApiService::instance().cancelReflectorMapping([this, loading](int code, const QJsonObject &res) {
            loading->close();
            loading->deleteLater();
            if (code == 200) {
                QString err = checkApiError(res);
                if (!err.isEmpty()) {
                    QMessageBox::warning(this, "错误", err);
                    return;
                }
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
    ApiService::instance().resetReflectorAll([this, loading](int code, const QJsonObject &res) {
        loading->close();
        loading->deleteLater();
        if (code == 200) {
            QString err = checkApiError(res);
            if (!err.isEmpty()) { QMessageBox::warning(this, "错误", err); return; }
            Toast::success(this, "全局重置反光柱成功");
        } else
            QMessageBox::warning(this, "错误", "全局重置反光柱失败");
    });
}

void CreateMapWidget::onResetReflectorLocal()
{
    auto *loading = new LoadingDialog("正在局部重置反光柱...", this);
    loading->show();
    ApiService::instance().resetReflectorLocal([this, loading](int code, const QJsonObject &res) {
        loading->close();
        loading->deleteLater();
        if (code == 200) {
            QString err = checkApiError(res);
            if (!err.isEmpty()) { QMessageBox::warning(this, "错误", err); return; }
            Toast::success(this, "局部重置反光柱成功");
        } else
            QMessageBox::warning(this, "错误", "局部重置反光柱失败");
    });
}

void CreateMapWidget::onToggleCruiseClick()
{
    if (!m_cruiseSuspended) {
        auto *loading = new LoadingDialog("正在暂停巡航...", this);
        loading->show();
        ApiService::instance().controlNavigation(QStringLiteral("pause navigation"), [this, loading](int code, const QJsonObject &res) {
            loading->close();
            loading->deleteLater();
            if (code == 200) {
                QString err = checkApiError(res);
                if (!err.isEmpty()) {
                    QMessageBox::warning(this, "错误", err);
                    return;
                }
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
        ApiService::instance().controlNavigation(QStringLiteral("continue navigation"), [this, loading](int code, const QJsonObject &res) {
            loading->close();
            loading->deleteLater();
            if (code == 200) {
                QString err = checkApiError(res);
                if (!err.isEmpty()) {
                    QMessageBox::warning(this, "错误", err);
                    return;
                }
                m_cruiseSuspended = false;
                updateToolbar();
                Toast::success(this, "巡航已恢复");
            } else {
                QMessageBox::warning(this, "错误", "恢复巡航失败");
            }
        });
    }
}

bool CreateMapWidget::tryExitMapping()
{
    // 未在建图中 → 直接允许离开
    if (!m_mappingActive && !m_highMappingActive) return true;

    // 建图中途退出 → 弹出保存提示
    QMessageBox msgBox(this);
    if (m_highMappingActive) {
        msgBox.setWindowTitle("保存高阶地图");
        msgBox.setText("是否保存当前高阶建图地图？");
    } else {
        msgBox.setWindowTitle("保存自适应地图");
        msgBox.setText("是否保存当前自适应建图地图？");
    }
    
    QPushButton *saveBtn   = msgBox.addButton("保存地图", QMessageBox::YesRole);
    QPushButton *cancelBtn = msgBox.addButton("取消保存", QMessageBox::NoRole);
    QPushButton *abortBtn  = msgBox.addButton("返回", QMessageBox::RejectRole);
    msgBox.exec();
    auto *clicked = msgBox.clickedButton();

    if (clicked == abortBtn) return false;   // 用户选择返回，不离开

    if (clicked == saveBtn) {
        // 保存地图: 调用对应接口，成功后才允许离开
        QString loadingText = m_highMappingActive ? "正在保存高阶地图，请稍候..." : "正在保存自适应地图，请稍候...";
        auto *loading = new LoadingDialog(loadingText, window(), Qt::ApplicationModal);
        loading->show();
        if (m_highMappingActive) {
            ApiService::instance().stopReflectorMapping([this, loading](int code, const QJsonObject &res) {
                loading->close();
                loading->deleteLater();
                if (code == 200) {
                    QString err = checkApiError(res);
                    if (!err.isEmpty()) {
                        QMessageBox::warning(this, "错误", err);
                        return;
                    }
                    m_mappingActive = false;
                    m_highMappingActive = false;
                    m_mapCanvas->setHighMappingActive(false);
                    updateToolbar();
                    Toast::success(this, "高阶地图保存成功");
                    emit goBack();
                } else {
                    QMessageBox::warning(this, "错误", "保存高阶地图失败，请重试");
                }
            });
        } else {
            ApiService::instance().saveMap([this, loading](int code, const QJsonObject &res) {
                loading->close();
                loading->deleteLater();
                if (code == 200) {
                    QString err = checkApiError(res);
                    if (!err.isEmpty()) {
                        QMessageBox::warning(this, "错误", err);
                        return;
                    }
                    m_mappingActive = false;
                    m_highMappingActive = false;
                    m_mapCanvas->setHighMappingActive(false);
                    updateToolbar();
                    Toast::success(this, "自适应地图保存成功");
                    emit goBack();
                } else {
                    QMessageBox::warning(this, "错误", "保存自适应地图失败，请重试");
                }
            });
        }
        return false;   // API 返回前阻止离开，回调中成功后 emit goBack
    }

    // 取消保存: 调用取消接口，成功后才允许离开
    QString cancelText = m_highMappingActive ? "正在取消高阶建图，请稍候..." : "正在取消自适应建图，请稍候...";
    auto *loading = new LoadingDialog(cancelText, window(), Qt::ApplicationModal);
    loading->show();
    if (m_highMappingActive) {
        ApiService::instance().cancelReflectorMapping([this, loading](int code, const QJsonObject &res) {
            loading->close();
            loading->deleteLater();
            if (code == 200) {
                QString err = checkApiError(res);
                if (!err.isEmpty()) {
                    QMessageBox::warning(this, "错误", err);
                    return;
                }
                m_mappingActive = false;
                m_highMappingActive = false;
                m_mapCanvas->setHighMappingActive(false);
                updateToolbar();
                Toast::success(this, "高阶建图已取消");
                emit goBack();
            } else {
                QMessageBox::warning(this, "错误", "取消高阶建图失败，请重试");
            }
        });
    } else {
        ApiService::instance().cancelMapping([this, loading](int code, const QJsonObject &res) {
            loading->close();
            loading->deleteLater();
            if (code == 200) {
                QString err = checkApiError(res);
                if (!err.isEmpty()) {
                    QMessageBox::warning(this, "错误", err);
                    return;
                }
                m_mappingActive = false;
                m_highMappingActive = false;
                m_mapCanvas->setHighMappingActive(false);
                updateToolbar();
                Toast::success(this, "自适应建图已取消");
                emit goBack();
            } else {
                QMessageBox::warning(this, "错误", "取消自适应建图失败，请重试");
            }
        });
    }
    return false;   // API 返回前阻止离开，回调中成功后 emit goBack
}
