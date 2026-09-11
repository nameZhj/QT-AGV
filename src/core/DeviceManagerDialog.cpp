#include "DeviceManagerDialog.h"
#include "widgets/NetworkConfigDialog.h"
#include "ApiService.h"
#include "AppConfig.h"
#include "TestPortMapping.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QApplication>
#include <QScreen>
#include <QDateTime>
#include <QJsonArray>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QIcon>
#include <QInputDialog>
#include <QTimer>
#include <QProgressDialog>
#include <QPointer>
#include "Toast.h"

// 从 QJsonObject 提取 linkStatus (可能是字符串 "0"/"1" 或数値 0/1)
// 参考 Vue index.vue: linkStatus==='0' 表示已链接, '1' 表示未链接
// bool true 对应 "0"(已链接), false 对应 "1"(未链接)
static QString extractLinkStatus(const QJsonObject &obj, const QString &key = "linkStatus")
{
    QJsonValue v = obj.value(key);
    if (v.isString()) return v.toString();
    if (v.isDouble()) return QString::number(v.toInt());
    if (v.isBool())   return v.toBool() ? "0" : "1";
    return QString();
}

// ═══════════════════════════════════════════════════════════════════════════════
//  DeviceCard
// ═══════════════════════════════════════════════════════════════════════════════

