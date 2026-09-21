#include "AudioManagerDialog.h"
#include "core/ApiService.h"
#include "core/Toast.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QToolButton>
#include <QPainter>
#include <QPolygonF>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QJsonArray>
#include <QJsonValue>
#include <QMediaPlayer>
#include <QAudioOutput>

namespace {

// 列表项样式
const char *kListStyle =
    "QListWidget {"
    "  background: #0b1220;"
    "  border: 1px solid #2b3648;"
    "  border-radius: 3px;"
    "  color: #cbd5e1;"
    "  font-size: 12px;"
    "  outline: none;"
    "  padding: 4px;"
    "}"
    "QListWidget::item {"
    "  height: 27px;"
    "  padding-left: 6px;"
    "  border-radius: 2px;"
    "}"
    "QListWidget::item:hover {"
    "  background: #16233a;"
    "}"
    "QListWidget::item:selected {"
    "  background: #1d4ed8;"
    "  color: #ffffff;"
    "  border-left: 3px solid #60a5fa;"
    "}"
    "QScrollBar:vertical {"
    "  background: #0b1220;"
    "  width: 8px;"
    "  margin: 0px;"
    "}"
    "QScrollBar::handle:vertical {"
    "  background: #334155;"
    "  border-radius: 4px;"
    "  min-height: 24px;"
    "}"
    "QScrollBar::handle:vertical:hover {"
    "  background: #475569;"
    "}"
    "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
    "  height: 0px;"
    "}";

// 普通小按钮
const char *kButtonStyle =
    "QPushButton {"
    "  background: #1f2937;"
    "  color: #dbe4ee;"
    "  border: 1px solid #334155;"
    "  border-radius: 3px;"
    "  padding: 4px 14px;"
    "  font-size: 12px;"
    "}"
    "QPushButton:hover {"
    "  background: #26344a;"
    "  border-color: #3b82f6;"
    "}"
    "QPushButton:pressed {"
    "  background: #172033;"
    "}"
    "QPushButton:disabled {"
    "  color: #5b6675;"
    "  border-color: #243044;"
    "  background: #161d2a;"
    "}";

// 超链接风格按钮
const char *kLinkButtonStyle =
    "QPushButton {"
    "  background: transparent;"
    "  color: #4da3ff;"
    "  border: none;"
    "  padding: 4px 6px;"
    "  font-size: 12px;"
    "}"
    "QPushButton:hover {"
    "  color: #8ec5ff;"
    "}";

// 图标小按钮 (刷新/删除)
const char *kIconButtonStyle =
    "QToolButton {"
    "  background: transparent;"
    "  border: none;"
    "  border-radius: 3px;"
    "}"
    "QToolButton:hover {"
    "  background: #22304a;"
    "}";

// 获取音频列表图标：紫色环形箭头
QIcon makeRefreshIcon()
{
    const int sz = 20;
    QPixmap pm(sz, sz);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    const QColor col("#a855f7");
    const double c = sz * 0.5;
    QRectF arc(c - sz * 0.32, c - sz * 0.32, sz * 0.64, sz * 0.64);
    p.setPen(QPen(col, 1.8, Qt::SolidLine, Qt::RoundCap));
    p.setBrush(Qt::NoBrush);
    p.drawArc(arc, 70 * 16, 250 * 16);

    p.setPen(Qt::NoPen);
    p.setBrush(col);
    QPolygonF tri;
    tri << QPointF(c + sz * 0.32, c - sz * 0.06)
        << QPointF(c + sz * 0.32, c + sz * 0.20)
        << QPointF(c + sz * 0.08, c + sz * 0.10);
    p.drawPolygon(tri);
    p.end();
    return QIcon(pm);
}

// 删除图标：垃圾桶
QIcon makeTrashIcon()
{
    const int sz = 20;
    QPixmap pm(sz, sz);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    const QColor col("#9aa1ad");
    p.setPen(QPen(col, 1.4, Qt::SolidLine, Qt::RoundCap));
    p.setBrush(Qt::NoBrush);
    // 桶盖与提手
    p.drawLine(QPointF(sz * 0.18, sz * 0.28), QPointF(sz * 0.82, sz * 0.28));
    p.drawLine(QPointF(sz * 0.38, sz * 0.28), QPointF(sz * 0.42, sz * 0.16));
    p.drawLine(QPointF(sz * 0.42, sz * 0.16), QPointF(sz * 0.58, sz * 0.16));
    p.drawLine(QPointF(sz * 0.58, sz * 0.16), QPointF(sz * 0.62, sz * 0.28));
    // 桶身
    p.drawRoundedRect(QRectF(sz * 0.26, sz * 0.28, sz * 0.48, sz * 0.54), 2, 2);
    // 桶身竖纹
    p.drawLine(QPointF(sz * 0.40, sz * 0.38), QPointF(sz * 0.40, sz * 0.72));
    p.drawLine(QPointF(sz * 0.50, sz * 0.38), QPointF(sz * 0.50, sz * 0.72));
    p.drawLine(QPointF(sz * 0.60, sz * 0.38), QPointF(sz * 0.60, sz * 0.72));
    p.end();
    return QIcon(pm);
}

} // namespace

