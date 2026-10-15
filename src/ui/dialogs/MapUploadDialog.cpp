#include "MapUploadDialog.h"
#include "network/ApiService.h"
#include "ui/common/LoadingDialog.h"
#include "ui/common/Toast.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QGroupBox>

namespace {

const char *kDialogStyle = R"(
QDialog {
    background-color: #1a1d23;
    color: #c8d0d8;
    font-family: "Microsoft YaHei UI", "Segoe UI", sans-serif;
    font-size: 13px;
}
QGroupBox {
    color: #4a9eff;
    font-weight: bold;
    border: 1px solid #2d3340;
    border-radius: 6px;
    margin-top: 10px;
    padding-top: 14px;
    background-color: #1f232b;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 12px;
    padding: 0 4px;
    background-color: #1f232b;
}
QLabel {
    color: #a0aec0;
    font-size: 12px;
}
QLineEdit {
    background-color: #14171c;
    border: 1px solid #363d4d;
    border-radius: 4px;
    color: #ffffff;
    padding: 5px 8px;
    font-size: 12px;
}
QLineEdit:focus {
    border: 1px solid #4a9eff;
}
QLineEdit:read-only {
    background-color: #181b21;
    color: #8fa0b5;
}
QPushButton {
    background-color: #252a35;
    border: 1px solid #3a3f4a;
    border-radius: 4px;
    color: #c8d0d8;
    padding: 5px 12px;
    font-size: 12px;
}
QPushButton:hover {
    background-color: #2d3340;
    border-color: #4a9eff;
    color: #ffffff;
}
QPushButton#primaryBtn {
    background-color: #1e4080;
    border: 1px solid #4a9eff;
    color: #ffffff;
    font-weight: bold;
}
QPushButton#primaryBtn:hover {
    background-color: #2550a0;
}
)";

} // namespace

MapUploadDialog::MapUploadDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("地图上传 (PGM / YAML / PBStream)");
    setMinimumWidth(560);
    setStyleSheet(kDialogStyle);
    setupUI();
}

