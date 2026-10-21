/**
 * @file    MapUploadDialog.cpp
 * @layer   GUILayer
 * @brief   Implementation of map package archive upload dialog.
 * @deps    MapUploadDialog.h, MapUploadStyle.h, MapUploadHandler.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapUploadDialog.h"
#include "MapUploadStyle.h"
#include "MapUploadHandler.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QGroupBox>

MapUploadDialog::MapUploadDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("地图上传 (PGM / YAML / PBStream)");
    setMinimumWidth(560);
    setStyleSheet(MapUploadStyle::kDialogStyle);
    setupUI();
}

void MapUploadDialog::setupUI()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(18, 16, 18, 16);
    rootLayout->setSpacing(14);

    // ── 基本信息与必选文件 ──
    auto *requiredGroup = new QGroupBox("地图必选信息", this);
    auto *reqForm = new QFormLayout(requiredGroup);
    reqForm->setContentsMargins(14, 16, 14, 14);
    reqForm->setSpacing(10);
    reqForm->setLabelAlignment(Qt::AlignRight);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText("请输入地图名称（必填，例如：测试地图）");
    reqForm->addRow("<font color='#e05555'>*</font> 地图名称:", m_nameEdit);

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

    // ── 可选文件 ──
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

    auto *tipLabel = new QLabel("💡 提示：选择任一文件时，将自动扫描同目录下的关联配置文件并填入地图名称。", this);
    tipLabel->setStyleSheet("color:#708090; font-size:11px;");
    rootLayout->addWidget(tipLabel);

    // ── 底部操作按钮 ──
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

void MapUploadDialog::onBrowsePgm()
{
    QString f = QFileDialog::getOpenFileName(
        this, "选择 PGM 或图像文件", "",
        "PGM / 图像文件 (*.pgm *.jpg *.jpeg *.png);;所有文件 (*.*)");
    if (!f.isEmpty()) {
        m_pgmEdit->setText(f);
        MapUploadHandler::autoDetectSiblings(this, f);
    }
}

void MapUploadDialog::onBrowseYaml()
{
    QString f = QFileDialog::getOpenFileName(
        this, "选择 YAML 配置文件", "",
        "YAML 配置文件 (*.yaml *.yml);;所有文件 (*.*)");
    if (!f.isEmpty()) {
        m_yamlEdit->setText(f);
        MapUploadHandler::autoDetectSiblings(this, f);
    }
}

void MapUploadDialog::onBrowsePbStream()
{
    QString f = QFileDialog::getOpenFileName(
        this, "选择 PBStream 轨迹文件", "",
        "PBStream 文件 (*.pbstream);;所有文件 (*.*)");
    if (!f.isEmpty()) {
        m_pbStreamEdit->setText(f);
        MapUploadHandler::autoDetectSiblings(this, f);
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
    MapUploadHandler::executeUpload(this);
}