AudioManagerDialog::AudioManagerDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("音频管理器");
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);
    setFixedSize(560, 430);

    m_player = new QMediaPlayer(this);
    m_audioOut = new QAudioOutput(this);
    m_audioOut->setVolume(0.8);
    m_player->setAudioOutput(m_audioOut);

    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus st) {
        if (st == QMediaPlayer::EndOfMedia) {
            m_statusLabel->setText("播放结束");
            setPlayState(PlayState::Stopped);
        } else if (st == QMediaPlayer::InvalidMedia) {
            m_statusLabel->setText(QString("音频无法播放: %1").arg(m_playingName));
            Toast::error(this, "音频无法播放，请确认文件格式为.wav");
            setPlayState(PlayState::Stopped);
        }
    });
    connect(m_player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString &err) {
        m_statusLabel->setText(QString("音频播放失败: %1").arg(m_playingName));
        Toast::error(this, QString("音频播放失败: %1").arg(err));
        setPlayState(PlayState::Stopped);
    });

    setupUi();
}

AudioManagerDialog::~AudioManagerDialog()
{
    if (m_player) {
        m_player->stop();
    }
}

void AudioManagerDialog::setupUi()
{
    setStyleSheet(
        "QDialog {"
        "  background: #111827;"
        "  color: #e5e7eb;"
        "}"
        "QLabel { background: transparent; }"
    );
    setStyleSheet(styleSheet() + kListStyle);

    auto *rootLay = new QVBoxLayout(this);
    rootLay->setContentsMargins(10, 8, 10, 10);
    rootLay->setSpacing(8);

    // ── 1. 状态提示 ──────────────────────────────────────────────────────────
    m_statusLabel = new QLabel("获取音频列表完成", this);
    m_statusLabel->setStyleSheet("color:#9aa1ad; font-size:12px;");
    rootLay->addWidget(m_statusLabel);

    // ── 2. 格式说明 ──────────────────────────────────────────────────────────
    auto *noteLabel = new QLabel(
        "<span style='color:#e879f9; font-weight:bold;'>注:</span>"
        "<span style='color:#e5e7eb;'> 仅支持.wav格式音频文件.</span>", this);
    noteLabel->setTextFormat(Qt::RichText);
    noteLabel->setStyleSheet("font-size:12px;");
    rootLay->addWidget(noteLabel);

    // ── 3. 列表标题行 (刷新 / 打开目录 / 上传 / 下载 / 删除) ──────────────────
    auto *headRow = new QHBoxLayout;
    headRow->setSpacing(6);

    auto *listTitle = new QLabel("音频列表:", this);
    listTitle->setStyleSheet("color:#e5e7eb; font-size:12px;");
    headRow->addWidget(listTitle);

    auto *refreshBtn = new QToolButton(this);
    refreshBtn->setIcon(makeRefreshIcon());
    refreshBtn->setIconSize(QSize(20, 20));
    refreshBtn->setFixedSize(24, 24);
    refreshBtn->setCursor(Qt::PointingHandCursor);
    refreshBtn->setStyleSheet(kIconButtonStyle);
    refreshBtn->setToolTip("刷新音频列表");
    connect(refreshBtn, &QToolButton::clicked, this, &AudioManagerDialog::onRefreshList);
    headRow->addWidget(refreshBtn);

    headRow->addStretch();

    auto *openDirBtn = new QPushButton("打开文件目录", this);
    openDirBtn->setCursor(Qt::PointingHandCursor);
    openDirBtn->setStyleSheet(kLinkButtonStyle);
    {
        QFont f = openDirBtn->font();
        f.setUnderline(true);
        openDirBtn->setFont(f);
    }
    connect(openDirBtn, &QPushButton::clicked, this, &AudioManagerDialog::onOpenDirectory);
    headRow->addWidget(openDirBtn);

    auto *uploadBtn = new QPushButton("上传", this);
    uploadBtn->setCursor(Qt::PointingHandCursor);
    uploadBtn->setStyleSheet(kButtonStyle);
    uploadBtn->setToolTip("上传本地.wav音频文件");
    connect(uploadBtn, &QPushButton::clicked, this, &AudioManagerDialog::onUpload);
    headRow->addWidget(uploadBtn);

    auto *downloadBtn = new QPushButton("下载", this);
    downloadBtn->setCursor(Qt::PointingHandCursor);
    downloadBtn->setStyleSheet(kButtonStyle);
    downloadBtn->setToolTip("下载选中音频到 mediaFile 目录");
    connect(downloadBtn, &QPushButton::clicked, this, &AudioManagerDialog::onDownload);
    headRow->addWidget(downloadBtn);

    m_deleteBtn = new QPushButton(this);
    m_deleteBtn->setIcon(makeTrashIcon());
    m_deleteBtn->setIconSize(QSize(20, 20));
    m_deleteBtn->setCursor(Qt::PointingHandCursor);
    m_deleteBtn->setStyleSheet(kButtonStyle + QString("QPushButton { padding: 4px 8px; }"));
    m_deleteBtn->setToolTip("删除选中音频");
    connect(m_deleteBtn, &QPushButton::clicked, this, &AudioManagerDialog::onDeleteSelected);
    headRow->addWidget(m_deleteBtn);

    rootLay->addLayout(headRow);

    // ── 4. 音频列表 ──────────────────────────────────────────────────────────
    m_listWidget = new QListWidget(this);
    connect(m_listWidget, &QListWidget::itemSelectionChanged,
            this, &AudioManagerDialog::updateControlStates);
    connect(m_listWidget, &QListWidget::itemDoubleClicked,
            this, [this](QListWidgetItem *) { onPlay(); });
    rootLay->addWidget(m_listWidget, 1);

    // ── 5. 底部播放控制 ──────────────────────────────────────────────────────
    auto *bottomRow = new QHBoxLayout;
    bottomRow->setSpacing(8);
    bottomRow->addStretch();

    auto makeCtrlBtn = [&](const QString &text) {
        auto *b = new QPushButton(text, this);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(kButtonStyle);
        bottomRow->addWidget(b);
        return b;
    };
    m_playBtn     = makeCtrlBtn("播放");
    m_pauseBtn    = makeCtrlBtn("暂停");
    m_continueBtn = makeCtrlBtn("继续");
    m_stopBtn     = makeCtrlBtn("停止");

    connect(m_playBtn,     &QPushButton::clicked, this, &AudioManagerDialog::onPlay);
    connect(m_pauseBtn,    &QPushButton::clicked, this, &AudioManagerDialog::onPause);
    connect(m_continueBtn, &QPushButton::clicked, this, &AudioManagerDialog::onContinue);
    connect(m_stopBtn,     &QPushButton::clicked, this, &AudioManagerDialog::onStop);

    rootLay->addLayout(bottomRow);

    loadAudioList();
}

