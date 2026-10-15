#include "SmapUploadDialog.h"
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
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>

namespace {

const char *kDialogStyle = R"(
QDialog {
    background-color: #1a1d23;
    color: #c8d0d8;
    font-family: "Microsoft YaHei UI", "Segoe UI", sans-serif;
    font-size: 13px;
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
    padding: 6px 8px;
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
    padding: 6px 14px;
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

SmapUploadDialog::SmapUploadDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("smap 地图上传");
    setMinimumWidth(500);
    setStyleSheet(kDialogStyle);
    setupUI();
}

void SmapUploadDialog::setupUI()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(20, 18, 20, 18);
    rootLayout->setSpacing(16);

    auto *form = new QFormLayout;
    form->setSpacing(12);
    form->setLabelAlignment(Qt::AlignRight);

    // 地图名称
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText("请输入地图名称（例如：车间一号地图）");
    form->addRow("<font color='#e05555'>*</font> 地图名称:", m_nameEdit);

    // smap 文件
    auto *fileRow = new QHBoxLayout;
    fileRow->setSpacing(6);
    m_fileEdit = new QLineEdit(this);
    m_fileEdit->setReadOnly(true);
    m_fileEdit->setPlaceholderText("请选择工业级 .smap 地图文件");
    auto *browseBtn = new QPushButton("浏览...", this);
    browseBtn->setCursor(Qt::PointingHandCursor);
    connect(browseBtn, &QPushButton::clicked, this, &SmapUploadDialog::onBrowseSmap);
    fileRow->addWidget(m_fileEdit, 1);
    fileRow->addWidget(browseBtn);
    form->addRow("<font color='#e05555'>*</font> smap文件:", fileRow);

    rootLayout->addLayout(form);

    auto *tipLabel = new QLabel("💡 提示：选择 .smap 文件后，系统将自动读取文件内的地图名称作为默认名称。", this);
    tipLabel->setStyleSheet("color:#708090; font-size:11px;");
    rootLayout->addWidget(tipLabel);

    // 底部按钮
    auto *btnRow = new QHBoxLayout;
    btnRow->addStretch();

    m_cancelBtn = new QPushButton("取消", this);
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    m_uploadBtn = new QPushButton("开始上传", this);
    m_uploadBtn->setObjectName("primaryBtn");
    m_uploadBtn->setCursor(Qt::PointingHandCursor);
    connect(m_uploadBtn, &QPushButton::clicked, this, &SmapUploadDialog::onStartUpload);

    btnRow->addWidget(m_cancelBtn);
    btnRow->addWidget(m_uploadBtn);
    rootLayout->addLayout(btnRow);
}

void SmapUploadDialog::onBrowseSmap()
{
    QString f = QFileDialog::getOpenFileName(
        this, "选择 smap 地图文件", "",
        "smap 地图文件 (*.smap);;所有文件 (*.*)");
    if (f.isEmpty()) return;

    m_fileEdit->setText(f);

    // 尝试读取内部 header.mapName
    if (m_nameEdit->text().trimmed().isEmpty()) {
        QString detectedName;
        QFile file(f);
        if (file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            if (doc.isObject()) {
                detectedName = doc.object().value("header").toObject().value("mapName").toString().trimmed();
            }
        }
        if (detectedName.isEmpty()) {
            detectedName = QFileInfo(f).completeBaseName();
        }
        m_nameEdit->setText(detectedName);
    }
}

void SmapUploadDialog::onStartUpload()
{
    QString mapName = m_nameEdit->text().trimmed();
    if (mapName.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入地图名称");
        m_nameEdit->setFocus();
        return;
    }

    QString smapPath = m_fileEdit->text().trimmed();
    if (smapPath.isEmpty() || !QFile::exists(smapPath)) {
        QMessageBox::warning(this, "提示", "请选择有效的 .smap 地图文件");
        return;
    }

    auto *loading = new LoadingDialog("正在上传 smap 地图，请稍候...", this);
    loading->show();

    ApiService::instance().uploadSmap(mapName, smapPath, [this, loading, mapName](int code, const QJsonObject &resp) {
        loading->close();
        loading->deleteLater();

        if (code == 200) {
            Toast::success(parentWidget() ? parentWidget() : this, QString("smap 地图「%1」上传成功").arg(mapName));
            emit uploadSucceeded();
            accept();
        } else {
            QString msg = resp.value("msg").toString("未知错误");
            QMessageBox::warning(this, "上传失败", QString("smap 上传失败 (错误码 %1):\n%2").arg(code).arg(msg));
        }
    });
}