DeviceCard::DeviceCard(const DeviceProfile &profile, int index, QWidget *parent)
    : QWidget(parent), m_index(index), m_profile(profile)
{
    setFixedSize(280, 320);
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(
        "DeviceCard {"
        "  background: #252a35;"
        "  border: 1px solid #3a4050;"
        "  border-radius: 8px;"
        "}");

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header bar ──────────────────────────────────────────────────────────
    auto *hdr = new QWidget;
    hdr->setFixedHeight(38);
    hdr->setStyleSheet(
        "background: #1e2228;"
        "border-radius: 8px 8px 0 0;"
        "border-bottom: 1px solid #3a4050;");
    auto *hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(10, 0, 8, 0);
    hl->setSpacing(5);

    // Status dot
    m_onlineDot = new QLabel;
    m_onlineDot->setFixedSize(10, 10);
    m_onlineDot->setStyleSheet("background:#4a5568; border-radius:5px;");
    hl->addWidget(m_onlineDot);

    m_onlineLabel = new QLabel("检测中...");
    m_onlineLabel->setStyleSheet("color:#8898a8; font-size:11px; background:transparent;");
    hl->addWidget(m_onlineLabel);

    hl->addSpacing(6);

    auto *typeLbl = new QLabel("AGV");
    typeLbl->setStyleSheet("color:#c8d0d8; font-size:11px; font-weight:600; background:transparent;");
    hl->addWidget(typeLbl);

    hl->addStretch();

    auto mkDelBtn = []() {
        QPixmap pm(20, 20);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        QColor col("#e8e8e8");
        p.setPen(QPen(col, 1.5, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(5, 7, 10, 11), 1.5, 1.5);
        p.drawLine(QPointF(3.5, 7), QPointF(16.5, 7));
        p.drawLine(QPointF(7, 4.5), QPointF(13, 4.5));
        p.drawLine(QPointF(8, 4.5), QPointF(8, 7));
        p.drawLine(QPointF(12, 4.5), QPointF(12, 7));
        p.drawLine(QPointF(8.5, 9), QPointF(8.5, 16));
        p.drawLine(QPointF(11.5, 9), QPointF(11.5, 16));
        p.end();

        auto *b = new QPushButton;
        b->setFlat(true);
        b->setFixedSize(28, 28);
        b->setIcon(QIcon(pm));
        b->setIconSize(QSize(16, 16));
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(
            "QPushButton{background:#3a4450;border:1px solid #5a6070;border-radius:4px;}"
            "QPushButton:hover{background:#4a1414;border-color:#e05555;}"
            "QPushButton:pressed{background:#2a0a0a;}");
        return b;
    };

    auto *delBtn = mkDelBtn();
    connect(delBtn, &QPushButton::clicked, this, [this]{ emit removeClicked(m_index); });
    hl->addWidget(delBtn);

    root->addWidget(hdr);

    // ── Body ────────────────────────────────────────────────────────────────
    auto *body = new QWidget;
    body->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *bl = new QVBoxLayout(body);
    bl->setContentsMargins(12, 12, 12, 10);
    bl->setSpacing(8);

    // Top row: Robot icon + Device name + IP
    auto *topRow = new QHBoxLayout;
    topRow->setSpacing(10);

    auto *iconLbl = new QLabel("🤖");
    iconLbl->setFixedSize(46, 46);
    iconLbl->setAlignment(Qt::AlignCenter);
    iconLbl->setStyleSheet(
        "font-size:24px;"
        "background:#00304a;"
        "border-radius:23px;"
        "border:2px solid #00bcd4;");
    topRow->addWidget(iconLbl);

    auto *nameCol = new QVBoxLayout;
    nameCol->setSpacing(2);
    nameCol->setAlignment(Qt::AlignVCenter);

    m_nameLabel = new QLabel;
    m_nameLabel->setStyleSheet("color:#ffffff; font-size:14px; font-weight:700; background:transparent;");
    nameCol->addWidget(m_nameLabel);

    auto *ipLbl = new QLabel(profile.ip);
    ipLbl->setStyleSheet("color:#90a4ae; font-size:12px; font-family:Consolas,monospace; background:transparent;");
    nameCol->addWidget(ipLbl);

    topRow->addLayout(nameCol, 1);
    bl->addLayout(topRow);

    // Badges row: Car ID
    auto *tagRow = new QHBoxLayout;
    tagRow->setSpacing(6);
    auto *idBadge = new QLabel(QString("ID: %1").arg(profile.carId.isEmpty() ? "未注册" : profile.carId));
    idBadge->setStyleSheet(
        "color:#7ab0d8; font-size:11px;"
        "background:#1e2a3a; border:1px solid #2d4060; border-radius:3px; padding:2px 8px;");
    tagRow->addWidget(idBadge);
    tagRow->addStretch();
    bl->addLayout(tagRow);

    // Info details panel (Map + Version, cached locally)
    auto *infoBox = new QFrame;
    infoBox->setStyleSheet(
        "QFrame { background:#1a1e27; border:1px solid #2c3240; border-radius:6px; }");
    auto *grid = new QGridLayout(infoBox);
    grid->setContentsMargins(8, 6, 8, 6);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(5);

    auto *mapTitle = new QLabel("🗺 绑定地图:");
    mapTitle->setStyleSheet("color:#8898a8; font-size:11px; background:transparent;");
    m_mapLabel = new QLabel;
    m_mapLabel->setStyleSheet("color:#e2e8f0; font-size:11px; font-weight:600; background:transparent;");

    auto *verTitle = new QLabel("🏷 软件版本:");
    verTitle->setStyleSheet("color:#8898a8; font-size:11px; background:transparent;");
    m_verLabel = new QLabel;
    m_verLabel->setStyleSheet("color:#e2e8f0; font-size:11px; font-weight:600; background:transparent;");

    grid->addWidget(mapTitle, 0, 0);
    grid->addWidget(m_mapLabel, 0, 1);
    grid->addWidget(verTitle, 1, 0);
    grid->addWidget(m_verLabel, 1, 1);
    grid->setColumnStretch(1, 1);

    bl->addWidget(infoBox);
    bl->addStretch();

    root->addWidget(body, 1);

    // ── Footer ──────────────────────────────────────────────────────────────
    auto *ftr = new QWidget;
    ftr->setFixedHeight(48);
    ftr->setStyleSheet(
        "background:#1e2228;"
        "border-radius:0 0 8px 8px;"
        "border-top:1px solid #3a4050;");
    auto *fl = new QHBoxLayout(ftr);
    fl->setContentsMargins(10, 0, 10, 0);
    fl->setSpacing(8);
    m_footerLay = fl;

    auto *netBtn = new QPushButton("🌐 网络配置");
    netBtn->setCursor(Qt::PointingHandCursor);
    netBtn->setToolTip("远程 SSH 配置 AGV 网络参数 (IP/掩码/网关)");
    netBtn->setStyleSheet(
        "QPushButton{"
        "  background:#1e2a38; color:#00bcd4; border:1px solid #2d4060;"
        "  border-radius:4px; padding:6px 10px;"
        "  font-size:12px; font-weight:600;"
        "}"
        "QPushButton:hover{background:#26364a; color:#ffffff; border-color:#00bcd4;}"
        "QPushButton:pressed{background:#16202c;}");
    connect(netBtn, &QPushButton::clicked, this, [this]{ emit networkConfigClicked(m_index); });
    fl->addWidget(netBtn);

    fl->addStretch();

    if (profile.carId.isEmpty()) {
        auto *warn = new QLabel("⚠ 未同步");
        warn->setStyleSheet("color:#f0a060; font-size:11px; background:transparent;");
        fl->addWidget(warn);
    } else if (profile.linkStatus != "0") {
        m_warnLabel = new QLabel("⚠ 未链接");
        m_warnLabel->setStyleSheet("color:#f0a060; font-size:11px; background:transparent;");
        fl->addWidget(m_warnLabel);
    }

    if (!profile.carId.isEmpty() && profile.linkStatus != "0") {
        m_linkBtn = new QPushButton("链接设备");
        m_linkBtn->setCursor(Qt::PointingHandCursor);
        m_linkBtn->setStyleSheet(
            "QPushButton{"
            "  background:#6a4f1e; color:#ffd699; border:none;"
            "  border-radius:4px; padding:6px 12px;"
            "  font-size:12px; font-weight:700;"
            "}"
            "QPushButton:hover{background:#8a6f2e;}"
            "QPushButton:disabled{background:#2d3340; color:#556677;}");
        connect(m_linkBtn, &QPushButton::clicked, this, [this]{ emit linkClicked(m_index); });
        fl->addWidget(m_linkBtn);
    }

    m_connectBtn = new QPushButton("进入控制");
    m_connectBtn->setCursor(Qt::PointingHandCursor);
    m_connectBtn->setStyleSheet(
        "QPushButton{"
        "  background:#1e6fd4; color:#fff; border:none;"
        "  border-radius:4px; padding:6px 16px;"
        "  font-size:12px; font-weight:700;"
        "}"
        "QPushButton:hover{background:#2980e8;}"
        "QPushButton:disabled{background:#2d3340; color:#556677;}");
    connect(m_connectBtn, &QPushButton::clicked, this, [this]{ emit connectClicked(m_index); });
    m_connectBtn->setVisible(profile.linkStatus == "0");
    fl->addWidget(m_connectBtn);

    root->addWidget(ftr);

    // 初始化渲染已缓存的信息 (离线也可正常显示)
    updateCardDisplay();

    // 自动连接 MQTT 监听设备基本信息
    if (!profile.ip.isEmpty() && !profile.carId.isEmpty() && !AppConfig::instance().testMode()) {
        startMqttMonitor(profile.ip, profile.carId);
    } else if (AppConfig::instance().testMode()) {
        setOnlineUI(true);
        if (m_profile.mapName.isEmpty()) m_profile.mapName = "测试地图_01";
        if (m_profile.version.isEmpty()) m_profile.version = "v2.1.0-release";
        updateCardDisplay();
    } else {
        setOnlineUI(false);
    }
}

DeviceCard::~DeviceCard()
{
    if (m_heartbeatTimer) {
        m_heartbeatTimer->stop();
    }
    if (m_mqttClient) {
        m_mqttClient->disconnectFromHost();
    }
}

void DeviceCard::setBusy(bool busy)
{
    m_connectBtn->setEnabled(!busy);
    m_connectBtn->setText(busy ? "连接中..." : "进入控制");
}

void DeviceCard::setLinkStatus(bool linked)
{
    if (m_warnLabel)  m_warnLabel->setVisible(!linked);
    if (m_linkBtn)    m_linkBtn->setVisible(!linked);
    if (m_connectBtn) m_connectBtn->setVisible(linked);
}

void DeviceCard::setOnlineUI(bool online)
{
    if (online) {
        m_onlineDot->setStyleSheet("background:#4caf7a; border-radius:5px;");
        m_onlineLabel->setText("在线");
        m_onlineLabel->setStyleSheet("color:#4caf7a; font-size:11px; background:transparent;");
    } else {
        m_onlineDot->setStyleSheet("background:#4a5568; border-radius:5px;");
        m_onlineLabel->setText("离线");
        m_onlineLabel->setStyleSheet("color:#8898a8; font-size:11px; background:transparent;");
    }
}

void DeviceCard::updateCardDisplay()
{
    QString displayName = m_profile.name.trimmed();
    if (displayName.isEmpty()) {
        displayName = m_profile.carId.isEmpty() ? "未注册设备" : QString("AGV-%1").arg(m_profile.carId);
    }
    if (displayName.length() > 14) {
        m_nameLabel->setText(displayName.left(13) + "…");
        m_nameLabel->setToolTip(displayName);
    } else {
        m_nameLabel->setText(displayName);
        m_nameLabel->setToolTip(displayName);
    }

    QString mapName = m_profile.mapName.trimmed();
    if (mapName.isEmpty()) {
        m_mapLabel->setText("未绑定 / 离线缓存空");
        m_mapLabel->setStyleSheet("color:#718096; font-size:11px; background:transparent;");
    } else {
        if (mapName.length() > 13) {
            m_mapLabel->setText(mapName.left(12) + "…");
            m_mapLabel->setToolTip(mapName);
        } else {
            m_mapLabel->setText(mapName);
            m_mapLabel->setToolTip(mapName);
        }
        m_mapLabel->setStyleSheet("color:#48bb78; font-size:11px; font-weight:600; background:transparent;");
    }

    QString ver = m_profile.version.trimmed();
    if (ver.isEmpty()) {
        m_verLabel->setText("未知 / 离线缓存空");
        m_verLabel->setStyleSheet("color:#718096; font-size:11px; background:transparent;");
    } else {
        if (ver.length() > 13) {
            m_verLabel->setText(ver.left(12) + "…");
            m_verLabel->setToolTip(ver);
        } else {
            m_verLabel->setText(ver);
            m_verLabel->setToolTip(ver);
        }
        m_verLabel->setStyleSheet("color:#63b3ed; font-size:11px; font-weight:600; background:transparent;");
    }

    bool isCurrent = (AppConfig::instance().currentDeviceIndex() == m_index);
    if (m_connectBtn) {
        if (isCurrent) {
            m_connectBtn->setText("✓ 当前设备");
            m_connectBtn->setStyleSheet(
                "QPushButton{"
                "  background:#234e38; color:#68d391; border:1px solid #38a169;"
                "  border-radius:4px; padding:6px 14px;"
                "  font-size:12px; font-weight:700;"
                "}"
                "QPushButton:hover{background:#2f6848;}");
        } else {
            m_connectBtn->setText("进入控制");
            m_connectBtn->setStyleSheet(
                "QPushButton{"
                "  background:#1e6fd4; color:#fff; border:none;"
                "  border-radius:4px; padding:6px 14px;"
                "  font-size:12px; font-weight:700;"
                "}"
                "QPushButton:hover{background:#2980e8;}"
                "QPushButton:disabled{background:#2d3340; color:#556677;}");
        }
    }
}

void DeviceCard::startMqttMonitor(const QString &ip, const QString &carId)
{
    if (ip.isEmpty() || carId.isEmpty()) {
        setOnlineUI(false);
        return;
    }

    if (!m_heartbeatTimer) {
        m_heartbeatTimer = new QTimer(this);
        m_heartbeatTimer->setSingleShot(true);
        connect(m_heartbeatTimer, &QTimer::timeout, this, [this]() {
            setOnlineUI(false);
        });
    }

    m_onlineLabel->setText("连接中...");
    m_onlineLabel->setStyleSheet("color:#ecc94b; font-size:11px; background:transparent;");

    m_mqttClient = new QMqttClient(this);
    m_mqttClient->setHostname(ip);
    m_mqttClient->setPort(AppConfig::mapPort(1883));
    m_mqttClient->setUsername("admin");
    m_mqttClient->setPassword("vsislab");
    m_mqttClient->setClientId(QString("qt-agv-card-%1-%2").arg(carId).arg(QDateTime::currentMSecsSinceEpoch()));
    m_mqttClient->setKeepAlive(60);

    connect(m_mqttClient, &QMqttClient::connected, this, [this, carId]() {
        if (!m_mqttClient) return;
        m_mqttClient->subscribe(QString("/rob/%1/info").arg(carId), 0);
        setOnlineUI(true);
        if (m_heartbeatTimer) m_heartbeatTimer->start(12000);
    });

    connect(m_mqttClient, &QMqttClient::messageReceived, this,
        [this](const QByteArray &payload, const QMqttTopicName &) {
            setOnlineUI(true);
            if (m_heartbeatTimer) m_heartbeatTimer->start(12000);

            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
            if (!doc.isObject()) return;
            QJsonObject d = doc.object();

            // 提取设备名称
            QString devName;
            for (const char *k : {"carName", "car_name", "deviceName", "device_name", "name"}) {
                if (d.contains(k) && !d.value(k).toString().trimmed().isEmpty()) {
                    devName = d.value(k).toString().trimmed();
                    break;
                }
            }

            // 提取绑定的地图名称
            QString mapName;
            for (const char *k : {"mapName", "map_name", "currentMap", "current_map", "map_filename", "map"}) {
                if (d.contains(k) && !d.value(k).toString().trimmed().isEmpty()) {
                    mapName = d.value(k).toString().trimmed();
                    break;
                }
            }
            if (mapName.isEmpty()) {
                QJsonValue mv = d.contains("map_id") ? d.value("map_id") : d.value("mapId");
                if (!mv.isUndefined() && !mv.isNull()) {
                    QString mid = mv.toVariant().toString().trimmed();
                    if (!mid.isEmpty() && mid != "0") {
                        mapName = QString("地图 ID: %1").arg(mid);
                    }
                }
            }

            // 提取版本号
            QString version;
            for (const char *k : {"version", "soft_version", "software_version", "softVersion", "softwareVersion", "app_version", "appVersion", "system_version", "dsp_version"}) {
                if (d.contains(k) && !d.value(k).toString().trimmed().isEmpty()) {
                    version = d.value(k).toString().trimmed();
                    break;
                }
            }

            // 更新并持久化缓存
            bool changed = false;
            if (!devName.isEmpty() && devName != m_profile.name) {
                m_profile.name = devName;
                changed = true;
            }
            if (!mapName.isEmpty() && mapName != m_profile.mapName) {
                m_profile.mapName = mapName;
                changed = true;
            }
            if (!version.isEmpty() && version != m_profile.version) {
                m_profile.version = version;
                changed = true;
            }

            if (changed) {
                updateCardDisplay();
                AppConfig::instance().saveDevice(m_profile);
            }
        });

    connect(m_mqttClient, &QMqttClient::disconnected, this, [this]() {
        setOnlineUI(false);
        if (m_heartbeatTimer) m_heartbeatTimer->stop();
    });

    connect(m_mqttClient, &QMqttClient::errorChanged, this, [this](QMqttClient::ClientError error) {
        if (error != QMqttClient::NoError) {
            setOnlineUI(false);
            if (m_heartbeatTimer) m_heartbeatTimer->stop();
        }
    });

    m_mqttClient->connectToHost();
}

// ═══════════════════════════════════════════════════════════════════════════════
//  DeviceManagerDialog
// ═══════════════════════════════════════════════════════════════════════════════

static const char *kDlgStyle = R"(
QDialog { background:#1a1d23; color:#c8d0d8; font-family:"Microsoft YaHei UI","Segoe UI",sans-serif; }
QLineEdit {
    background:#22262e; border:1px solid #3a3f4a; border-radius:4px;
    color:#c8d0d8; padding:6px 10px; font-size:13px;
}
QLineEdit:focus { border-color:#00bcd4; }
QPushButton {
    border-radius:4px; padding:6px 14px; font-size:12px; font-weight:600;
    border:1px solid #3a3f4a; background:#252a35; color:#c8d0d8;
}
QPushButton:hover { background:#2d3340; border-color:#4a9eff; }
QPushButton:pressed { background:#1e4080; }
QPushButton:disabled { color:#445566; border-color:#2d3340; }
QScrollArea { border:none; background:transparent; }
QScrollBar:vertical { background:#1a1d23; width:6px; border-radius:3px; }
QScrollBar::handle:vertical { background:#3a3f4a; border-radius:3px; min-height:20px; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
)";

DeviceManagerDialog::DeviceManagerDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("移动机器人管理平台");
    setModal(true);
    setStyleSheet(kDlgStyle);

    if (parent) {
        resize(parent->size());
        move(parent->pos());
    } else {
        QRect sg = QApplication::primaryScreen()->availableGeometry();
        resize(sg.width() * 88 / 100, sg.height() * 88 / 100);
        move((sg.width() - width()) / 2, (sg.height() - height()) / 2);
    }

    setupUI();
    refreshCards();
}

void DeviceManagerDialog::setupUI()
{
    m_stack = new QStackedWidget(this);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(m_stack);

    // ── Main panel ──────────────────────────────────────────────────────────
    m_mainPanel = new QWidget;
    auto *mainLay = new QHBoxLayout(m_mainPanel);
    mainLay->setContentsMargins(0, 0, 0, 0);
    mainLay->setSpacing(0);

    // Content area
    auto *content = new QWidget;
    content->setStyleSheet("background:#1a1d23;");
    auto *cl = new QVBoxLayout(content);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(0);

    // Top bar
    auto *topBar = new QWidget;
    topBar->setFixedHeight(52);
    topBar->setStyleSheet("background:#12151a; border-bottom:1px solid #2d3340;");
    setupTopBar(topBar);
    cl->addWidget(topBar);

    // Cards scrollable area
    auto *cardsWidget = new QWidget;
    cardsWidget->setStyleSheet("background:#1a1d23;");
    setupCardArea(cardsWidget);
    cl->addWidget(cardsWidget, 1);

    // Status bar
    m_statusLabel = new QLabel;
    m_statusLabel->setFixedHeight(26);
    m_statusLabel->setStyleSheet(
        "background:#0d1117; color:#8898a8; font-size:11px; padding:0 16px;"
        "border-top:1px solid #2d3340;");
    cl->addWidget(m_statusLabel);

    mainLay->addWidget(content, 1);
    m_stack->addWidget(m_mainPanel);

    setupAddPanel();
    m_stack->setCurrentWidget(m_mainPanel);
}

void DeviceManagerDialog::setupTopBar(QWidget *topBar)
{
    auto *lay = new QHBoxLayout(topBar);
    lay->setContentsMargins(12, 0, 12, 0);
    lay->setSpacing(8);

    lay->addStretch();

    auto mkToolBtn = [&](const QString &text, bool primary = false) {
        auto *b = new QPushButton(text);
        b->setFixedHeight(32);
        b->setCursor(Qt::PointingHandCursor);
        if (primary) {
            b->setStyleSheet(
                "QPushButton{background:#00bcd4;color:#fff;border:none;"
                "border-radius:4px;padding:0 14px;font-size:12px;font-weight:700;}"
                "QPushButton:hover{background:#00acc1;}");
        } else {
            b->setStyleSheet(
                "QPushButton{background:transparent;color:#00bcd4;"
                "border:1px solid #00bcd4;border-radius:4px;"
                "padding:0 12px;font-size:12px;}"
                "QPushButton:hover{background:rgba(0,188,212,0.1);}");
        }
        lay->addWidget(b);
        return b;
    };

    auto *refreshAllBtn = mkToolBtn("⟳  刷新全部");
    auto *addDevBtn     = mkToolBtn("＋  添加设备", true);

    // 端口映射开关按钮
    auto *portMapBtn = mkToolBtn("🌐  端口映射");
    portMapBtn->setCheckable(true);
    portMapBtn->setChecked(TestPortMapping::instance().isEnabled());
    auto updatePortMapStyle = [portMapBtn](bool enabled) {
        if (enabled) {
            portMapBtn->setStyleSheet(
                "QPushButton{background:#9c27b0;color:#fff;border:none;"
                "border-radius:4px;padding:0 12px;font-size:12px;}"
                "QPushButton:hover{background:#7b1fa2;}");
            portMapBtn->setToolTip("测试环境外网端口映射: 已开启\n1883->6020, 8061->6017, 8062->6018, 8048->6015, 8001->6019");
        } else {
            portMapBtn->setStyleSheet(
                "QPushButton{background:transparent;color:#9c27b0;"
                "border:1px solid #9c27b0;border-radius:4px;"
                "padding:0 12px;font-size:12px;}"
                "QPushButton:hover{background:rgba(156,39,176,0.1);}");
            portMapBtn->setToolTip("测试环境外网端口映射: 已关闭 (直接使用服务原始内网端口)");
        }
    };
    updatePortMapStyle(TestPortMapping::instance().isEnabled());

    connect(portMapBtn, &QPushButton::toggled, this, [this, updatePortMapStyle](bool checked) {
        TestPortMapping::instance().setEnabled(checked);
        updatePortMapStyle(checked);
        if (checked) {
            setStatus("外网端口映射已开启 (1883->6020, 8061->6017, 8062->6018, 8048->6015, 8001->6019)");
        } else {
            setStatus("外网端口映射已关闭 (使用内网原始端口)");
        }
    });

    // 测试模式按钮
    auto *testBtn = mkToolBtn("🔧  测试模式");
    testBtn->setCheckable(true);
    testBtn->setChecked(AppConfig::instance().testMode());
    if (AppConfig::instance().testMode())
        testBtn->setStyleSheet(
            "QPushButton{background:#f0a060;color:#fff;border:none;"
            "border-radius:4px;padding:0 12px;font-size:12px;}"
            "QPushButton:hover{background:#e09050;}");
    connect(testBtn, &QPushButton::toggled, this, [this, testBtn](bool checked) {
        AppConfig::instance().setTestMode(checked);
        if (checked) {
            AppConfig::instance().ensureTestDevice();
            testBtn->setStyleSheet(
                "QPushButton{background:#f0a060;color:#fff;border:none;"
                "border-radius:4px;padding:0 12px;font-size:12px;}"
                "QPushButton:hover{background:#e09050;}");
            setStatus("测试模式已开启 — 模拟设备已添加");
        } else {
            testBtn->setStyleSheet(
                "QPushButton{background:transparent;color:#00bcd4;"
                "border:1px solid #00bcd4;border-radius:4px;"
                "padding:0 12px;font-size:12px;}"
                "QPushButton:hover{background:rgba(0,188,212,0.1);}");
            setStatus("测试模式已关闭");
        }
        refreshCards();
    });

    connect(refreshAllBtn, &QPushButton::clicked, this, &DeviceManagerDialog::onRefreshAll);
    connect(addDevBtn,     &QPushButton::clicked, this, &DeviceManagerDialog::onAddDevice);
}

void DeviceManagerDialog::setupCardArea(QWidget *area)
{
    auto *lay = new QVBoxLayout(area);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    // Scroll area
    m_scrollArea = new QScrollArea;
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setStyleSheet("QScrollArea{background:#1a1d23;}");

    m_cardsContainer = new QWidget;
    m_cardsContainer->setStyleSheet("background:#1a1d23;");
    m_cardsGrid = new QGridLayout(m_cardsContainer);
    m_cardsGrid->setContentsMargins(20, 20, 20, 20);
    m_cardsGrid->setSpacing(16);
    m_cardsGrid->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_scrollArea->setWidget(m_cardsContainer);
    lay->addWidget(m_scrollArea, 1);
}

void DeviceManagerDialog::setupAddPanel()
{
    m_addPanel = new QWidget;
    m_addPanel->setStyleSheet("background:#1a1d23;");
    auto *al = new QVBoxLayout(m_addPanel);
    al->setContentsMargins(0, 0, 0, 0);
    al->setSpacing(0);

    // Panel header
    auto *phdr = new QWidget;
    phdr->setFixedHeight(52);
    phdr->setStyleSheet("background:#12151a; border-bottom:1px solid #2d3340;");
    auto *phl = new QHBoxLayout(phdr);
    phl->setContentsMargins(16, 0, 16, 0);

    m_cancelBtn = new QPushButton("\u2190 \u8fd4\u56de");
    m_cancelBtn->setFixedHeight(32);
    m_cancelBtn->setStyleSheet(
        "QPushButton{background:transparent;color:#8898a8;border:1px solid #3a3f4a;"
        "border-radius:4px;padding:0 12px;font-size:12px;}"
        "QPushButton:hover{color:#c8d0d8;border-color:#8898a8;}");
    connect(m_cancelBtn, &QPushButton::clicked, this, &DeviceManagerDialog::onCancelAdd);
    phl->addWidget(m_cancelBtn);
    phl->addStretch();

    auto *panelTitle = new QLabel("\u6dfb\u52a0\u8bbe\u5907");
    panelTitle->setStyleSheet("color:#c8d0d8; font-size:15px; font-weight:700;");
    phl->addWidget(panelTitle);
    phl->addStretch();
    phl->addSpacing(80); // balance with back button width

    al->addWidget(phdr);

    // Form area (centered)
    auto *center = new QWidget;
    auto *cl = new QVBoxLayout(center);
    cl->setContentsMargins(0, 60, 0, 0);
    cl->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    cl->setSpacing(20);

    auto *formBox = new QWidget;
    formBox->setFixedWidth(500);
    formBox->setStyleSheet(
        "background:#1e2228; border:1px solid #2d3340; border-radius:8px;");
    auto *fbl = new QVBoxLayout(formBox);
    fbl->setContentsMargins(32, 28, 32, 28);
    fbl->setSpacing(16);

    auto *fTitle = new QLabel("\u8f93\u5165\u8bbe\u5907IP");
    fTitle->setStyleSheet("color:#c8d0d8; font-size:16px; font-weight:700;");
    fbl->addWidget(fTitle);

    fbl->addSpacing(4);

    m_ipEdit = new QLineEdit;
    m_ipEdit->setPlaceholderText("\u4f8b\u5982: 192.168.1.100");
    m_ipEdit->setFixedHeight(42);
    fbl->addWidget(m_ipEdit);

    // ── 设备名称行（无 carId 时才显示） ──────────────────────────────────────
    m_nameRow = new QWidget;
    auto *nrl = new QVBoxLayout(m_nameRow);
    nrl->setContentsMargins(0, 0, 0, 0);
    nrl->setSpacing(6);

    auto *nameLbl = new QLabel("\u8bbe\u5907\u540d\u79f0");
    nameLbl->setStyleSheet("color:#8898a8; font-size:12px;");
    nrl->addWidget(nameLbl);

    m_nameEdit = new QLineEdit;
    m_nameEdit->setPlaceholderText("\u8bf7\u8f93\u5165\u8bbe\u5907\u540d\u79f0");
    m_nameEdit->setFixedHeight(42);
    nrl->addWidget(m_nameEdit);

    m_nameRow->hide(); // 默认隐藏，无 carId 时才展开
    fbl->addWidget(m_nameRow);
    // ────────────────────────────────────────────────────────────────────────

    m_addStatus = new QLabel;
    m_addStatus->setStyleSheet("color:#f0a060; font-size:12px;");
    m_addStatus->setWordWrap(true);
    m_addStatus->setMinimumHeight(18);
    fbl->addWidget(m_addStatus);

    m_confirmBtn = new QPushButton("\u8fde\u63a5\u5e76\u6dfb\u52a0");
    m_confirmBtn->setFixedHeight(42);
    m_confirmBtn->setStyleSheet(
        "QPushButton{background:#00bcd4;color:#fff;border:none;"
        "border-radius:4px;font-size:14px;font-weight:700;}"
        "QPushButton:hover{background:#00acc1;}"
        "QPushButton:disabled{background:#2d3340;color:#556677;}");
    fbl->addWidget(m_confirmBtn);

    connect(m_confirmBtn, &QPushButton::clicked, this, &DeviceManagerDialog::onConfirmAdd);
    connect(m_ipEdit, &QLineEdit::returnPressed, m_confirmBtn, &QPushButton::click);

    cl->addWidget(formBox, 0, Qt::AlignHCenter);
    al->addWidget(center, 1);

    m_stack->addWidget(m_addPanel);
}

void DeviceManagerDialog::refreshCards()
{
    // Remove old cards and any other widgets from grid
    for (DeviceCard *c : m_cards) {
        m_cardsGrid->removeWidget(c);
        c->deleteLater();
    }
    m_cards.clear();

    // Also remove any leftover non-card widgets (e.g. empty label)
    while (m_cardsGrid->count() > 0) {
        QLayoutItem *item = m_cardsGrid->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    const QList<DeviceProfile> list = AppConfig::instance().deviceList();

    if (list.isEmpty()) {
        auto *emptyLbl = new QLabel("暂无设备，点击「＋ 添加设备」添加");
        emptyLbl->setAlignment(Qt::AlignCenter);
        emptyLbl->setStyleSheet("color:#556677; font-size:14px;");
        m_cardsGrid->addWidget(emptyLbl, 0, 0, 1, 3);
        return;
    }

    const int cols = 3;
    for (int i = 0; i < list.size(); ++i) {
        auto *card = new DeviceCard(list[i], i, m_cardsContainer);
        connect(card, &DeviceCard::connectClicked,       this, &DeviceManagerDialog::onConnectCard);
        connect(card, &DeviceCard::removeClicked,        this, &DeviceManagerDialog::onRemoveCard);
        connect(card, &DeviceCard::linkClicked,          this, &DeviceManagerDialog::onLinkCard);
        connect(card, &DeviceCard::networkConfigClicked, this, &DeviceManagerDialog::onNetworkConfig);
        m_cardsGrid->addWidget(card, i / cols, i % cols);
        m_cards.append(card);

        // 异步从服务器同步最新 linkStatus，防止本地缓存值陈旧导致状态显示错误
        // 测试模式跳过服务器查询
        if (!list[i].carId.isEmpty() && !AppConfig::instance().testMode()) {
            const QString carId = list[i].carId;
            const QString ip    = list[i].ip;
            QPointer<DeviceCard> cardPtr(card);
            ApiService::instance().getCar(carId,
                [this, cardPtr, carId, ip, i](int code, const QJsonObject &res) {
                    if (!cardPtr) return;
                    if (code != 200) return;
                    QJsonObject data = res.value("data").toObject();
                    QString serverStatus = extractLinkStatus(data);
                    if (serverStatus.isEmpty()) return;
                    bool linked = (serverStatus == "0");  // Vue: '0'=已链接
                    QList<DeviceProfile> devList = AppConfig::instance().deviceList();
                    if (i < devList.size() && devList[i].ip == ip) {
                        if (devList[i].linkStatus != serverStatus) {
                            devList[i].linkStatus = serverStatus;
                            AppConfig::instance().saveDevice(devList[i]);
                        }
                    }
                    cardPtr->setLinkStatus(linked);
                });
        }
    }
}

void DeviceManagerDialog::onRefreshAll()
{
    refreshCards();
    setStatus("已刷新设备列表", "#5cb85c");
}

void DeviceManagerDialog::onAddDevice()
{
    m_ipEdit->clear();
    m_addStatus->clear();
    m_confirmBtn->setEnabled(true);
    m_confirmBtn->setText("连接并添加");
    m_stack->setCurrentWidget(m_addPanel);
    m_ipEdit->setFocus();
}

void DeviceManagerDialog::onCancelAdd()
{
    m_stack->setCurrentWidget(m_mainPanel);
}


void DeviceManagerDialog::onConfirmAdd()
{
    QString ip = m_ipEdit->text().trimmed();
    if (ip.startsWith("http://"))  ip = ip.mid(7);
    if (ip.startsWith("https://")) ip = ip.mid(8);
    m_ipEdit->setText(ip);

    if (ip.isEmpty()) {
        m_addStatus->setStyleSheet("color:#e05555; font-size:12px;");
        m_addStatus->setText("请输入服务器 IP 地址");
        return;
    }

    m_confirmBtn->setEnabled(false);
    m_addStatus->setStyleSheet("color:#f0a060; font-size:12px;");
    m_addStatus->setText("正在连接服务器...");

    AppConfig::instance().setServerIp(ip);

    // ── 第一步: 登录 ─────────────────────────────────────────────────────────
    ApiService::instance().login("admin", "admin123",
        [this, ip](int code, const QJsonObject &data) {
            if (code != 200 || data.value("token").toString().isEmpty()) {
                m_addStatus->setStyleSheet("color:#e05555; font-size:12px;");
                m_addStatus->setText("连接失败: " +
                    data.value("msg").toString("服务器无响应，请检查 IP 地址"));
                m_confirmBtn->setEnabled(true);
                return;
            }
            AppConfig::instance().setToken(data.value("token").toString());
            m_addStatus->setText("登录成功，正在获取设备信息...");

            // ── 第二步: 查询车辆列表 ─────────────────────────────────────────
            ApiService::instance().listCarAllNoLogin(
                [this, ip](int code2, const QJsonObject &carData) {
                    QString carId, carName;
                    if (code2 == 200) {
                        QJsonArray rows = carData.value("rows").toArray();
                        if (!rows.isEmpty()) {
                            QJsonObject car = rows.last().toObject();
                            QJsonValue v = car.value("carId");
                            carId   = v.isString() ? v.toString() : QString::number(v.toInt());
                            carName = car.value("carName").toString();
                            if (carName.isEmpty()) carName = QString("AGV @ %1").arg(ip);
                        }
                    }

                    if (!carId.isEmpty()) {
                        // ── 场景(1): 列表不为空且 carId 不为空 ──────────────
                        // 检查当前链接状态 (getCar 获取实时 linkStatus)
                        m_addStatus->setText("正在获取设备链接状态...");
                        ApiService::instance().getCar(carId,
                            [this, ip, carId, carName](int code3, const QJsonObject &carRes) {
                                QString linkStatus;
                                if (code3 == 200) {
                                    QJsonObject d = carRes.value("data").toObject();
                                    linkStatus = extractLinkStatus(d);
                                }

                                if (linkStatus == "0") {
                                    // 已链接 (Vue: linkStatus==='0') → 保存已链接状态，完成
                                    DeviceProfile dev;
                                    dev.ip            = ip;
                                    dev.carId         = carId;
                                    dev.token         = AppConfig::instance().token();
                                    dev.name          = carName;
                                    dev.lastConnected = QDateTime::currentDateTime();
                                    dev.linkStatus    = "0";  // 已链接
                                    AppConfig::instance().saveDevice(dev);
                                    AppConfig::instance().setAgvId(carId);

                                    m_stack->setCurrentWidget(m_mainPanel);
                                    refreshCards();
                                    setStatus(QString("设备 [%1] 已添加 (已链接)").arg(dev.name), "#5cb85c");
                                    Toast::success(this, QString("设备 [%1] 已添加").arg(dev.name));
                                } else {
                                    // 未链接 → 发起链接请求，无论成功与否均以未链接状态保存
                                    m_addStatus->setText(QString("设备 [%1] 未链接，正在申请链接...").arg(carName));

                                    QJsonObject linkForm;
                                    linkForm["carId"]  = carId;
                                    linkForm["carName"] = carName;
                                    linkForm["mapId"]   = QJsonValue();
                                    linkForm["deptId"]  = 100;
                                    linkForm["ipPort"]  = ip + ":8061";

                                    // 无论链接请求是否成功，都保存设备（未链接状态）
                                    auto saveAsUnlinked = [this, ip, carId, carName]() {
                                        DeviceProfile dev;
                                        dev.ip            = ip;
                                        dev.carId         = carId;
                                        dev.token         = AppConfig::instance().token();
                                        dev.name          = carName;
                                        dev.lastConnected = QDateTime::currentDateTime();
                                        dev.linkStatus    = "1";  // 未链接
                                        AppConfig::instance().saveDevice(dev);
                                        AppConfig::instance().setAgvId(carId);

                                        m_stack->setCurrentWidget(m_mainPanel);
                                        refreshCards();
                                        setStatus(QString("设备 [%1] 已添加 (未链接，请点击\"链接设备\")").arg(dev.name), "#f0a060");
                                        Toast::info(this, QString("设备 [%1] 已添加，链接申请已发送").arg(dev.name));
                                    };

                                    ApiService::instance().linkCarWithoutKey(linkForm,
                                        [saveAsUnlinked](int /*code4*/, const QJsonObject &) {
                                            // 无论返回什么都保存设备，linkStatus 后续由 refreshCards 同步
                                            saveAsUnlinked();
                                        });
                                }
                            });
                    } else {
                        // ── 场景(2): 列表为空，弹窗让用户输入设备名称后再创建 ──
                        m_confirmBtn->setEnabled(true);  // 暂时恢复按钮，等待用户操作

                        bool ok = false;
                        QString newCarName = QInputDialog::getText(
                            this,
                            "创建新设备",
                            QString("服务器 [%1] 未注册任何设备，请输入设备名称：").arg(ip),
                            QLineEdit::Normal,
                            QString("AGV @ %1").arg(ip),
                            &ok);
                        newCarName = newCarName.trimmed();

                        if (!ok || newCarName.isEmpty()) {
                            // 用户取消或未填写，中止流程
                            m_addStatus->setStyleSheet("color:#8898a8; font-size:12px;");
                            m_addStatus->setText("已取消，请重新输入 IP 或填写设备名称。");
                            return;
                        }

                        // 用户确认后，锁定按钮并开始创建
                        m_confirmBtn->setEnabled(false);
                        m_addStatus->setStyleSheet("color:#f0a060; font-size:12px;");
                        m_addStatus->setText(QString("正在创建设备 [%1]...").arg(newCarName));

                        QJsonObject carForm;
                        carForm["deptId"]    = 100;
                        carForm["mapId"]     = QJsonValue();
                        carForm["carName"]   = newCarName;
                        carForm["ipPort"]    = ip + ":8061";
                        carForm["robotType"] = "1";
                        carForm["machinery"] = "0";

                        ApiService::instance().addCarWithoutKey(carForm,
                            [this, ip, newCarName](int code2, const QJsonObject &res) {
                                if (code2 != 200) {
                                    m_addStatus->setStyleSheet("color:#e05555; font-size:12px;");
                                    m_addStatus->setText("创建设备失败: " +
                                        res.value("msg").toString("请检查服务器连接"));
                                    m_confirmBtn->setEnabled(true);
                                    return;
                                }

                                // 获取服务器返回的 carId
                                QJsonValue v = res.value("data").toObject().value("carId");
                                QString newCarId = v.isString() ? v.toString()
                                                                : QString::number(v.toInt());
                                m_addStatus->setText("设备已创建，正在申请链接...");

                                QJsonObject linkForm;
                                linkForm["carId"]   = newCarId;
                                linkForm["carName"]  = newCarName;
                                linkForm["mapId"]    = QJsonValue();
                                linkForm["deptId"]   = 100;
                                linkForm["ipPort"]   = ip + ":8061";

                                // 无论链接请求是否成功，都保存设备（未链接状态）
                                auto saveAsUnlinked = [this, ip, newCarId, newCarName]() {
                                    DeviceProfile dev;
                                    dev.ip            = ip;
                                    dev.carId         = newCarId;
                                    dev.token         = AppConfig::instance().token();
                                    dev.name          = newCarName;
                                    dev.lastConnected = QDateTime::currentDateTime();
                                    dev.linkStatus    = "1";  // 未链接
                                    AppConfig::instance().saveDevice(dev);
                                    AppConfig::instance().setAgvId(newCarId);

                                    m_stack->setCurrentWidget(m_mainPanel);
                                    refreshCards();
                                    setStatus(QString("设备 [%1] 已创建 (未链接，请点击\"链接设备\")").arg(dev.name), "#f0a060");
                                    Toast::info(this, QString("设备 [%1] 已创建，链接申请已发送").arg(dev.name));
                                };

                                ApiService::instance().linkCarWithoutKey(linkForm,
                                    [saveAsUnlinked](int /*code3*/, const QJsonObject &) {
                                        saveAsUnlinked();
                                    });
                            });
                    }
                });
        });
}
// 轮询 getCar 检查 linkStatus: '0'=已链接, '1'=未链接
// 参考 Vue index.vue: linkStatus==='0' 表示已链接 (链接按钮禁用)
// linkCarWithoutKey 返回 200 仅表示请求已受理, 设备实际链接需要时间, 故轮询校验
// 轮询期间显示阻塞式进度弹窗, 用户可取消
void DeviceManagerDialog::checkLinkStatus(const QString &carId, const QString &ip,
                                           const QString &carName, int attempt)
{
    // 最多尝试 10 次, 间隔 2 秒, 总计约 20 秒
    static const int kMaxAttempts = 10;
    static const int kIntervalMs = 2000;

    // 第一次调用时创建进度弹窗
    if (attempt == 0) {
        m_linkCancelled = false;
        if (m_linkProgress) {
            m_linkProgress->cancel();
            delete m_linkProgress;
        }
        m_linkProgress = new QProgressDialog(
            QString("正在校验设备 [%1] 的链接状态...\n剩余约 %2 秒").arg(carName).arg(kMaxAttempts * kIntervalMs / 1000),
            "取消", 0, kMaxAttempts, this);
        m_linkProgress->setWindowTitle("链接中");
        m_linkProgress->setWindowModality(Qt::WindowModal);
        m_linkProgress->setMinimumDuration(0);
        m_linkProgress->setValue(0);
        m_linkProgress->setCancelButton(nullptr);  // 用 canceled 信号控制, 不自动关闭
        // 点击右上角 X 或 ESC 取消
        connect(m_linkProgress, &QProgressDialog::canceled, this, [this]() {
            m_linkCancelled = true;
            if (m_linkProgress) {
                m_linkProgress->hide();
                m_linkProgress->deleteLater();
                m_linkProgress = nullptr;
            }
            // 标记为未链接并保存, 允许用户稍后重试
            // (carId/ip/carName 通过最后调用方捕获, 这里只设标志)
        });
    }

    // 用户已取消 → 保存为未链接状态, 结束轮询
    if (m_linkCancelled) {
        DeviceProfile dev;
        dev.ip            = ip;
        dev.carId         = carId;
        dev.token         = AppConfig::instance().token();
        dev.name          = carName;
        dev.lastConnected = QDateTime::currentDateTime();
        dev.linkStatus    = "1";  // '1'=未链接 (参考 Vue: linkStatus==='0'才是已链接)
        AppConfig::instance().saveDevice(dev);

        m_stack->setCurrentWidget(m_mainPanel);
        refreshCards();
        setStatus(QString("设备 [%1] 链接已取消").arg(carName), "#f0a060");
        return;
    }

    // 超时 → 保存为未链接
    if (attempt >= kMaxAttempts) {
        if (m_linkProgress) {
            m_linkProgress->hide();
            m_linkProgress->deleteLater();
            m_linkProgress = nullptr;
        }
        DeviceProfile dev;
        dev.ip            = ip;
        dev.carId         = carId;
        dev.token         = AppConfig::instance().token();
        dev.name          = carName;
        dev.lastConnected = QDateTime::currentDateTime();
        dev.linkStatus    = "1";  // '1'=未链接
        AppConfig::instance().saveDevice(dev);

        m_stack->setCurrentWidget(m_mainPanel);
        refreshCards();
        setStatus(QString("设备 [%1] 已保存，但未链接").arg(dev.name), "#f0a060");
        Toast::error(this, QString("设备 [%1] 链接超时，请在列表中点击\"链接设备\"重试").arg(dev.name));
        return;
    }

    // 更新进度弹窗
    if (m_linkProgress) {
        int remaining = kMaxAttempts - attempt;
        m_linkProgress->setLabelText(
            QString("正在校验设备 [%1] 的链接状态...\n剩余约 %2 秒").arg(carName).arg(remaining * kIntervalMs / 1000));
        m_linkProgress->setValue(attempt);
    }

    ApiService::instance().getCar(carId,
        [this, carId, ip, carName, attempt](int code, const QJsonObject &res) {
            if (m_linkCancelled) return;  // 用户已取消, 不再处理

            if (code == 200) {
                QJsonObject data = res.value("data").toObject();
                QString linkStatus = extractLinkStatus(data);
                if (linkStatus == "0") {
                    // 链接成功 (参考 Vue: linkStatus==='0' 表示已链接) → 关闭弹窗, 保存设备
                    if (m_linkProgress) {
                        m_linkProgress->hide();
                        m_linkProgress->deleteLater();
                        m_linkProgress = nullptr;
                    }
                    DeviceProfile dev;
                    dev.ip            = ip;
                    dev.carId         = carId;
                    dev.token         = AppConfig::instance().token();
                    dev.name          = carName;
                    dev.lastConnected = QDateTime::currentDateTime();
                    dev.linkStatus    = "0";  // '0'=已链接
                    AppConfig::instance().saveDevice(dev);
                    AppConfig::instance().setAgvId(carId);

                    m_stack->setCurrentWidget(m_mainPanel);
                    refreshCards();
                    setStatus(QString("设备 [%1] 已创建并链接").arg(dev.name), "#5cb85c");
                    Toast::success(this, QString("设备 [%1] 已创建并链接").arg(dev.name));
                    return;
                }
            }
            // 未链接或请求失败, 间隔后重试
            QTimer::singleShot(kIntervalMs, this,
                [this, carId, ip, carName, attempt]() {
                    checkLinkStatus(carId, ip, carName, attempt + 1);
                });
        });
}

void DeviceManagerDialog::onConnectCard(int index)
{
    connectDevice(index);
}

void DeviceManagerDialog::onRemoveCard(int index)
{
    AppConfig::instance().removeDevice(index);
    refreshCards();
    setStatus("设备已移除");
    Toast::success(this, "设备已移除");
}

// 重新链接未链接的设备: 只执行 linkCarWithoutKey (已有 carId, 不再执行 addCarWithoutKey)
void DeviceManagerDialog::onLinkCard(int index)
{
    const QList<DeviceProfile> list = AppConfig::instance().deviceList();
    if (index < 0 || index >= list.size()) return;

    const DeviceProfile &dev = list[index];
    if (dev.carId.isEmpty()) return;

    // 标记卡片为忙碌状态
    if (index < m_cards.size())
        m_cards[index]->setBusy(true);

    setStatus(QString("正在链接设备 [%1] ...").arg(dev.name));

    QJsonObject linkForm;
    linkForm["carId"]   = dev.carId;
    linkForm["carName"]  = dev.name;
    linkForm["mapId"]    = QJsonValue();
    linkForm["deptId"]   = 100;
    linkForm["ipPort"]   = dev.ip + ":8061";  // 参考 Vue details.vue handleLink

    // 登录获取最新 token
    ApiService::instance().login("admin", "admin123",
        [this, dev, linkForm](int code, const QJsonObject &data) {
            if (code == 200 && !data.value("token").toString().isEmpty()) {
                AppConfig::instance().setToken(data.value("token").toString());
            }
            ApiService::instance().linkCarWithoutKey(linkForm,
                [this, dev](int code3, const QJsonObject &linkRes) {
                    if (code3 != 200) {
                        setStatus(QString("链接设备 [%1] 失败: %2").arg(dev.name,
                            linkRes.value("msg").toString("服务器无响应")), "#e05555");
                        // 刷新卡片以恢复按钮状态
                        refreshCards();
                        return;
                    }
                    setStatus(QString("已申请链接 [%1]，等待设备响应...").arg(dev.name));
                    checkLinkStatus(dev.carId, dev.ip, dev.name, 0);
                });
        });
}

void DeviceManagerDialog::connectDevice(int index)
{
    const QList<DeviceProfile> list = AppConfig::instance().deviceList();
    if (index < 0 || index >= list.size()) return;

    if (index < m_cards.size())
        m_cards[index]->setBusy(true);

    AppConfig::instance().setCurrentDeviceIndex(index);
    AppConfig::instance().setServerIp(list[index].ip);
    AppConfig::instance().setAgvId(list[index].carId);

    // 测试模式: 跳过登录直接进入主界面
    if (AppConfig::instance().testMode()) {
        AppConfig::instance().setToken("test-token");
        QList<DeviceProfile> list2 = AppConfig::instance().deviceList();
        if (index < list2.size()) {
            list2[index].token         = "test-token";
            list2[index].lastConnected = QDateTime::currentDateTime();
            AppConfig::instance().saveDevice(list2[index]);
        }
        accept();
        return;
    }

    setStatus(QString("正在连接 %1 ...").arg(list[index].ip));

    // 使用 QPointer 保护异步回调，防止 dialog 在请求返回前被销毁
    QPointer<DeviceManagerDialog> guard(this);
    ApiService::instance().login("admin", "admin123",
        [this, index, guard](int code, const QJsonObject &data) {
            if (!guard) return;  // dialog 已销毁
            if (code == 200 && !data.value("token").toString().isEmpty()) {
                QString token = data.value("token").toString();
                AppConfig::instance().setToken(token);

                QList<DeviceProfile> list2 = AppConfig::instance().deviceList();
                if (index < list2.size()) {
                    list2[index].token         = token;
                    list2[index].lastConnected = QDateTime::currentDateTime();
                    AppConfig::instance().saveDevice(list2[index]);
                }
                accept();
            } else {
                if (index < m_cards.size())
                    m_cards[index]->setBusy(false);
                setStatus("连接失败，请检查网络或服务器地址", "#e05555");
            }
        });
}

void DeviceManagerDialog::onNetworkConfig(int index)
{
    const QList<DeviceProfile> list = AppConfig::instance().deviceList();
    if (index < 0 || index >= list.size()) return;
    DeviceProfile profile = list[index];

    NetworkConfigDialog dlg(profile.ip, profile.name.isEmpty() ? profile.carId : profile.name, this);
    if (dlg.exec() == QDialog::Accepted && dlg.isIpChanged()) {
        QString oldIp = profile.ip;
        QString newIp = dlg.newIp().trimmed();
        if (!newIp.isEmpty() && newIp != oldIp) {
            profile.ip = newIp;
            AppConfig::instance().updateDevice(index, profile);
            setStatus(QString("已更新设备 [%1] 的 IP 地址为 %2").arg(profile.name.isEmpty() ? profile.carId : profile.name, newIp), "#4caf7a");
            refreshCards();
        }
    }
}

void DeviceManagerDialog::setStatus(const QString &msg, const QString &color)
{
    if (!m_statusLabel) return;
    m_statusLabel->setText(msg);
    m_statusLabel->setStyleSheet(
        QString("background:#0d1117; color:%1; font-size:11px;"
                "padding:0 16px; border-top:1px solid #2d3340;").arg(color));
}