// ── 本地缓存目录 ────────────────────────────────────────────────────────────

QString AudioManagerDialog::mediaDir()
{
    const QString dir = QCoreApplication::applicationDirPath() + "/mediaFile";
    QDir().mkpath(dir);
    return dir;
}

QString AudioManagerDialog::cachedPath(const QString &name)
{
    return mediaDir() + "/" + QFileInfo(name).fileName();
}

QString AudioManagerDialog::currentAudioName() const
{
    auto *item = m_listWidget->currentItem();
    return item ? item->text() : QString();
}

// ── 列表 ────────────────────────────────────────────────────────────────────

void AudioManagerDialog::loadAudioList()
{
    m_statusLabel->setText("正在获取音频列表...");
    ApiService::instance().listAudio([this](int code, const QJsonObject &resp) {
        if (code != 200) {
            m_listWidget->clear();
            m_statusLabel->setText(QString("获取音频列表失败 (code=%1)").arg(code));
            Toast::error(this, QString("获取音频列表失败: %1")
                .arg(resp.value("msg").toString(QStringLiteral("请求失败"))));
            updateControlStates();
            return;
        }

        m_listWidget->clear();
        const QJsonArray rows = resp.value("data").toArray();
        for (const QJsonValue &v : rows) {
            // 兼容服务端返回完整路径的情况，只取文件名
            const QString name = QFileInfo(v.toString().trimmed()).fileName();
            if (!name.isEmpty()) m_listWidget->addItem(name);
        }
        m_statusLabel->setText("获取音频列表完成");
        updateControlStates();
    });
}

