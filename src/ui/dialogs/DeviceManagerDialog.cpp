#include "DeviceManagerDialog.h"
#include "ui/dialogs/NetworkConfigDialog.h"
#include "ApiService.h"
#include "AppConfig.h"
#include "AppConstants.h"
#include "MqttService.h"
#include "RobotInfoManager.h"
#include <QMouseEvent>
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
#include <QTcpSocket>
#include <QElapsedTimer>
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

// ── 地图列表缓存与匹配 (/robot/map/listAll) ─────────────────────────────────
static QMap<QString, QString> s_mapIdToName;
static bool s_isFetchingMaps = false;

static void fetchGlobalMapList(std::function<void()> onDone = nullptr) {
    if (s_isFetchingMaps) {
        if (onDone) onDone();
        return;
    }
    s_isFetchingMaps = true;
    ApiService::instance().listMapAll(QJsonObject{{"pageNum", 1}, {"pageSize", 1000}}, [onDone](int code, const QJsonObject &data) {
        s_isFetchingMaps = false;
        if (code == 200) {
            const QJsonArray rows = data.value("rows").toArray();
            for (const auto &v : rows) {
                QJsonObject m = v.toObject();
                QString mid = jsonIdStr(m.value("mapId"));
                if (mid.isEmpty() && m.contains("mapId")) mid = QString::number(m.value("mapId").toInt());
                QString mname = m.value("mapName").toString().trimmed();
                if (!mid.isEmpty() && !mname.isEmpty()) {
                    s_mapIdToName[mid] = mname;
                }
            }
        }
        if (onDone) onDone();
    });
}

static QString lookupMapName(const QString &mapId) {
    if (mapId.isEmpty() || mapId == "0") return QString();
    if (s_mapIdToName.contains(mapId)) {
        return s_mapIdToName.value(mapId);
    }
    return QString();
}

// ═══════════════════════════════════════════════════════════════════════════════
//  BatteryBadge
// ═══════════════════════════════════════════════════════════════════════════════

BatteryBadge::BatteryBadge(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(36, 16);
}

void BatteryBadge::setBattery(int level)
{
    m_level = level;
    update();
}

void BatteryBadge::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // 电池外框
    QRectF body(1, 1, 28, 14);
    p.setPen(QPen(QColor(255, 255, 255, 220), 1.2));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(body, 2, 2);

    // 电池正极极柱
    QRectF tip(29, 4.5, 2.5, 7);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 220));
    p.drawRoundedRect(tip, 1, 1);

    // 电池内部电量文本
    p.setPen(QColor("#ffffff"));
    QFont f = font();
    f.setPixelSize(10);
    f.setBold(true);
    p.setFont(f);
    QString txt = (m_level >= 0) ? QString::number(m_level) : "--";
    p.drawText(body, Qt::AlignCenter, txt);
}

static QPixmap makeSlidersIcon(int size = 18, const QColor &color = QColor("#ffffff"))
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap));

    p.drawLine(QPointF(2, 4), QPointF(size - 2, 4));
    p.setBrush(color);
    p.drawEllipse(QPointF(6, 4), 2, 2);

    p.drawLine(QPointF(2, 9), QPointF(size - 2, 9));
    p.drawEllipse(QPointF(size - 6, 9), 2, 2);

    p.drawLine(QPointF(2, 14), QPointF(size - 2, 14));
    p.drawEllipse(QPointF(8, 14), 2, 2);

    p.end();
    return pm;
}

static QPixmap makeRobotHeadIcon(int size = 26, const QColor &color = QColor("#00e5ff"))
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap));

    p.drawLine(QPointF(size / 2.0, 1.5), QPointF(size / 2.0, 6.0));
    p.setBrush(color);
    p.drawEllipse(QPointF(size / 2.0, 2.0), 1.5, 1.5);

    QRectF head(3.5, 6.0, size - 7.0, size - 10.0);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(head, 2.5, 2.5);

    p.setBrush(color);
    p.drawEllipse(QPointF(size / 2.0 - 4.5, 11.5), 1.5, 1.5);
    p.drawEllipse(QPointF(size / 2.0 + 4.5, 11.5), 1.5, 1.5);

    p.drawLine(QPointF(size / 2.0 - 3.0, 16.5), QPointF(size / 2.0 + 3.0, 16.5));

    p.end();
    return pm;
}

static QPixmap makeBaseStationIcon(int w = 26, int h = 20, const QColor &color = QColor("#00e5ff"))
{
    QPixmap pm(w, h);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    p.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap));
    p.setBrush(Qt::NoBrush);
    p.drawArc(QRectF(w / 2.0 - 8, 2, 16, 12), 45 * 16, 90 * 16);
    p.drawArc(QRectF(w / 2.0 - 4.5, 5, 9, 8), 45 * 16, 90 * 16);

    p.drawRoundedRect(QRectF(3.5, 13.5, w - 7, 5), 1.5, 1.5);

    p.end();
    return pm;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  DeviceCard
// ═══════════════════════════════════════════════════════════════════════════════

