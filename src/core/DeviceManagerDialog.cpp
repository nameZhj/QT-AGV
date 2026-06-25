#include "DeviceManagerDialog.h"
#include "ApiService.h"
#include "AppConfig.h"
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
#include <QInputDialog>
#include <QTimer>
#include "Toast.h"

// ═══════════════════════════════════════════════════════════════════════════════
//  DeviceCard
// ═══════════════════════════════════════════════════════════════════════════════

DeviceCard::DeviceCard(const DeviceProfile &profile, int index, QWidget *parent)
    : QWidget(parent), m_index(index)
{
    setFixedSize(244, 282);
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
    hdr->setFixedHeight(40);
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

    auto mkIconBtn = [](const QString &text, const QString &hoverColor) {
        auto *b = new QPushButton(text);
        b->setFlat(true);
        b->setFixedSize(26, 26);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(QString(
            "QPushButton{color:#8898a8;background:transparent;border:none;font-size:14px;}"
            "QPushButton:hover{color:%1;}").arg(hoverColor));
        return b;
    };

    auto *delBtn = mkIconBtn("✕", "#e05555");
    connect(delBtn, &QPushButton::clicked, this, [this]{ emit removeClicked(m_index); });
    hl->addWidget(delBtn);

    root->addWidget(hdr);

    // ── Body ────────────────────────────────────────────────────────────────
    auto *body = new QWidget;
    body->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *bl = new QVBoxLayout(body);
    bl->setContentsMargins(12, 18, 12, 10);
    bl->setSpacing(10);
    bl->setAlignment(Qt::AlignCenter);

    // Robot icon (colored circle + emoji)
    auto *iconLbl = new QLabel("🤖");
    iconLbl->setFixedSize(72, 72);
    iconLbl->setAlignment(Qt::AlignCenter);
    iconLbl->setStyleSheet(
        "font-size:38px;"
        "background:#00304a;"
        "border-radius:36px;"
        "border:2px solid #00bcd4;");
    bl->addWidget(iconLbl, 0, Qt::AlignCenter);

    // IP address
    auto *ipLbl = new QLabel(profile.ip);
    ipLbl->setAlignment(Qt::AlignCenter);
    ipLbl->setStyleSheet(
        "color:#e0e8f0; font-size:13px; font-weight:700;"
        "font-family:Consolas,monospace; background:transparent;");
    bl->addWidget(ipLbl);

    // Name / ID badge
    QString badgeText = profile.name.isEmpty()
                            ? (profile.carId.isEmpty() ? "未注册" : profile.carId)
                            : profile.name;
    if (badgeText.length() > 14) badgeText = badgeText.left(13) + "…";
    auto *badge = new QLabel(badgeText);
    badge->setAlignment(Qt::AlignCenter);
    badge->setStyleSheet(
        "color:#7ab0d8; font-size:11px;"
        "background:#1e2a3a;"
        "border:1px solid #2d4060;"
        "border-radius:3px; padding:2px 10px;");
    bl->addWidget(badge, 0, Qt::AlignCenter);

    root->addWidget(body, 1);

    // ── Footer ──────────────────────────────────────────────────────────────
    auto *ftr = new QWidget;
    ftr->setFixedHeight(50);
    ftr->setStyleSheet(
        "background:#1e2228;"
        "border-radius:0 0 8px 8px;"
        "border-top:1px solid #3a4050;");
    auto *fl = new QHBoxLayout(ftr);
    fl->setContentsMargins(10, 0, 10, 0);
    fl->setSpacing(8);

    if (profile.carId.isEmpty()) {
        auto *warn = new QLabel("⚠ 未同步");
        warn->setStyleSheet("color:#f0a060; font-size:11px; background:transparent;");
        fl->addWidget(warn);
    }
    fl->addStretch();

    m_connectBtn = new QPushButton("开始连接");
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
    fl->addWidget(m_connectBtn);

    root->addWidget(ftr);

    if (!profile.ip.isEmpty() && !profile.carId.isEmpty())
        checkOnlineStatus(profile.ip, profile.carId);
    else
        setOnlineUI(false);
}