void AudioManagerDialog::updateControlStates()
{
    const bool hasSel = (m_listWidget->currentItem() != nullptr);
    if (m_deleteBtn)   m_deleteBtn->setEnabled(hasSel);
    if (m_playBtn)     m_playBtn->setEnabled(hasSel);
    if (m_pauseBtn)    m_pauseBtn->setEnabled(hasSel && m_playState == PlayState::Playing);
    if (m_continueBtn) m_continueBtn->setEnabled(hasSel && m_playState == PlayState::Paused);
    if (m_stopBtn)     m_stopBtn->setEnabled(hasSel && m_playState != PlayState::Stopped);
}

void AudioManagerDialog::setPlayState(PlayState state)
{
    m_playState = state;
    updateControlStates();
}

// ── 按钮响应 ────────────────────────────────────────────────────────────────

void AudioManagerDialog::onRefreshList()
{
    // 列表刷新后原选中项可能已不存在，先停止当前播放
    stopPlayback();
    loadAudioList();
}

void AudioManagerDialog::onUpload()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择要上传的音频文件"), QString(),
        QStringLiteral("音频文件 (*.wav)"));
    if (filePath.isEmpty()) return;

    m_statusLabel->setText(QString("正在上传: %1").arg(QFileInfo(filePath).fileName()));
    ApiService::instance().uploadAudio(filePath, [this](int code, const QJsonObject &resp) {
        if (code == 200) {
            Toast::success(this, "音频上传成功");
            loadAudioList();
        } else {
            m_statusLabel->setText(QString("音频上传失败 (code=%1)").arg(code));
            Toast::error(this, QString("音频上传失败: %1")
                .arg(resp.value("msg").toString(QStringLiteral("请求失败"))));
        }
    });
}

void AudioManagerDialog::onDownload()
{
    const QString name = currentAudioName();
    if (name.isEmpty()) {
        Toast::info(this, "请先选择要下载的音频");
        return;
    }

    m_statusLabel->setText(QString("正在下载: %1").arg(name));
    ApiService::instance().downloadAudio(name, [this, name](bool ok, const QByteArray &data) {
        if (!ok || data.isEmpty()) {
            m_statusLabel->setText(QString("音频下载失败: %1").arg(name));
            Toast::error(this, QString("音频下载失败: %1").arg(name));
            return;
        }
        const QString path = cachedPath(name);
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) {
            m_statusLabel->setText("音频本地写入失败");
            Toast::error(this, "无法写入本地缓存目录 mediaFile");
            return;
        }
        f.write(data);
        f.close();
        m_statusLabel->setText(QString("已下载: %1").arg(name));
        Toast::success(this, QString("已下载到 mediaFile: %1").arg(name));
    });
}