DeviceCard::DeviceCard(const DeviceProfile &profile, int index, QWidget *parent)
    : QFrame(parent), m_index(index), m_profile(profile)
{
    setObjectName("deviceCard");
    setFrameShape(QFrame::StyledPanel);
    setFixedSize(260, 335);

    // 整卡立体投影效果：让整个 item 从弹窗底色中清晰凸起，形成明确的卡片感
    m_shadow = new QGraphicsDropShadowEffect(this);
    m_shadow->setBlurRadius(16);
    m_shadow->setOffset(0, 4);
    m_shadow->setColor(QColor(0, 0, 0, 160));
    setGraphicsEffect(m_shadow);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header bar (参考图顶部栏：☐ 已连接 🟢 机器人  🎛️  [47]) ─────────────────
    m_hdrWidget = new QWidget;
    m_hdrWidget->setFixedHeight(32);
    auto *hl = new QHBoxLayout(m_hdrWidget);
    hl->setContentsMargins(10, 0, 8, 0);
    hl->setSpacing(6);

    // 1. Checkbox icon (白框方形)
    auto *chkBox = new QLabel;
    chkBox->setFixedSize(12, 12);
    chkBox->setStyleSheet("border:1.5px solid #ffffff; border-radius:2px; background:transparent;");
    hl->addWidget(chkBox);

    // 2. 连接文字 (默认未连接)
    m_onlineLabel = new QLabel("未连接");
    m_onlineLabel->setStyleSheet("color:#94a3b8; font-size:12px; font-weight:bold; background:transparent;");
    hl->addWidget(m_onlineLabel);

    // 3. 状态指示灯 (圆形灰点)
    m_onlineDot = new QLabel;
    m_onlineDot->setFixedSize(10, 10);
    m_onlineDot->setStyleSheet("background:#718096; border-radius:5px;");
    hl->addWidget(m_onlineDot);

    // 4. "机器人" 文本
    m_robLabel = new QLabel("机器人");
    m_robLabel->setStyleSheet("color:#ffffff; font-size:12px; font-weight:500; background:transparent;");
    hl->addWidget(m_robLabel);

    hl->addStretch();

    // 5. 调优/网络配置滑块图标 🎛️ (可点击直接打开 SSH 网络配置)
    m_configBtn = new QPushButton;
    m_configBtn->setFlat(true);
    m_configBtn->setFixedSize(22, 22);
    m_configBtn->setIcon(QIcon(makeSlidersIcon(18, QColor("#ffffff"))));
    m_configBtn->setIconSize(QSize(16, 16));
    m_configBtn->setCursor(Qt::PointingHandCursor);
    m_configBtn->setToolTip("网络参数配置 (SSH)");
    m_configBtn->setStyleSheet("QPushButton{background:transparent;border:none;border-radius:3px;}"
                               "QPushButton:hover{background:rgba(255,255,255,0.25);}");
    connect(m_configBtn, &QPushButton::clicked, this, [this]{ emit networkConfigClicked(m_index); });
    hl->addWidget(m_configBtn);

    // 6. 电池电量组件
    m_batteryBadge = new BatteryBadge;
    hl->addWidget(m_batteryBadge);

    // 7. 小尺寸删除按钮
    auto *delBtn = new QPushButton("✕");
    delBtn->setFixedSize(18, 18);
    delBtn->setCursor(Qt::PointingHandCursor);
    delBtn->setToolTip("从列表中移除该设备");
    delBtn->setStyleSheet(
        "QPushButton{background:transparent;color:rgba(255,255,255,0.7);border:none;font-size:11px;font-weight:bold;}"
        "QPushButton:hover{background:rgba(239,68,68,0.85);color:#fff;border-radius:3px;}");
    connect(delBtn, &QPushButton::clicked, this, [this]{ emit removeClicked(m_index); });
    hl->addWidget(delBtn);

    root->addWidget(m_hdrWidget);

    // ── Body ────────────────────────────────────────────────────────────────
    auto *body = new QWidget;
    body->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *bl = new QVBoxLayout(body);
    bl->setContentsMargins(14, 10, 14, 6);
    bl->setSpacing(6);

    // 1. 首行: 青色机器人图标 + IP + [ 潜伏底盘 ] (固定写死机器人类型)
    auto *topRow = new QHBoxLayout;
    topRow->setSpacing(6);

    m_robotIcon = new QLabel;
    m_robotIcon->setFixedSize(26, 26);
    m_robotIcon->setPixmap(makeRobotHeadIcon(26, QColor("#00e5ff")));
    m_robotIcon->setStyleSheet("background:transparent;");
    topRow->addWidget(m_robotIcon);

    m_ipLabel = new QLabel(profile.ip);
    m_ipLabel->setStyleSheet("color:#ffffff; font-size:13px; font-weight:bold; font-family:Consolas,monospace; background:transparent;");
    topRow->addWidget(m_ipLabel);

    topRow->addStretch();

    m_typeTag = new QLabel("潜伏底盘");
    m_typeTag->setStyleSheet(
        "color:#00e5ff; font-size:11px; font-weight:bold; background:transparent;"
        "border:1.2px solid #00e5ff; border-radius:2px; padding:1px 5px;");
    topRow->addWidget(m_typeTag);

    bl->addLayout(topRow);

    // 2. 橙色置信度底色块 (0.35) + 绿色通讯延迟 (9ms)
    auto *metricsRow = new QHBoxLayout;
    metricsRow->setSpacing(8);

    m_confBadge = new QLabel("--");
    m_confBadge->setAlignment(Qt::AlignCenter);
    m_confBadge->setFixedHeight(22);
    m_confBadge->setStyleSheet(
        "background:#ff6600; color:#ffffff; font-size:12px; font-weight:bold; "
        "border-radius:3px; padding:0 8px; min-width:42px;");
    metricsRow->addWidget(m_confBadge);

    m_latencyLabel = new QLabel("--");
    m_latencyLabel->setStyleSheet("color:#64748b; font-size:12px; font-weight:bold; font-family:Consolas,monospace; background:transparent;");
    metricsRow->addWidget(m_latencyLabel);

    metricsRow->addStretch();
    bl->addLayout(metricsRow);

    // 3. 信息列表: 地图、名称、备注、版本、固件版本
    auto *infoLay = new QVBoxLayout;
    infoLay->setSpacing(4);
    infoLay->setContentsMargins(0, 2, 0, 0);

    auto mkRow = [](const QString &keyTxt, QLabel *&valLbl) {
        auto *row = new QHBoxLayout;
        row->setSpacing(4);
        auto *kl = new QLabel(keyTxt);
        kl->setStyleSheet("color:#cbd5e1; font-size:12px; background:transparent;");
        row->addWidget(kl);

        valLbl = new QLabel("--");
        valLbl->setStyleSheet("color:#f1f5f9; font-size:12px; font-weight:500; background:transparent;");
        row->addWidget(valLbl, 1);
        return row;
    };

    infoLay->addLayout(mkRow("地图:", m_mapLabel));
    infoLay->addLayout(mkRow("名称:", m_nameLabel));
    infoLay->addLayout(mkRow("备注:", m_remarkLabel));
    infoLay->addLayout(mkRow("版本:", m_verLabel));
    infoLay->addLayout(mkRow("固件版本:", m_fwVerLabel));

    bl->addLayout(infoLay);

    // 4. 青色基站/无线图标 (参考图红框下方左侧基站图标)
    m_baseIcon = new QLabel;
    m_baseIcon->setFixedSize(26, 20);
    m_baseIcon->setPixmap(makeBaseStationIcon(26, 20, QColor("#00e5ff")));
    m_baseIcon->setStyleSheet("background:transparent;");
    bl->addWidget(m_baseIcon, 0, Qt::AlignLeft);

    bl->addStretch();
    root->addWidget(body, 1);

    // ── Footer ──────────────────────────────────────────────────────────────
    auto *ftr = new QWidget;
    ftr->setFixedHeight(42);
    ftr->setStyleSheet("background:transparent; border:none;");
    auto *fl = new QHBoxLayout(ftr);
    fl->setContentsMargins(10, 0, 10, 6);
    fl->setSpacing(10);

    // 左侧: [✓] 已同步 按钮 (对应参考图左下角)
    m_syncBtn = new QPushButton("✓ 已同步");
    m_syncBtn->setCursor(Qt::PointingHandCursor);
    m_syncBtn->setStyleSheet(
        "QPushButton{"
        "  background:#333842; color:#38bdf8; border:1px solid #4a5160;"
        "  border-radius:3px; padding:4px 10px; font-size:11px; font-weight:bold;"
        "}"
        "QPushButton:hover{background:#3f4756;}");
    fl->addWidget(m_syncBtn);

    fl->addStretch();

    // 右侧: 操作按钮 (未连接显示"连接"，已连接显示"断开连接"，互斥常驻显示，绝不隐藏)
    m_connectBtn = new QPushButton("连接", ftr);
    m_connectBtn->setObjectName("deviceCardConnectBtn");
    m_connectBtn->setProperty("noClickEffect", true);
    m_connectBtn->setGraphicsEffect(nullptr);
    m_connectBtn->setCursor(Qt::PointingHandCursor);
    m_connectBtn->setFixedHeight(26);
    m_connectBtn->setMinimumWidth(72);
    m_connectBtn->setVisible(true);
    m_connectBtn->setEnabled(true);
    m_connectBtn->setStyleSheet(
        "QPushButton#deviceCardConnectBtn{"
        "  background:#0284c7; color:#ffffff; border:1px solid #0369a1;"
        "  border-radius:3px; padding:4px 14px; font-size:11px; font-weight:bold;"
        "}"
        "QPushButton#deviceCardConnectBtn:hover{background:#0ea5e9;}"
        "QPushButton#deviceCardConnectBtn:pressed{background:#0369a1;}");
    connect(m_connectBtn, &QPushButton::clicked, this, [this]{
        bool isCurrent = (AppConfig::instance().currentDeviceIndex() == m_index);
        if (isCurrent) {
            emit disconnectClicked(m_index);
        } else {
            emit connectClicked(m_index);
        }
    });
    fl->addWidget(m_connectBtn);

    root->addWidget(ftr);

    // 设置整卡手型光标与双击进入主页提示
    setCursor(Qt::PointingHandCursor);
    setToolTip("双击卡片进入主页");

    // 默认版本与固件版本统一为 v2.1.1
    if (m_profile.version.isEmpty() || m_profile.version.startsWith("v3.4.8")) {
        m_profile.version = "v2.1.1";
    }
    m_fwVersion = "v2.1.1";

    // 初始化样式与数据显示 (若此卡片为当前已连接设备则立即高亮显示并展示"断开连接")
    bool isCurrent = (AppConfig::instance().currentDeviceIndex() == m_index);
    setOnlineUI(isCurrent);
    updateCardDisplay();

    // 为卡片内部所有非按钮子控件安装事件过滤器，确保双击任何非按钮位置均能进入主页
    const auto widgets = findChildren<QWidget*>();
    for (QWidget *w : widgets) {
        if (!qobject_cast<QAbstractButton*>(w)) {
            w->installEventFilter(this);
        }
    }

    // 仅当此设备已由用户主动连接时才执行探活，初始未连接状态不自动连接或探测
    if (isCurrent && !profile.ip.isEmpty()) {
        probeDeviceStatus(profile.ip);
    }
}