void MapUploadDialog::setupUI()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(18, 16, 18, 16);
    rootLayout->setSpacing(14);

    // ── 基本信息与必选文件 ──────────────────────────────────────────
    auto *requiredGroup = new QGroupBox("地图必选信息", this);
    auto *reqForm = new QFormLayout(requiredGroup);
    reqForm->setContentsMargins(14, 16, 14, 14);
    reqForm->setSpacing(10);
    reqForm->setLabelAlignment(Qt::AlignRight);

    // 地图名称
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText("请输入地图名称（必填，例如：测试地图）");
    reqForm->addRow("<font color='#e05555'>*</font> 地图名称:", m_nameEdit);

    // PGM 文件
    auto makeFileRow = [this](QLineEdit *&edit, const QString &hint, const char *slot, QPushButton *&browseBtn) {
        auto *row = new QHBoxLayout;
        row->setSpacing(6);
        edit = new QLineEdit(this);
        edit->setReadOnly(true);
        edit->setPlaceholderText(hint);
        browseBtn = new QPushButton("浏览...", this);
        browseBtn->setCursor(Qt::PointingHandCursor);
        connect(browseBtn, SIGNAL(clicked()), this, slot);
        row->addWidget(edit, 1);
        row->addWidget(browseBtn);
        return row;
    };

    QPushButton *btnPgm = nullptr;
    reqForm->addRow("<font color='#e05555'>*</font> PGM/图像:", makeFileRow(m_pgmEdit, "选择 .pgm 或 .jpg/.png 栅格图", SLOT(onBrowsePgm()), btnPgm));

    QPushButton *btnYaml = nullptr;
    reqForm->addRow("<font color='#e05555'>*</font> YAML配置:", makeFileRow(m_yamlEdit, "选择地图元数据 .yaml 描述文件", SLOT(onBrowseYaml()), btnYaml));

    QPushButton *btnPb = nullptr;
    reqForm->addRow("<font color='#e05555'>*</font> PBStream:", makeFileRow(m_pbStreamEdit, "选择 Cartographer 轨迹 .pbstream 文件", SLOT(onBrowsePbStream()), btnPb));

    rootLayout->addWidget(requiredGroup);

    // ── 可选文件 ──────────────────────────────────────────────────
    auto *optGroup = new QGroupBox("反光柱配置 (可选)", this);
    auto *optForm = new QFormLayout(optGroup);
    optForm->setContentsMargins(14, 16, 14, 14);
    optForm->setSpacing(10);
    optForm->setLabelAlignment(Qt::AlignRight);

    auto makeOptFileRow = [this](QLineEdit *&edit, const QString &hint, const char *slot) {
        auto *row = new QHBoxLayout;
        row->setSpacing(6);
        edit = new QLineEdit(this);
        edit->setReadOnly(true);
        edit->setPlaceholderText(hint);
        auto *browseBtn = new QPushButton("浏览...", this);
        browseBtn->setCursor(Qt::PointingHandCursor);
        connect(browseBtn, SIGNAL(clicked()), this, slot);
        auto *clearBtn = new QPushButton("清除", this);
        clearBtn->setCursor(Qt::PointingHandCursor);
        connect(clearBtn, &QPushButton::clicked, edit, &QLineEdit::clear);
        row->addWidget(edit, 1);
        row->addWidget(browseBtn);
        row->addWidget(clearBtn);
        return row;
    };

    optForm->addRow("反光柱 BIN:", makeOptFileRow(m_reflectorBinEdit, "选择反光柱点云二进制 .bin 文件 (可选)", SLOT(onBrowseReflectorBin())));
    optForm->addRow("反光柱 JSON:", makeOptFileRow(m_reflectorJsonEdit, "选择反光柱参数 .json 文件 (可选)", SLOT(onBrowseReflectorJson())));

    rootLayout->addWidget(optGroup);

    // ── 提示信息 ──
    auto *tipLabel = new QLabel("💡 提示：选择任一文件时，将自动扫描同目录下的关联配置文件并填入地图名称。", this);
    tipLabel->setStyleSheet("color:#708090; font-size:11px;");
    rootLayout->addWidget(tipLabel);

    // ── 底部操作按钮 ──────────────────────────────────────────────
    auto *btnRow = new QHBoxLayout;
    btnRow->addStretch();

    m_cancelBtn = new QPushButton("取消", this);
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    m_uploadBtn = new QPushButton("开始上传", this);
    m_uploadBtn->setObjectName("primaryBtn");
    m_uploadBtn->setCursor(Qt::PointingHandCursor);
    connect(m_uploadBtn, &QPushButton::clicked, this, &MapUploadDialog::onStartUpload);

    btnRow->addWidget(m_cancelBtn);
    btnRow->addWidget(m_uploadBtn);
    rootLayout->addLayout(btnRow);
}

void MapUploadDialog::autoDetectSiblings(const QString &chosenPath)
{
    if (chosenPath.isEmpty()) return;
    QFileInfo fi(chosenPath);
    QDir dir = fi.dir();
    QString stem = fi.completeBaseName();

    // 自动补齐地图名称
    if (m_nameEdit->text().trimmed().isEmpty()) {
        m_nameEdit->setText(stem);
    }

    // 自动探测 PGM/图片
    if (m_pgmEdit->text().isEmpty()) {
        const QStringList exts = {".pgm", ".jpg", ".jpeg", ".png"};
        for (const auto &ext : exts) {
            QString p = dir.filePath(stem + ext);
            if (QFile::exists(p)) { m_pgmEdit->setText(p); break; }
        }
    }

    // 自动探测 YAML
    if (m_yamlEdit->text().isEmpty()) {
        const QStringList exts = {".yaml", ".yml"};
        for (const auto &ext : exts) {
            QString p = dir.filePath(stem + ext);
            if (QFile::exists(p)) { m_yamlEdit->setText(p); break; }
        }
    }

    // 自动探测 PBStream
    if (m_pbStreamEdit->text().isEmpty()) {
        QString p = dir.filePath(stem + ".pbstream");
        if (QFile::exists(p)) m_pbStreamEdit->setText(p);
    }

    // 自动探测反光柱 BIN
    if (m_reflectorBinEdit->text().isEmpty()) {
        const QStringList cands = {stem + ".bin", stem + "_reflector.bin", "reflector.bin"};
        for (const auto &c : cands) {
            QString p = dir.filePath(c);
            if (QFile::exists(p)) { m_reflectorBinEdit->setText(p); break; }
        }
    }

    // 自动探测反光柱 JSON
    if (m_reflectorJsonEdit->text().isEmpty()) {
        const QStringList cands = {stem + ".json", stem + "_reflector.json", "reflector.json"};
        for (const auto &c : cands) {
            QString p = dir.filePath(c);
            if (QFile::exists(p)) { m_reflectorJsonEdit->setText(p); break; }
        }
    }
}