void AudioManagerDialog::onDeleteSelected()
{
    const QString name = currentAudioName();
    if (name.isEmpty()) {
        Toast::info(this, "请先选择要删除的音频");
        return;
    }

    m_statusLabel->setText(QString("正在删除: %1").arg(name));
    ApiService::instance().deleteAudio(name, [this, name](int code, const QJsonObject &resp) {
        if (code == 200) {
            // 服务端删除成功，同步清理本地缓存文件
            const QString path = cachedPath(name);
            if (QFile::exists(path)) QFile::remove(path);

            if (m_playingName == name) stopPlayback();
            m_statusLabel->setText(QString("已删除: %1").arg(name));
            Toast::success(this, resp.value("msg").toString(QStringLiteral("删除成功")));
            loadAudioList();
        } else {
            m_statusLabel->setText(QString("删除失败 (code=%1)").arg(code));
            Toast::error(this, QString("删除失败: %1")
                .arg(resp.value("msg").toString(QStringLiteral("请求失败"))));
        }
    });
}

void AudioManagerDialog::onOpenDirectory()
{
    const QString dir = mediaDir();
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(dir))) {
        Toast::error(this, QString("打开目录失败: %1").arg(QDir::toNativeSeparators(dir)));
    } else {
        m_statusLabel->setText(QString("音频缓存目录: %1").arg(QDir::toNativeSeparators(dir)));
    }
}

void AudioManagerDialog::onPlay()
{
    const QString name = currentAudioName();
    if (name.isEmpty()) {
        Toast::info(this, "请先选择要播放的音频");
        return;
    }

    // 优先播放本地缓存；未缓存则先从服务端拉取再播放
    const QString path = cachedPath(name);
    if (QFile::exists(path)) {
        playLocal(name, path);
        return;
    }

    m_statusLabel->setText(QString("正在获取音频: %1").arg(name));
    ApiService::instance().downloadAudio(name, [this, name](bool ok, const QByteArray &data) {
        if (!ok || data.isEmpty()) {
            m_statusLabel->setText(QString("音频获取失败: %1").arg(name));
            Toast::error(this, QString("音频获取失败: %1").arg(name));
            return;
        }
        const QString path = cachedPath(name);
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) {
            m_statusLabel->setText("音频本地写入失败");
            Toast::error(this, "无法写入本地缓存目录 mediaFile");
            return;
        }
        f.write(data);
        f.close();
        playLocal(name, path);
    });
}

void AudioManagerDialog::playLocal(const QString &name, const QString &filePath)
{
    m_playingName = name;
    m_player->setSource(QUrl::fromLocalFile(filePath));
    m_player->play();
    setPlayState(PlayState::Playing);
    m_statusLabel->setText(QString("正在播放: %1").arg(name));
}

void AudioManagerDialog::stopPlayback()
{
    if (m_player) m_player->stop();
    m_playingName.clear();
    setPlayState(PlayState::Stopped);
}

void AudioManagerDialog::onPause()
{
    if (m_playState != PlayState::Playing) return;
    m_player->pause();
    setPlayState(PlayState::Paused);
    m_statusLabel->setText(QString("已暂停: %1").arg(m_playingName));
}

void AudioManagerDialog::onContinue()
{
    if (m_playState != PlayState::Paused) return;
    m_player->play();
    setPlayState(PlayState::Playing);
    m_statusLabel->setText(QString("继续播放: %1").arg(m_playingName));
}

void AudioManagerDialog::onStop()
{
    if (m_playState == PlayState::Stopped) return;
    stopPlayback();
    m_statusLabel->setText("已停止播放");
}