DeviceCard::~DeviceCard()
{
    if (m_latencyTimer) {
        m_latencyTimer->stop();
    }
}

void DeviceCard::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // 检查双击位置是否在任何已知按钮上，防止误触发
        QWidget *child = childAt(event->pos());
        while (child && child != this) {
            if (qobject_cast<QAbstractButton*>(child)) {
                QFrame::mouseDoubleClickEvent(event);
                return;
            }
            child = child->parentWidget();
        }
        emit doubleClicked(m_index);
        event->accept();
        return;
    }
    QFrame::mouseDoubleClickEvent(event);
}

bool DeviceCard::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonDblClick) {
        auto *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            if (!qobject_cast<QAbstractButton*>(watched)) {
                emit doubleClicked(m_index);
                return true;
            }
        }
    }
    return QFrame::eventFilter(watched, event);
}

void DeviceCard::disconnectDevice()
{
    m_isBusy = false;
    if (m_latencyTimer) {
        m_latencyTimer->stop();
    }
    m_latency = -1;
    m_isProbingLatency = false;
    m_confidence = -1.0;
    m_statusText = "未连接";
    setOnlineUI(false);
}

void DeviceCard::setBusy(bool busy)
{
    m_isBusy = busy;
    updateCardDisplay();
}

void DeviceCard::setLinkStatus(bool /*linked*/)
{
}

