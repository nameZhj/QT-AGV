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
#include <QPixmap>
#include <QIcon>
#include <QInputDialog>
#include <QTimer>
#include <QPointer>
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

    auto mkDelBtn = []() {
        // 绘制垃圾桶图标，避免文字字符渲染问题
        QPixmap pm(20, 20);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        QColor col("#e8e8e8");
        p.setPen(QPen(col, 1.5, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);
        // 桶身
        p.drawRoundedRect(QRectF(5, 7, 10, 11), 1.5, 1.5);
        // 桶盖
        p.drawLine(QPointF(3.5, 7), QPointF(16.5, 7));
        p.drawLine(QPointF(7, 4.5), QPointF(13, 4.5));
        p.drawLine(QPointF(8, 4.5), QPointF(8, 7));
        p.drawLine(QPointF(12, 4.5), QPointF(12, 7));
        // 桶身竖线
        p.drawLine(QPointF(8.5, 9), QPointF(8.5, 16));
        p.drawLine(QPointF(11.5, 9), QPointF(11.5, 16));
        p.end();

        auto *b = new QPushButton;
        b->setFlat(true);
        b->setFixedSize(30, 30);
        b->setIcon(QIcon(pm));
        b->setIconSize(QSize(18, 18));
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
    if (m_nameEdit) {
        m_nameEdit->clear();
        m_nameRow->hide();
        m_confirmBtn->setText("连接并添加");
    }
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

    // 如果设备名称行已展开，说明此次点击是确认创建新设备
    if (m_nameRow && m_nameRow->isVisible()) {
        QString carName = m_nameEdit->text().trimmed();
        if (carName.isEmpty()) {
            m_addStatus->setStyleSheet("color:#e05555; font-size:12px;");
            m_addStatus->setText("请输入设备名称");
            return;
        }
        m_confirmBtn->setEnabled(false);
        m_addStatus->setStyleSheet("color:#f0a060; font-size:12px;");
        m_addStatus->setText("正在创建设备...");

        // 参考 Vue submitCarForm: 先登录，再调用 addCarWithoutKey
        ApiService::instance().login("admin", "admin123",
            [this, ip, carName](int code, const QJsonObject &data) {
                if (code != 200 || data.value("token").toString().isEmpty()) {
                    m_addStatus->setStyleSheet("color:#e05555; font-size:12px;");
                    m_addStatus->setText("登录失败: " +
                        data.value("msg").toString("服务器无响应"));
                    m_confirmBtn->setEnabled(true);
                    return;
                }
                AppConfig::instance().setToken(data.value("token").toString());

                // 构造与 Vue carForm 等价的请求体
                QJsonObject carForm;
                carForm["deptId"]    = 100;
                carForm["mapId"]     = QJsonValue();
                carForm["carName"]   = carName;
                carForm["ipPort"]    = ip + ":8061";
                carForm["robotType"] = "1";
                carForm["machinery"] = "0";

                ApiService::instance().addCarWithoutKey(carForm,
                    [this, ip, carName](int code2, const QJsonObject &res) {
                        if (code2 != 200) {
                            m_addStatus->setStyleSheet("color:#e05555; font-size:12px;");
                            m_addStatus->setText("创建设备失败: " +
                                res.value("msg").toString("请检查服务器"));
                            m_confirmBtn->setEnabled(true);
                            return;
                        }
                        // 获取服务器返回的 carId
                        QJsonValue v = res.value("data").toObject().value("carId");
                        QString carId = v.isString() ? v.toString()
                                                     : QString::number(v.toInt());

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
                        setStatus(QString("设备 [%1] 已创建并添加").arg(dev.name), "#5cb85c");
                        Toast::success(this, QString("设备 [%1] 已创建并添加").arg(dev.name));
                    });
            });
        return;
    }

    // 第一阶段: 登录并尝试获取 carId
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

            // 参考 Vue submitForm: 查询设备列表，判断是否已有设备
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

                    if (!carId.isEmpty()) {
                        // 服务器已有设备，直接保存（对应 Vue 有设备时的处理逻辑）
                        if (carName.isEmpty()) carName = QString("AGV @ %1").arg(ip);

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
                    } else {
                        // 服务器无设备，展开设备名称输入行，要求用户输入名称创建
                        // （对应 Vue 无设备时展示 createCar 对话框）
                        m_addStatus->setStyleSheet("color:#f0a060; font-size:12px;");
                        m_addStatus->setText("服务器无已注册设备，请输入设备名称进行创建");
                        m_nameEdit->setText(QString("AGV @ %1").arg(ip));
                        m_nameRow->show();
                        m_confirmBtn->setEnabled(true);
                        m_confirmBtn->setText("创建并添加");
                    }
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

void DeviceManagerDialog::setStatus(const QString &msg, const QString &color)
{
    if (!m_statusLabel) return;
    m_statusLabel->setText(msg);
    m_statusLabel->setStyleSheet(
        QString("background:#0d1117; color:%1; font-size:11px;"
                "padding:0 16px; border-top:1px solid #2d3340;").arg(color));
}