void MapUploadDialog::onBrowsePgm()
{
    QString f = QFileDialog::getOpenFileName(
        this, "选择 PGM 或图像文件", "",
        "PGM / 图像文件 (*.pgm *.jpg *.jpeg *.png);;所有文件 (*.*)");
    if (!f.isEmpty()) {
        m_pgmEdit->setText(f);
        autoDetectSiblings(f);
    }
}

void MapUploadDialog::onBrowseYaml()
{
    QString f = QFileDialog::getOpenFileName(
        this, "选择 YAML 配置文件", "",
        "YAML 配置文件 (*.yaml *.yml);;所有文件 (*.*)");
    if (!f.isEmpty()) {
        m_yamlEdit->setText(f);
        autoDetectSiblings(f);
    }
}

void MapUploadDialog::onBrowsePbStream()
{
    QString f = QFileDialog::getOpenFileName(
        this, "选择 PBStream 轨迹文件", "",
        "PBStream 文件 (*.pbstream);;所有文件 (*.*)");
    if (!f.isEmpty()) {
        m_pbStreamEdit->setText(f);
        autoDetectSiblings(f);
    }
}

void MapUploadDialog::onBrowseReflectorBin()
{
    QString f = QFileDialog::getOpenFileName(
        this, "选择反光柱 BIN 文件", "",
        "BIN 文件 (*.bin);;所有文件 (*.*)");
    if (!f.isEmpty()) {
        m_reflectorBinEdit->setText(f);
    }
}

void MapUploadDialog::onBrowseReflectorJson()
{
    QString f = QFileDialog::getOpenFileName(
        this, "选择反光柱 JSON 文件", "",
        "JSON 文件 (*.json);;所有文件 (*.*)");
    if (!f.isEmpty()) {
        m_reflectorJsonEdit->setText(f);
    }
}

void MapUploadDialog::onStartUpload()
{
    QString mapName = m_nameEdit->text().trimmed();
    if (mapName.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入地图名称");
        m_nameEdit->setFocus();
        return;
    }

    QString pgmPath = m_pgmEdit->text().trimmed();
    if (pgmPath.isEmpty() || !QFile::exists(pgmPath)) {
        QMessageBox::warning(this, "提示", "请选择有效的 PGM / 图像文件");
        return;
    }

    QString yamlPath = m_yamlEdit->text().trimmed();
    if (yamlPath.isEmpty() || !QFile::exists(yamlPath)) {
        QMessageBox::warning(this, "提示", "请选择有效的 YAML 配置文件");
        return;
    }

    QString pbStreamPath = m_pbStreamEdit->text().trimmed();
    if (pbStreamPath.isEmpty() || !QFile::exists(pbStreamPath)) {
        QMessageBox::warning(this, "提示", "请选择有效的 PBStream 文件");
        return;
    }

    QString binPath = m_reflectorBinEdit->text().trimmed();
    if (!binPath.isEmpty() && !QFile::exists(binPath)) {
        QMessageBox::warning(this, "提示", "反光柱 BIN 文件路径无效或文件不存在");
        return;
    }

    QString jsonPath = m_reflectorJsonEdit->text().trimmed();
    if (!jsonPath.isEmpty() && !QFile::exists(jsonPath)) {
        QMessageBox::warning(this, "提示", "反光柱 JSON 文件路径无效或文件不存在");
        return;
    }

    ApiService::IndoorMapUploadParams params;
    params.mapName = mapName;
    params.pgmFilePath = pgmPath;
    params.yamlFilePath = yamlPath;
    params.pbStreamFilePath = pbStreamPath;
    params.reflectorBinFilePath = binPath;
    params.reflectorJsonFilePath = jsonPath;

    auto *loading = new LoadingDialog("正在上传地图文件，请稍候...", this);
    loading->show();

    ApiService::instance().uploadIndoorMap(params, [this, loading, mapName](int code, const QJsonObject &resp) {
        loading->close();
        loading->deleteLater();

        if (code == 200) {
            Toast::success(parentWidget() ? parentWidget() : this, QString("地图「%1」上传成功").arg(mapName));
            emit uploadSucceeded();
            accept();
        } else {
            QString msg = resp.value("msg").toString("未知错误");
            QMessageBox::warning(this, "上传失败", QString("地图上传失败 (错误码 %1):\n%2").arg(code).arg(msg));
        }
    });
}