void DeviceCard::setOnlineUI(bool online)
{
    m_isOnline = online;
    if (online) {
        setStyleSheet(
            "QFrame#deviceCard {"
            "  background: #252a35;"
            "  border: 2px solid #0091da;"
            "  border-radius: 8px;"
            "}"
            "QFrame#deviceCard:hover {"
            "  background: #28303e;"
            "  border-color: #00b4d8;"
            "}");
        if (m_hdrWidget) {
            m_hdrWidget->setStyleSheet("background: #0091da; border-top-left-radius: 6px; border-top-right-radius: 6px;");
        }
        if (m_shadow) {
            m_shadow->setColor(QColor(0, 145, 218, 80));
            m_shadow->setBlurRadius(20);
            m_shadow->setOffset(0, 4);
        }
        if (m_onlineDot) {
            m_onlineDot->setStyleSheet("background: #44db22; border-radius: 5px;");
        }
        if (m_onlineLabel) {
            m_onlineLabel->setText(m_statusText.isEmpty() ? "已连接" : m_statusText);
            m_onlineLabel->setStyleSheet("color: #ffffff; font-size: 12px; font-weight: bold; background: transparent;");
        }
        if (m_robotIcon) {
            m_robotIcon->setPixmap(makeRobotHeadIcon(26, QColor("#00e5ff")));
        }
        if (m_ipLabel) {
            m_ipLabel->setStyleSheet("color: #ffffff; font-size: 13px; font-weight: bold; font-family: Consolas,monospace; background: transparent;");
        }
        if (m_typeTag) {
            m_typeTag->setStyleSheet("color: #00e5ff; font-size: 11px; font-weight: bold; background: transparent; border: 1.2px solid #00e5ff; border-radius: 2px; padding: 1px 5px;");
        }
        if (m_baseIcon) {
            m_baseIcon->setPixmap(makeBaseStationIcon(26, 20, QColor("#00e5ff")));
        }
    } else {
        setStyleSheet(
            "QFrame#deviceCard {"
            "  background: #1f232c;"
            "  border: 1.5px solid #475569;"
            "  border-radius: 8px;"
            "}"
            "QFrame#deviceCard:hover {"
            "  background: #242934;"
            "  border-color: #64748b;"
            "}");
        if (m_hdrWidget) {
            m_hdrWidget->setStyleSheet("background: #2d3544; border-top-left-radius: 6px; border-top-right-radius: 6px;");
        }
        if (m_shadow) {
            m_shadow->setColor(QColor(0, 0, 0, 160));
            m_shadow->setBlurRadius(16);
            m_shadow->setOffset(0, 4);
        }
        if (m_onlineDot) {
            m_onlineDot->setStyleSheet("background: #718096; border-radius: 5px;");
        }
        if (m_onlineLabel) {
            m_onlineLabel->setText(m_statusText.isEmpty() ? "未连接" : m_statusText);
            m_onlineLabel->setStyleSheet("color: #94a3b8; font-size: 12px; font-weight: bold; background: transparent;");
        }
        if (m_robotIcon) {
            m_robotIcon->setPixmap(makeRobotHeadIcon(26, QColor("#718096")));
        }
        if (m_ipLabel) {
            m_ipLabel->setStyleSheet("color: #94a3b8; font-size: 13px; font-weight: bold; font-family: Consolas,monospace; background: transparent;");
        }
        if (m_typeTag) {
            m_typeTag->setStyleSheet("color: #718096; font-size: 11px; font-weight: bold; background: transparent; border: 1.2px solid #4a5568; border-radius: 2px; padding: 1px 5px;");
        }
        if (m_baseIcon) {
            m_baseIcon->setPixmap(makeBaseStationIcon(26, 20, QColor("#4a5568")));
        }
        m_confidence = -1.0;
        m_battery = -1;
        m_latency = -1;
    }
    updateCardDisplay();
}

