/**
 * @file    MapViewSwitchMapDialog.cpp
 * @layer   GUI
 * @brief   Card-based map management dialog implementation.
 * @deps    MapViewSwitchMapDialog.h, ApiService.h, MqttService.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapViewSwitchMapDialog.h"
#include "app/AppConfig.h"
#include "network/ApiService.h"
#include "network/MqttService.h"
#include "ui/common/LoadingDialog.h"
#include "ui/common/Toast.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QTimer>
#include <QDir>
#include <QDirIterator>
#include <QCoreApplication>
#include <QMessageBox>
#include <QMouseEvent>
#include <QJsonDocument>
#include <memory>

static bool hasCachedMap(const QString &dirPath)
{
    QDir dir(dirPath);
    if (!dir.exists()) return false;
    return QDirIterator(dirPath, QStringList() << "*.yaml" << "*.yml", QDir::Files, QDirIterator::Subdirectories).hasNext() &&
           QDirIterator(dirPath, QStringList() << "*.pgm", QDir::Files, QDirIterator::Subdirectories).hasNext();
}

static QString jsonId(const QJsonValue &v) { return v.isDouble() ? QString::number((qint64)v.toDouble()) : v.toString().trimmed(); }

MapViewSwitchMapDialog::MapViewSwitchMapDialog(const QString &currentMapId, QWidget *parent)
    : QDialog(parent), m_currentMapId(currentMapId)
{
    setupUI();
    onRefreshList();
}

void MapViewSwitchMapDialog::setupUI()
{
    setWindowTitle("选择地图");
    setMinimumSize(840, 580);
    setStyleSheet("QDialog{background:#181b22; color:#e2e8f0;} QScrollArea{background:#181b22; border:none;}"
                  "QWidget#cards{background:#181b22;} QLabel{background:transparent;}"
                  "QPushButton{background:#232936; color:#cbd5e1; border:1px solid #3b4252; border-radius:4px; padding:6px 14px; font-size:12px; font-weight:500;}"
                  "QPushButton:hover{background:#2d3545; border-color:#4a9eff; color:#fff;}");

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16); root->setSpacing(12);

    auto *topRow = new QHBoxLayout;
    topRow->setSpacing(10);
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("🔍 搜索地图名称或地图ID...");
    m_searchEdit->setStyleSheet("background:#232936; border:1px solid #3b4252; border-radius:6px; color:#f8fafc; padding:7px 12px; font-size:13px;");
    m_statsLabel = new QLabel("共 0 张地图", this);
    m_statsLabel->setStyleSheet("color:#94a3b8; font-size:12px; font-weight:500;");
    auto *refreshBtn = new QPushButton("🔄 刷新列表", this);
    topRow->addWidget(m_searchEdit, 1); topRow->addWidget(m_statsLabel); topRow->addWidget(refreshBtn);
    root->addLayout(topRow);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
    auto *cardsWidget = new QWidget;
    cardsWidget->setObjectName("cards");
    m_grid = new QGridLayout(cardsWidget);
    m_grid->setContentsMargins(4, 4, 4, 4); m_grid->setSpacing(10);
    m_grid->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    scroll->setWidget(cardsWidget); root->addWidget(scroll, 1);

    auto *bottomRow = new QHBoxLayout;
    bottomRow->setSpacing(10);
    m_selTipLabel = new QLabel("请在上方选择地图 (支持双击快速切换)", this);
    m_selTipLabel->setStyleSheet("color:#94a3b8; font-size:12px;");
    m_cancelBtn = new QPushButton("取消", this);
    m_okBtn     = new QPushButton("确认切换", this);
    m_okBtn->setStyleSheet("QPushButton{background:#2563eb; color:#fff; font-weight:600; border:none; border-radius:4px; padding:6px 18px;} QPushButton:hover{background:#3b82f6;}");
    bottomRow->addWidget(m_selTipLabel, 1); bottomRow->addWidget(m_cancelBtn); bottomRow->addWidget(m_okBtn);
    root->addLayout(bottomRow);

    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_okBtn, &QPushButton::clicked, this, &MapViewSwitchMapDialog::onConfirmSwitch);
    connect(refreshBtn, &QPushButton::clicked, this, &MapViewSwitchMapDialog::onRefreshList);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &MapViewSwitchMapDialog::onFilterChanged);
}

void MapViewSwitchMapDialog::onRefreshList()
{
    ApiService::instance().listMapAll(QJsonObject{{"pageNum",1},{"pageSize",100}}, [this](int code, const QJsonObject &data) {
        if (code != 200) { if (m_statsLabel) m_statsLabel->setText("获取地图列表失败"); return; }
        m_mapList = data.value("rows").toArray();
        buildCards(m_mapList, m_searchEdit ? m_searchEdit->text() : QString());
    });
}

void MapViewSwitchMapDialog::onFilterChanged(const QString &kw)
{
    buildCards(m_mapList, kw);
}

void MapViewSwitchMapDialog::buildCards(const QJsonArray &list, const QString &filter)
{
    for (auto *c : m_allCards) c->deleteLater();
    m_allCards.clear();
    while (auto *item = m_grid->takeAt(0)) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    int visibleCount = 0, col = 0, row = 0;
    const int COLS = 2;
    for (const auto &v : list) {
        QJsonObject m = v.toObject();
        QString name = m.value("mapName").toString();
        QString mid  = jsonId(m.value("mapId"));
        if (!filter.isEmpty() && !name.contains(filter, Qt::CaseInsensitive) && !mid.contains(filter.trimmed())) continue;

        visibleCount++;
        bool isCached  = hasCachedMap(QCoreApplication::applicationDirPath() + "/maps/" + mid);
        bool isCurrent = (mid == m_currentMapId);
        bool isSel     = (mid == m_selectedMapId);

        auto *card = createCard(m, mid, name, isCurrent, isCached, isSel);
        m_grid->addWidget(card, row, col);
        if (++col >= COLS) { col = 0; ++row; }
    }
    m_statsLabel->setText(QString("共 %1 张地图%2").arg(list.size()).arg(filter.isEmpty() ? "" : QString(" (已筛选: %1)").arg(visibleCount)));
}

QFrame* MapViewSwitchMapDialog::createCard(const QJsonObject &, const QString &mid, const QString &name,
                                          bool isCurrent, bool isCached, bool isSelected)
{
    auto *card = new QFrame;
    card->setMinimumHeight(68);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    card->setStyleSheet(isSelected ? "QFrame{background:#183354; border:2px solid #3b82f6; border-radius:6px;}"
                        : (isCurrent ? "QFrame{background:#1c2738; border:1px solid #2d456b; border-radius:6px;} QFrame:hover{background:#23334a; border-color:#3b82f6;}"
                                     : "QFrame{background:#1e2430; border:1px solid #2d3545; border-radius:6px;} QFrame:hover{background:#252e3d; border-color:#3b82f6;}"));
    m_allCards.append(card);

    auto *hl = new QHBoxLayout(card);
    hl->setContentsMargins(12, 10, 12, 10); hl->setSpacing(12);

    auto *iconBox = new QFrame;
    iconBox->setFixedSize(40, 40);
    iconBox->setStyleSheet("background:#141b2d; border:1px solid #1e3a8a; border-radius:6px;");
    auto *iconLyt = new QVBoxLayout(iconBox);
    iconLyt->setContentsMargins(0, 0, 0, 0);
    auto *iconLbl = new QLabel("MAP", iconBox);
    iconLbl->setAlignment(Qt::AlignCenter);
    iconLbl->setStyleSheet("color:#38bdf8; font-weight:bold; font-size:11px;");
    iconLyt->addWidget(iconLbl);
    hl->addWidget(iconBox);

    auto *infoLyt = new QVBoxLayout;
    infoLyt->setContentsMargins(0, 0, 0, 0); infoLyt->setSpacing(4);
    auto *nameLbl = new QLabel(name);
    nameLbl->setStyleSheet("color:#f8fafc; font-size:13px; font-weight:bold;");
    infoLyt->addWidget(nameLbl);

    auto *tagRow = new QHBoxLayout;
    tagRow->setContentsMargins(0, 0, 0, 0); tagRow->setSpacing(6);
    auto mkBadge = [](const QString &t, const QString &ss) {
        auto *b = new QLabel(t); b->setStyleSheet(ss); return b;
    };
    tagRow->addWidget(mkBadge(QString("ID: %1").arg(mid), "background:#0f172a; color:#38bdf8; border:1px solid #0284c7; border-radius:3px; padding:1px 6px; font-size:11px; font-family:monospace; font-weight:600;"));
    tagRow->addWidget(mkBadge(isCached ? "✓ 本地已缓存" : "云端资源包", isCached ? "background:rgba(16,185,129,0.18); color:#34d399; border:1px solid rgba(16,185,129,0.4); border-radius:3px; padding:1px 6px; font-size:11px;" : "background:rgba(148,163,184,0.12); color:#94a3b8; border:1px solid rgba(148,163,184,0.3); border-radius:3px; padding:1px 6px; font-size:11px;"));
    if (isCurrent) tagRow->addWidget(mkBadge("当前使用中", "background:rgba(59,130,246,0.25); color:#60a5fa; border:1px solid #3b82f6; border-radius:3px; padding:1px 6px; font-size:11px; font-weight:bold;"));
    tagRow->addStretch();
    infoLyt->addLayout(tagRow);
    hl->addLayout(infoLyt, 1);

    auto *delBtn = new QPushButton("✕", card);
    delBtn->setFixedSize(24, 24); delBtn->setCursor(Qt::PointingHandCursor); delBtn->setToolTip("删除此地图");
    delBtn->setStyleSheet("QPushButton{background:rgba(220,38,38,0.18); color:#f87171; border:1px solid rgba(220,38,38,0.3); border-radius:12px; font-size:11px; font-weight:bold; padding:0;} QPushButton:hover{background:#ef4444; color:#fff; border:none;}");
    hl->addWidget(delBtn);

    auto doSelect = [this, card, mid, name, isCached]() {
        m_selectedMapId = mid; m_selectedMapName = name;
        for (auto *c : m_allCards) c->setStyleSheet("QFrame{background:#1e2430; border:1px solid #2d3545; border-radius:6px;} QFrame:hover{background:#252e3d; border-color:#3b82f6;}");
        card->setStyleSheet("QFrame{background:#183354; border:2px solid #3b82f6; border-radius:6px;}");
        m_selTipLabel->setText(QString("已选中地图: [ID: %1] %2 (%3)").arg(mid, name, isCached ? "本地已缓存" : "需下载资源包"));
        m_selTipLabel->setStyleSheet("color:#60a5fa; font-size:12px; font-weight:500;");
    };

    struct Filter : public QObject {
        std::function<void()> onClick, onDbl;
        Filter(std::function<void()> c, std::function<void()> d, QObject *p) : QObject(p), onClick(c), onDbl(d) {}
        bool eventFilter(QObject *o, QEvent *e) override {
            if (e->type() == QEvent::MouseButtonPress) { if (onClick) onClick(); }
            else if (e->type() == QEvent::MouseButtonDblClick) { if (onDbl) onDbl(); return true; }
            return QObject::eventFilter(o, e);
        }
    };
    card->installEventFilter(new Filter(doSelect, [doSelect, this]() { doSelect(); onConfirmSwitch(); }, card));

    connect(delBtn, &QPushButton::clicked, this, [this, mid, name]() {
        QMessageBox confirm(this);
        confirm.setWindowTitle("确认删除");
        confirm.setText(QString("确认删除地图「%1」(ID: %2)？\n此操作不可撤销。").arg(name, mid));
        QPushButton *yesBtn = confirm.addButton("确定", QMessageBox::YesRole);
        confirm.addButton("取消", QMessageBox::NoRole);
        confirm.exec();
        if (confirm.clickedButton() != yesBtn) return;
        ApiService::instance().delMap(mid, [this](int code, const QJsonObject &) {
            if (code != 200) { QMessageBox::warning(this, "错误", "删除地图失败"); return; }
            Toast::success(this, "地图已删除");
            onRefreshList();
        });
    });

    return card;
}

void MapViewSwitchMapDialog::onConfirmSwitch()
{
    if (m_selectedMapId.isEmpty()) { Toast::info(this, "请先选择需要切换的地图"); return; }
    m_okBtn->setEnabled(false); m_cancelBtn->setEnabled(false);
    m_okBtn->setText("正在切换...");
    m_selTipLabel->setText(QString("正在请求切换至地图 [ID: %1]...").arg(m_selectedMapId));
    m_selTipLabel->setStyleSheet("color:#38bdf8; font-size:12px;");

    auto *loading = new LoadingDialog("正在切换地图，等待设备确认...", this, Qt::ApplicationModal, 40000);
    loading->show();

    QString carId = AppConfig::instance().agvId().isEmpty() ? "1" : AppConfig::instance().agvId();
    QStringList topics = { QString("/rob/%1/crud").arg(carId) };
    if (carId != "1") topics << QStringLiteral("/rob/1/crud");

    auto handled = std::make_shared<bool>(false);
    auto *timer = new QTimer(this);
    timer->setSingleShot(true);

    auto cleanup = [handled, loading, topics, timer]() {
        if (*handled) return;
        *handled = true;
        if (timer) timer->stop();
        for (const QString &t : topics) MqttService::instance().unsubscribeTopic(t);
        if (loading) loading->finish();
    };
    connect(this, &QDialog::finished, this, [cleanup](int) { cleanup(); });

    auto onCrud = [this, handled, cleanup](const QString &, const QJsonObject &obj) {
        if (*handled || obj.value("sender").toString().trimmed().compare("Robot", Qt::CaseInsensitive) != 0) return;
        int mId = obj.value("map_id").toInt();
        if (mId > 0 && mId != m_selectedMapId.toInt()) return;

        QString status = obj.value("status").toString().trimmed();
        QString info = obj.value("info").toString().trimmed();
        cleanup();

        if (status.compare("success", Qt::CaseInsensitive) == 0) {
            m_switchSuccessMsg = info;
            accept();
        } else {
            m_okBtn->setEnabled(true); m_cancelBtn->setEnabled(true);
            m_okBtn->setText("确认切换");
            QString err = info.isEmpty() ? QString("设备切换地图失败 (%1)").arg(status) : info;
            m_selTipLabel->setText(QString("<font color='#f87171'>切换失败: %1</font>").arg(err));
            QMessageBox::warning(this, "切换地图失败", err);
        }
    };
    for (const QString &t : topics) MqttService::instance().subscribeJsonTopic(t, 0, onCrud);

    connect(timer, &QTimer::timeout, this, [this, handled, cleanup]() {
        if (*handled) return;
        cleanup();
        m_okBtn->setEnabled(true); m_cancelBtn->setEnabled(true); m_okBtn->setText("确认切换");
        m_selTipLabel->setText("<font color='#f87171'>切换失败: 等待设备确认超时</font>");
        QMessageBox::warning(this, "切换超时", "等待设备切换地图响应超时 (30秒)，请确认设备在线且网络畅通。");
    });
    timer->start(30000);

    ApiService::instance().editCarMap(m_selectedMapId, [this, loading, handled, cleanup](int code, const QJsonObject &data) {
        if (*handled) return;
        if (code == 200) {
            if (loading) loading->setMessage("指令已下发，正在等待设备切换与导航重启...");
        } else {
            cleanup();
            m_okBtn->setEnabled(true); m_cancelBtn->setEnabled(true); m_okBtn->setText("确认切换");
            QString err = data.value("msg").toString(QString("错误码 %1").arg(code));
            m_selTipLabel->setText(QString("<font color='#f87171'>切换失败: %1</font>").arg(err));
            QMessageBox::warning(this, "切换失败", QString("访问切换地图接口失败:\n%1").arg(err));
        }
    });
}
