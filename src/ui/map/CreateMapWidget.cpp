/**
 * @file    CreateMapWidget.cpp
 * @layer   GUI
 * @brief   SLAM online mapping view controller and HUD orchestration implementation.
 * @deps    CreateMapWidget.h, CreateMapWorkflowController.h, DirectionPadWidget.h, TeleopController.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "CreateMapWidget.h"
#include "app/AppConfig.h"
#include "network/MqttService.h"
#include "core/TeleopController.h"
#include "DirectionPadWidget.h"
#include "CreateMapStyle.h"
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QFrame>
#include <QEvent>
#include <QJsonDocument>

CreateMapWidget::CreateMapWidget(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    m_workflow = new CreateMapWorkflowController(this);

    setupUI();

    connect(&TeleopController::instance(), &TeleopController::activeChanged,
            this, &CreateMapWidget::onTeleopActiveChanged);
    connect(&TeleopController::instance(), &TeleopController::brakeChanged,
            this, &CreateMapWidget::onTeleopBrakeChanged);
}

void CreateMapWidget::setupUI()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    setupHeaderBar(root);

    auto *content = new QWidget;
    auto *mainLayout = new QVBoxLayout(content);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    root->addWidget(content, 1);

    setupToolbar(mainLayout);
    setupContentSplitter(mainLayout);
    updateToolbar();
}

void CreateMapWidget::setupHeaderBar(QVBoxLayout *root)
{
    auto *headerBar = new QWidget;
    headerBar->setFixedHeight(44);
    headerBar->setStyleSheet(CreateMapStyle::kHeaderBar);
    auto *hl = new QHBoxLayout(headerBar);
    hl->setContentsMargins(8, 4, 8, 4);
    hl->setSpacing(8);

    auto *backBtn = new QPushButton("← 返回");
    backBtn->setStyleSheet(CreateMapStyle::kBackBtn);
    backBtn->setFocusPolicy(Qt::NoFocus);
    connect(backBtn, &QPushButton::clicked, this, [this]() {
        if (!tryExitMapping()) return;
        emit goBack();
    });
    hl->addWidget(backBtn);

    auto *div = new QFrame;
    div->setFrameShape(QFrame::VLine);
    div->setStyleSheet(CreateMapStyle::kDivider);
    hl->addWidget(div);

    auto *titleLabel = new QLabel("创建地图");
    titleLabel->setStyleSheet(CreateMapStyle::kTitleLabel);
    hl->addWidget(titleLabel);
    hl->addStretch();

    root->addWidget(headerBar);
}

void CreateMapWidget::setupToolbar(QVBoxLayout *mainLayout)
{
    auto *toolbar = new QHBoxLayout;
    m_adaptiveBtn     = new QPushButton("自适应建图");
    m_highBtn         = new QPushButton("高阶建图");
    m_stopAdaptiveBtn = new QPushButton("停止自适应建图");
    m_resetAllBtn     = new QPushButton("全局重置反光柱");
    m_resetLocalBtn   = new QPushButton("局部重置反光柱");
    m_stopHighBtn     = new QPushButton("停止高阶建图");
    m_cruiseBtn       = new QPushButton("暂停巡航");

    for (auto btn : {m_adaptiveBtn, m_highBtn, m_stopAdaptiveBtn, m_resetAllBtn, m_resetLocalBtn, m_stopHighBtn, m_cruiseBtn}) {
        btn->setStyleSheet(CreateMapStyle::kActionBtn);
        btn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        btn->setFocusPolicy(Qt::NoFocus);
        toolbar->addWidget(btn);
    }
    toolbar->addStretch();

    connect(m_adaptiveBtn,     &QPushButton::clicked, this, &CreateMapWidget::onStartAdaptiveClick);
    connect(m_highBtn,         &QPushButton::clicked, this, &CreateMapWidget::onStartHighClick);
    connect(m_stopAdaptiveBtn, &QPushButton::clicked, this, &CreateMapWidget::onStopAdaptiveClick);
    connect(m_stopHighBtn,     &QPushButton::clicked, this, &CreateMapWidget::onStopHighClick);
    connect(m_resetAllBtn,     &QPushButton::clicked, this, &CreateMapWidget::onResetReflectorAll);
    connect(m_resetLocalBtn,   &QPushButton::clicked, this, &CreateMapWidget::onResetReflectorLocal);
    connect(m_cruiseBtn,       &QPushButton::clicked, this, &CreateMapWidget::onToggleCruiseClick);

    mainLayout->addLayout(toolbar);
}

void CreateMapWidget::setupContentSplitter(QVBoxLayout *mainLayout)
{
    auto *splitter = new QSplitter(Qt::Horizontal);

    m_mapCanvas = new CreateMapCanvasWidget;
    m_mapCanvas->setStyleSheet(CreateMapStyle::kCanvasBackground);

    m_directionPad = new DirectionPadWidget(m_mapCanvas);
    m_directionPad->move(16, qMax(16, m_mapCanvas->height() - m_directionPad->height() - 16));
    m_directionPad->raise();
    m_mapCanvas->installEventFilter(this);

    splitter->addWidget(m_mapCanvas);

    m_teleopPanel = new CreateMapTeleopPanel;
    m_teleopPanel->setMaximumWidth(280);
    splitter->addWidget(m_teleopPanel);

    splitter->setSizes({800, 280});
    mainLayout->addWidget(splitter);
}

void CreateMapWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    initMqtt();
    setFocus();
    if (!m_mappingActive && !m_highMappingActive) m_mapCanvas->setMapData(QJsonObject());
    if (m_directionPad) {
        m_directionPad->move(16, qMax(16, m_mapCanvas->height() - m_directionPad->height() - 16));
        m_directionPad->raise();
    }
}

bool CreateMapWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_mapCanvas && event->type() == QEvent::Resize && m_directionPad) {
        m_directionPad->move(16, qMax(16, m_mapCanvas->height() - m_directionPad->height() - 16));
        m_directionPad->raise();
    }
    return QWidget::eventFilter(watched, event);
}

void CreateMapWidget::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    disconnectMqtt();
}

void CreateMapWidget::onTeleopActiveChanged(bool active)
{
    if (m_teleopPanel) m_teleopPanel->setTeleopActive(active);
}

void CreateMapWidget::onTeleopBrakeChanged(bool brake)
{
    if (m_teleopPanel) m_teleopPanel->setBrakeActive(brake);
}

void CreateMapWidget::initMqtt()
{
    auto &mqtt = MqttService::instance();
    if (!mqtt.isConnected()) mqtt.connectToHost(AppConfig::instance().serverIp(), 1883);

    QString agvId = AppConfig::instance().agvId();
    if (agvId.isEmpty()) return;
    QString base = QString("/rob/%1/").arg(agvId);
    mqtt.subscribeTopic(base + "mapping",    0, [this](const QString &t, const QByteArray &m) { onMqttMessage(t, m); });
    mqtt.subscribeTopic(base + "carto_pose", 0, [this](const QString &t, const QByteArray &m) { onMqttMessage(t, m); });
    mqtt.subscribeTopic(base + "markers",    0, [this](const QString &t, const QByteArray &m) { onMqttMessage(t, m); });
}

void CreateMapWidget::disconnectMqtt()
{
    auto &mqtt = MqttService::instance();
    QString agvId = AppConfig::instance().agvId();
    if (mqtt.isConnected() && !agvId.isEmpty()) {
        mqtt.publish(QString("/rob/%1/navigationContinue").arg(agvId), "1");
        if (m_mappingActive) mqtt.publish(QString("/rob/%1/cancel").arg(agvId), "1");
        else if (m_highMappingActive) mqtt.publish(QString("/rob/%1/cancel").arg(agvId), "2");
    }
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
    } else if (topic.contains("/carto_pose")) {
        m_robotPose = data;
        m_mapCanvas->setRobotPose(data);
        if (m_teleopPanel) {
            m_teleopPanel->setLinearVelocity(data.value("linear_velocity").toDouble(0));
            m_teleopPanel->setAngularVelocity(data.value("angular_velocity").toDouble(0));
        }
    } else if (topic.contains("/markers")) {
        m_poles = doc.isArray() ? doc.array() : data.value("markers").toArray();
        m_mapCanvas->setPoles(m_poles);
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
    m_workflow->startAdaptiveMapping([this](bool ok) { if (ok) { m_mappingActive = true; updateToolbar(); } });
}

void CreateMapWidget::onStopAdaptiveClick()
{
    m_workflow->stopAdaptiveMapping([this](bool fin, bool) {
        if (fin) { m_mappingActive = false; m_highMappingActive = false; m_mapCanvas->setHighMappingActive(false); updateToolbar(); }
    });
}

void CreateMapWidget::onStartHighClick()
{
    m_workflow->startHighOrderMapping([this](bool ok) {
        if (ok) { m_highMappingActive = true; m_mapCanvas->setHighMappingActive(true); updateToolbar(); }
    });
}

void CreateMapWidget::onStopHighClick()
{
    m_workflow->stopHighOrderMapping([this](bool fin, bool) {
        if (fin) { m_highMappingActive = false; m_mappingActive = false; updateToolbar(); }
    });
}

void CreateMapWidget::onResetReflectorAll()   { m_workflow->resetReflectorAll(); }
void CreateMapWidget::onResetReflectorLocal() { m_workflow->resetReflectorLocal(); }

void CreateMapWidget::onToggleCruiseClick()
{
    m_workflow->toggleCruise(m_cruiseSuspended, [this](bool next) { m_cruiseSuspended = next; updateToolbar(); });
}

bool CreateMapWidget::tryExitMapping()
{
    return m_workflow->tryExitMapping(m_mappingActive, m_highMappingActive, [this]() {
        m_mappingActive = false; m_highMappingActive = false; m_mapCanvas->setHighMappingActive(false);
        updateToolbar();
        emit goBack();
    });
}