void DeviceCard::updateCardDisplay()
{
    // 1. 设备名称 (如 AMB-01)
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

    // 2. 绑定地图名称 (如从接口查询到的真实地图名称)
    QString mapName = m_profile.mapName.trimmed();
    if (mapName.startsWith("地图 ID:", Qt::CaseInsensitive)) {
        QString mid = mapName.mid(QString("地图 ID:").length()).trimmed();
        QString realName = lookupMapName(mid);
        mapName = realName.isEmpty() ? mid : realName;
        m_profile.mapName = mapName;
    } else {
        bool ok = false;
        mapName.toInt(&ok);
        if (ok) {
            QString realName = lookupMapName(mapName);
            if (!realName.isEmpty()) {
                mapName = realName;
                m_profile.mapName = realName;
            }
        }
    }

    if (mapName.isEmpty()) {
        m_mapLabel->setText("--");
    } else {
        if (mapName.length() > 14) {
            m_mapLabel->setText(mapName.left(13) + "…");
            m_mapLabel->setToolTip(mapName);
        } else {
            m_mapLabel->setText(mapName);
            m_mapLabel->setToolTip(mapName);
        }
    }

    // 3. 备注 (如空则显示空白)
    if (m_remarkLabel) {
        m_remarkLabel->setText(m_remark.trimmed());
    }

    // 4. 软件版本 (版本改为：v2.1.1)
    QString ver = m_profile.version.trimmed();
    if (ver.isEmpty() || ver == "未知" || ver.startsWith("v3.4.8")) {
        ver = "v2.1.1";
    }
    m_verLabel->setText(ver);

    // 5. 固件版本 (固件版本改为：v2.1.1)
    QString fwVer = m_fwVersion.trimmed();
    if (fwVer.isEmpty() || fwVer == "未知" || fwVer.startsWith("v22A")) {
        fwVer = "v2.1.1";
    }
    m_fwVerLabel->setText(fwVer);

    // 6. 机器人类型 (固定写死：潜伏底盘)
    if (m_typeTag) {
        m_typeTag->setText("潜伏底盘");
    }

    // 7. 位置置信度 badge (如 0.35)
    if (m_confBadge) {
        if (m_confidence >= 0.0) {
            m_confBadge->setText(QString::number(m_confidence, 'f', 2));
            m_confBadge->setStyleSheet("background:#ff6600; color:#ffffff; font-size:12px; font-weight:bold; border-radius:3px; padding:0 8px; min-width:42px;");
        } else {
            m_confBadge->setText("--");
            m_confBadge->setStyleSheet("background:#374151; color:#9ca3af; font-size:12px; font-weight:bold; border-radius:3px; padding:0 8px; min-width:42px;");
        }
    }

    // 8. 通讯延迟 (如 9ms)
    if (m_latencyLabel) {
        if (m_latency >= 0) {
            QString latStr = QString("%1ms").arg(m_latency);
            if (m_latencyLabel->text() != latStr) {
                m_latencyLabel->setText(latStr);
                m_latencyLabel->setStyleSheet("color:#44db22; font-size:12px; font-weight:bold; font-family:Consolas,monospace; background:transparent;");
            }
        } else {
            if (m_latencyLabel->text() != "--") {
                m_latencyLabel->setText("--");
                m_latencyLabel->setStyleSheet("color:#64748b; font-size:12px; font-weight:bold; font-family:Consolas,monospace; background:transparent;");
            }
        }
    }

    // 9. 电量 (如 47)
    if (m_batteryBadge) {
        m_batteryBadge->setBattery(m_battery);
    }

    // 10. 底部状态与操作按钮
    bool isConnected = (AppConfig::instance().currentDeviceIndex() == m_index);
    if (m_syncBtn) {
        if (isConnected) {
            m_syncBtn->setText("✓ 已同步");
            m_syncBtn->setStyleSheet(
                "QPushButton{"
                "  background:#1e3a5f; color:#38bdf8; border:1px solid #0284c7;"
                "  border-radius:3px; padding:4px 10px; font-size:11px; font-weight:bold;"
                "}");
        } else {
            m_syncBtn->setText("未同步");
            m_syncBtn->setStyleSheet(
                "QPushButton{"
                "  background:#333842; color:#94a3b8; border:1px solid #4a5160;"
                "  border-radius:3px; padding:4px 10px; font-size:11px; font-weight:bold;"
                "}");
        }
    }

    // 核心操作按钮：未连接显示"连接"，已连接显示"断开连接"，永远常驻可见且互斥切换
    if (m_connectBtn) {
        if (m_connectBtn->graphicsEffect()) {
            m_connectBtn->setGraphicsEffect(nullptr);
        }
        m_connectBtn->setVisible(true);
        m_connectBtn->setEnabled(true);
        if (isConnected) {
            m_connectBtn->setText("断开连接");
            m_connectBtn->setStyleSheet(
                "QPushButton#deviceCardConnectBtn{"
                "  background:#333842; color:#f87171; border:1px solid #7f1d1d;"
                "  border-radius:3px; padding:4px 12px; font-size:11px; font-weight:bold;"
                "}"
                "QPushButton#deviceCardConnectBtn:hover{background:#450a0a; color:#fca5a5;}"
                "QPushButton#deviceCardConnectBtn:pressed{background:#2b0505;}");
        } else {
            m_connectBtn->setText("连接");
            m_connectBtn->setStyleSheet(
                "QPushButton#deviceCardConnectBtn{"
                "  background:#0284c7; color:#ffffff; border:1px solid #0369a1;"
                "  border-radius:3px; padding:4px 14px; font-size:11px; font-weight:bold;"
                "}"
                "QPushButton#deviceCardConnectBtn:hover{background:#0ea5e9;}"
                "QPushButton#deviceCardConnectBtn:pressed{background:#0369a1;}");
        }
        m_connectBtn->update();
    }
}