void DeviceCard::setBusy(bool busy)
{
    m_connectBtn->setEnabled(!busy);
    m_connectBtn->setText(busy ? "连接中..." : "开始连接");
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

void DeviceCard::finishProbe()
{
    if (m_probeTimer) { m_probeTimer->stop(); m_probeTimer->deleteLater(); m_probeTimer = nullptr; }
    if (m_probeClient) {
        m_probeClient->disconnectFromHost();
        m_probeClient->deleteLater();
        m_probeClient = nullptr;
    }
}

// 主动连接该设备自己的 MQTT 服务并订阅其实时信息主题，以此判断设备是否真正
// 在线——而不是想当然地把卡片显示成固定的"离线"。用独立的探测连接，
// 不影响应用主连接(MqttService)当前连的是哪台设备。
void DeviceCard::checkOnlineStatus(const QString &ip, const QString &carId)
{
    m_probeResolved = false;
    m_onlineLabel->setText("检测中...");

    m_probeClient = new QMqttClient(this);
    m_probeClient->setHostname(ip);
    m_probeClient->setPort(AppConfig::mapPort(1883));
    m_probeClient->setUsername("admin");
    m_probeClient->setPassword("vsislab");
    m_probeClient->setClientId(QString("qt-agv-probe-%1").arg(QDateTime::currentMSecsSinceEpoch()));
    m_probeClient->setKeepAlive(60);

    connect(m_probeClient, &QMqttClient::connected, this, [this, carId]() {
        if (!m_probeClient) return;
        m_probeClient->subscribe(QString("/rob/%1/info").arg(carId), 0);
    });
    connect(m_probeClient, &QMqttClient::messageReceived, this, [this](const QByteArray &, const QMqttTopicName &) {
        if (m_probeResolved) return;
        m_probeResolved = true;
        setOnlineUI(true);
        finishProbe();
    });
    connect(m_probeClient, &QMqttClient::errorChanged, this, [this](QMqttClient::ClientError error) {
        if (m_probeResolved || error == QMqttClient::NoError) return;
        m_probeResolved = true;
        setOnlineUI(false);
        finishProbe();
    });

    m_probeTimer = new QTimer(this);
    m_probeTimer->setSingleShot(true);
    connect(m_probeTimer, &QTimer::timeout, this, [this]() {
        if (m_probeResolved) return;
        m_probeResolved = true;
        setOnlineUI(false);
        finishProbe();
    });
    // 3 秒对实际网络环境（端口映射/跨网段）偏短，会把仍在建连中的设备
    // 误判为离线，这里放宽到 8 秒再判定失败。
    m_probeTimer->start(8000);

    m_probeClient->connectToHost();
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

    m_cancelBtn = new QPushButton("← 返回");
    m_cancelBtn->setFixedHeight(32);
    m_cancelBtn->setStyleSheet(
        "QPushButton{background:transparent;color:#8898a8;border:1px solid #3a3f4a;"
        "border-radius:4px;padding:0 12px;font-size:12px;}"
        "QPushButton:hover{color:#c8d0d8;border-color:#8898a8;}");
    connect(m_cancelBtn, &QPushButton::clicked, this, &DeviceManagerDialog::onCancelAdd);
    phl->addWidget(m_cancelBtn);
    phl->addStretch();

    auto *panelTitle = new QLabel("添加设备");
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

    auto *fTitle = new QLabel("输入设备IP");
    fTitle->setStyleSheet("color:#c8d0d8; font-size:16px; font-weight:700;");
    fbl->addWidget(fTitle);

    fbl->addSpacing(4);

    m_ipEdit = new QLineEdit;
    m_ipEdit->setPlaceholderText("例如: 192.168.1.100");
    m_ipEdit->setFixedHeight(42);
    fbl->addWidget(m_ipEdit);

    m_addStatus = new QLabel;
    m_addStatus->setStyleSheet("color:#f0a060; font-size:12px;");
    m_addStatus->setWordWrap(true);
    m_addStatus->setMinimumHeight(18);
    fbl->addWidget(m_addStatus);

    m_confirmBtn = new QPushButton("连接并添加");
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
    // Remove old cards from grid
    for (DeviceCard *c : m_cards) {
        m_cardsGrid->removeWidget(c);
        c->deleteLater();
    }
    m_cards.clear();

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
        connect(card, &DeviceCard::connectClicked, this, &DeviceManagerDialog::onConnectCard);
        connect(card, &DeviceCard::removeClicked,  this, &DeviceManagerDialog::onRemoveCard);
        m_cardsGrid->addWidget(card, i / cols, i % cols);
        m_cards.append(card);
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
                        }
                    }

                    // 服务器没有返回设备名称时，让用户自己起一个并保存，而不是静默生成占位名
                    if (carName.isEmpty()) {
                        bool ok = false;
                        QString input = QInputDialog::getText(this, "设备名称",
                            "无法从服务器获取设备名称，请输入一个名称：",
                            QLineEdit::Normal, QString("AGV @ %1").arg(ip), &ok);
                        carName = (ok && !input.trimmed().isEmpty())
                                      ? input.trimmed() : QString("AGV @ %1").arg(ip);
                    }

                    DeviceProfile dev;
                    dev.ip            = ip;
                    dev.carId         = carId;
                    dev.token         = AppConfig::instance().token();
                    dev.name          = carName;
                    dev.lastConnected = QDateTime::currentDateTime();
                    AppConfig::instance().saveDevice(dev);
                    AppConfig::instance().setAgvId(carId);

                    m_stack->setCurrentWidget(m_mainPanel);
                    refreshCards();
                    setStatus(QString("设备 [%1] 已添加").arg(dev.name), "#5cb85c");
                    Toast::success(this, QString("设备 [%1] 已添加").arg(dev.name));
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

void DeviceManagerDialog::connectDevice(int index)
{
    const QList<DeviceProfile> list = AppConfig::instance().deviceList();
    if (index < 0 || index >= list.size()) return;

    if (index < m_cards.size())
        m_cards[index]->setBusy(true);

    AppConfig::instance().setCurrentDeviceIndex(index);
    AppConfig::instance().setServerIp(list[index].ip);
    AppConfig::instance().setAgvId(list[index].carId);

    setStatus(QString("正在连接 %1 ...").arg(list[index].ip));

    ApiService::instance().login("admin", "admin123",
        [this, index](int code, const QJsonObject &data) {
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

void DeviceManagerDialog::setStatus(const QString &msg, const QString &color)
{
    if (!m_statusLabel) return;
    m_statusLabel->setText(msg);
    m_statusLabel->setStyleSheet(
        QString("background:#0d1117; color:%1; font-size:11px;"
                "padding:0 16px; border-top:1px solid #2d3340;").arg(color));
}