void DeviceCard::testLatency(const QString &ip)
{
    if (ip.isEmpty() || !m_isOnline || (AppConfig::instance().currentDeviceIndex() != m_index)) {
        return;
    }
    if (m_isProbingLatency) {
        return;
    }

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_latency >= 0 && (now - m_lastLatencyProbeMs < 3000)) {
        return; // 已有延迟数值且 3 秒内已测过，避免频繁测速
    }
    m_lastLatencyProbeMs = now;
    m_isProbingLatency = true;

    auto *socket = new QTcpSocket(this);
    auto *timer = new QElapsedTimer;
    timer->start();

    // 设定连接超时定时器 (1500ms)，防止挂起
    QTimer::singleShot(1500, socket, [this, socket, timer]() {
        if (socket && socket->state() != QAbstractSocket::ConnectedState) {
            m_isProbingLatency = false;
            socket->abort();
            socket->deleteLater();
            delete timer;
        }
    });

    connect(socket, &QTcpSocket::connected, this, [this, timer, socket]() {
        int elapsed = static_cast<int>(timer->elapsed());
        if (m_isOnline && (AppConfig::instance().currentDeviceIndex() == m_index)) {
            m_latency = qMax(1, elapsed);
            updateCardDisplay();
        }
        m_isProbingLatency = false;
        socket->disconnectFromHost();
        socket->deleteLater();
        delete timer;
    });

    connect(socket, &QTcpSocket::errorOccurred, this, [this, socket, timer](QAbstractSocket::SocketError) {
        m_isProbingLatency = false;
        socket->deleteLater();
        delete timer;
    });

    socket->connectToHost(ip, 8048);
}

void DeviceCard::probeDeviceStatus(const QString &ip)
{
    if (ip.isEmpty()) {
        disconnectDevice();
        return;
    }

    // 检查此卡片是否就是全局当前选定并连接的设备
    bool isCurrent = (AppConfig::instance().currentDeviceIndex() == m_index);

    if (isCurrent) {
        m_isBusy = false;
        setOnlineUI(true);
        const RobotInfo &info = RobotInfoManager::instance().currentInfo();
        if (info.isOnline()) {
            m_battery = info.batteryPercent();
            m_confidence = info.covScore;
            m_statusText = info.statusText();
            // 报文若含有有效延迟才更新，绝不以 -1 覆盖已有延迟，防止闪烁
            if (info.latency >= 0) {
                m_latency = info.latency;
            }
        } else {
            if (m_statusText.isEmpty() || m_statusText == "未连接") {
                m_statusText = "已连接";
            }
        }
        updateCardDisplay();

        // 保持延迟测速定时器常驻运行（每 3 秒刷新一次）
        if (!m_latencyTimer) {
            m_latencyTimer = new QTimer(this);
            connect(m_latencyTimer, &QTimer::timeout, this, [this]() {
                if (m_isOnline && (AppConfig::instance().currentDeviceIndex() == m_index) && !m_profile.ip.isEmpty()) {
                    testLatency(m_profile.ip);
                }
            });
        }
        if (!m_latencyTimer->isActive()) {
            m_latencyTimer->start(3000);
        }

        // 若尚未取得有效延迟数据，立即异步测试一次
        if (m_latency < 0) {
            testLatency(ip);
        }
        return;
    }

    // 若当前卡片正在连接中，不被其他断开事件重置
    if (m_isBusy) {
        return;
    }

    // 未连接该设备时，统一严格展示为未连接状态，通讯延迟显示 "--"，绝不误显为已连接
    disconnectDevice();
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
    setWindowTitle(AppConstants::AppTitle);
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

    // 实时同步 MQTT 连接与车辆数据到卡片
    connect(&MqttService::instance(), &MqttService::connected, this, [this]() {
        int curIdx = AppConfig::instance().currentDeviceIndex();
        if (curIdx >= 0 && curIdx < m_cards.size() && m_cards[curIdx]) {
            m_cards[curIdx]->setBusy(false);
            m_cards[curIdx]->probeDeviceStatus(AppConfig::instance().serverIp());
            setStatus(QString("设备 [%1] 已成功连接").arg(AppConfig::instance().serverIp()), "#22c55e");
        }
    });
    connect(&MqttService::instance(), &MqttService::disconnected, this, [this]() {
        for (auto *c : m_cards) {
            if (c) c->probeDeviceStatus(AppConfig::instance().serverIp());
        }
    });
    connect(&RobotInfoManager::instance(), &RobotInfoManager::currentRobotInfoUpdated, this, [this]() {
        int curIdx = AppConfig::instance().currentDeviceIndex();
        if (curIdx >= 0 && curIdx < m_cards.size() && m_cards[curIdx]) {
            m_cards[curIdx]->probeDeviceStatus(AppConfig::instance().serverIp());
        }
    });
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
    auto *enterHomeBtn  = mkToolBtn("进入主页 →", false);
    enterHomeBtn->setStyleSheet(
        "QPushButton{background:#1e3a5f;color:#38bdf8;border:1px solid #0284c7;"
        "border-radius:4px;padding:0 14px;font-size:12px;font-weight:700;}"
        "QPushButton:hover{background:#0284c7;color:#fff;}");

    connect(refreshAllBtn, &QPushButton::clicked, this, &DeviceManagerDialog::onRefreshAll);
    connect(addDevBtn,     &QPushButton::clicked, this, &DeviceManagerDialog::onAddDevice);
    connect(enterHomeBtn,  &QPushButton::clicked, this, [this]() {
        if (AppConfig::instance().currentDeviceIndex() < 0) {
            Toast::warning(this, "请先选择并点击「连接」设备");
            return;
        }
        accept();
    });
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
    m_scrollArea->setStyleSheet("QScrollArea{background:#13161c; border:none;}");

    m_cardsContainer = new QWidget;
    m_cardsContainer->setStyleSheet("background:#13161c;");
    m_cardsGrid = new QGridLayout(m_cardsContainer);
    m_cardsGrid->setContentsMargins(20, 20, 20, 20);
    m_cardsGrid->setSpacing(20);
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

    // ── 设备名称行 ──────────────────────────────────────────────────────────
    m_nameRow = new QWidget;
    auto *nrl = new QVBoxLayout(m_nameRow);
    nrl->setContentsMargins(0, 0, 0, 0);
    nrl->setSpacing(6);

    auto *nameLbl = new QLabel("\u8bbe\u5907\u540d\u79f0");
    nameLbl->setStyleSheet("color:#8898a8; font-size:12px;");
    nrl->addWidget(nameLbl);

    m_nameEdit = new QLineEdit;
    m_nameEdit->setPlaceholderText("例如: AGV-01 (选填，默认 AGV @ IP)");
    m_nameEdit->setFixedHeight(42);
    nrl->addWidget(m_nameEdit);

    fbl->addWidget(m_nameRow);
    // ────────────────────────────────────────────────────────────────────────

    m_addStatus = new QLabel;
    m_addStatus->setStyleSheet("color:#f0a060; font-size:12px;");
    m_addStatus->setWordWrap(true);
    m_addStatus->setMinimumHeight(18);
    fbl->addWidget(m_addStatus);

    m_confirmBtn = new QPushButton("添加设备");
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
        connect(card, &DeviceCard::doubleClicked,        this, &DeviceManagerDialog::onCardDoubleClicked);
        connect(card, &DeviceCard::connectClicked,       this, &DeviceManagerDialog::onConnectCard);
        connect(card, &DeviceCard::disconnectClicked,    this, &DeviceManagerDialog::onDisconnectCard);
        connect(card, &DeviceCard::removeClicked,        this, &DeviceManagerDialog::onRemoveCard);
        connect(card, &DeviceCard::networkConfigClicked, this, &DeviceManagerDialog::onNetworkConfig);
        m_cardsGrid->addWidget(card, i / cols, i % cols);
        m_cards.append(card);
    }

    fetchGlobalMapList([this]() {
        for (auto *c : m_cards) {
            if (c) c->updateCardDisplay();
        }
    });
}

void DeviceManagerDialog::onRefreshAll()
{
    refreshCards();
    setStatus("已刷新设备列表", "#5cb85c");
}

void DeviceManagerDialog::onAddDevice()
{
    m_ipEdit->clear();
    if (m_nameEdit) m_nameEdit->clear();
    m_addStatus->clear();
    m_confirmBtn->setEnabled(true);
    m_confirmBtn->setText("添加设备");
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

    QString name = m_nameEdit ? m_nameEdit->text().trimmed() : "";
    if (name.isEmpty()) {
        name = QString("AGV @ %1").arg(ip);
    }

    DeviceProfile dev;
    dev.ip            = ip;
    dev.carId         = "1";
    dev.name          = name;
    dev.lastConnected = QDateTime::currentDateTime();
    dev.linkStatus    = "0";
    AppConfig::instance().saveDevice(dev);
    AppConfig::instance().setServerIp(ip);
    AppConfig::instance().setAgvId(dev.carId);

    m_stack->setCurrentWidget(m_mainPanel);
    refreshCards();
    setStatus(QString("设备 [%1] 已添加").arg(dev.name), "#5cb85c");
    Toast::success(this, QString("设备 [%1] 已添加").arg(dev.name));
}

void DeviceManagerDialog::onCardDoubleClicked(int index)
{
    connectDevice(index);
}

void DeviceManagerDialog::onConnectCard(int index)
{
    const QList<DeviceProfile> list = AppConfig::instance().deviceList();
    if (index < 0 || index >= list.size()) return;

    // 1. 单车独占原则：一次只能连接一辆设备。若已有设备连接，主动断开旧设备
    int oldIdx = AppConfig::instance().currentDeviceIndex();
    if (oldIdx >= 0 && oldIdx != index) {
        if (oldIdx < m_cards.size() && m_cards[oldIdx]) {
            m_cards[oldIdx]->disconnectDevice();
        }
    }

    // 2. 更新全局活动设备配置 (IP 是区分设备的唯一标识)
    AppConfig::instance().setCurrentDeviceIndex(index);
    AppConfig::instance().setServerIp(list[index].ip);
    QString effectiveCarId = list[index].carId.trimmed().isEmpty() ? "1" : list[index].carId.trimmed();
    AppConfig::instance().setAgvId(effectiveCarId);
    RobotInfoManager::instance().resetForCar(effectiveCarId);
    RobotInfoManager::instance().init();

    // 3. 立即刷新所有卡片状态：目标设备高亮并显示“断开连接”，其他设备恢复未连接
    for (int i = 0; i < m_cards.size(); ++i) {
        if (!m_cards[i]) continue;
        if (i == index) {
            m_cards[i]->probeDeviceStatus(list[index].ip);
        } else {
            m_cards[i]->disconnectDevice();
        }
    }

    // 4. 发起独占 MQTT 连接 (connectToHost 内部会彻底销毁旧 client 并重启连接至新 IP，单次点击即完成无感切换)
    MqttService::instance().connectToHost(list[index].ip, 1883);

    QList<DeviceProfile> curList = AppConfig::instance().deviceList();
    if (index < curList.size()) {
        curList[index].lastConnected = QDateTime::currentDateTime();
        curList[index].linkStatus = "1";
        AppConfig::instance().saveDevice(curList[index]);
    }

    QString devName = list[index].name.isEmpty() ? list[index].ip : list[index].name;
    setStatus(QString("已连接设备 [%1] (%2)").arg(devName, list[index].ip), "#22c55e");
    Toast::success(this, QString("已连接设备 [%1]").arg(devName));
}

void DeviceManagerDialog::onDisconnectCard(int index)
{
    const QList<DeviceProfile> list = AppConfig::instance().deviceList();
    if (index < 0 || index >= list.size()) return;

    QString devName = list[index].name.isEmpty() ? list[index].ip : list[index].name;

    // 1. 重置当前活动设备索引为 -1，断开全局连接服务
    AppConfig::instance().setCurrentDeviceIndex(-1);
    MqttService::instance().disconnectFromHost();

    // 2. 刷新所有卡片，全部恢复未连接状态并显示“连接”
    for (auto *c : m_cards) {
        if (c) {
            c->disconnectDevice();
        }
    }

    // 3. 更新设备 linkStatus 状态并保存
    QList<DeviceProfile> curList = AppConfig::instance().deviceList();
    if (index < curList.size()) {
        curList[index].linkStatus = "0";
        AppConfig::instance().updateDevice(index, curList[index]);
    }

    setStatus(QString("设备 [%1] 已断开连接").arg(devName), "#e05555");
    Toast::info(this, QString("设备 [%1] 已断开连接").arg(devName));
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

    // 若尚未连接此设备，发起单车独占连接
    if (AppConfig::instance().currentDeviceIndex() != index || !MqttService::instance().isConnected()) {
        onConnectCard(index);
    }
    accept();
}

void DeviceManagerDialog::onNetworkConfig(int index)
{
    const QList<DeviceProfile> list = AppConfig::instance().deviceList();
    if (index < 0 || index >= list.size()) return;
    DeviceProfile profile = list[index];

    NetworkConfigDialog dlg(profile.ip,
                            profile.name.isEmpty() ? profile.carId : profile.name,
                            profile.sshUser,
                            profile.sshPwd,
                            profile.sshPort,
                            this);

    // 当 SSH 连接读取成功或保存生效时，立即记住每辆车专属的登录名与密码
    connect(&dlg, &NetworkConfigDialog::sshCredentialsUpdated, this, [index](const QString &user, const QString &pwd, int port) {
        QList<DeviceProfile> curList = AppConfig::instance().deviceList();
        if (index >= 0 && index < curList.size()) {
            curList[index].sshUser = user;
            curList[index].sshPwd  = pwd;
            curList[index].sshPort = port;
            AppConfig::instance().updateDevice(index, curList[index]);
        }
    });

    if (dlg.exec() == QDialog::Accepted) {
        profile.sshUser = dlg.sshUser();
        profile.sshPwd  = dlg.sshPwd();
        profile.sshPort = dlg.sshPort();

        if (dlg.isIpChanged()) {
            QString oldIp = profile.ip;
            QString newIp = dlg.newIp().trimmed();
            if (!newIp.isEmpty() && newIp != oldIp) {
                profile.ip = newIp;
                AppConfig::instance().updateDevice(index, profile);
                setStatus(QString("已更新设备 [%1] 的 IP 地址为 %2").arg(profile.name.isEmpty() ? profile.carId : profile.name, newIp), "#4caf7a");
                refreshCards();
                return;
            }
        }
        AppConfig::instance().updateDevice(index, profile);
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
