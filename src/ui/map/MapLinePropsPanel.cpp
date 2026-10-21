/**
 * @file    MapLinePropsPanel.cpp
 * @layer   GUI
 * @brief   Implementation of straight line and Bezier curve property editing panel.
 * @deps    MapLinePropsPanel.h, MapPropRowHelper.h, MapEditTypes.h, MapEditDataModel.h, MapMotionFormGroup.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapLinePropsPanel.h"
#include "MapPropRowHelper.h"
#include "MapEditTypes.h"
#include "MapEditDataModel.h"
#include "MapMotionFormGroup.h"

#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

using namespace MapPropRowHelper;
using namespace MapEdit;

MapLinePropsPanel::MapLinePropsPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void MapLinePropsPanel::setupUI()
{
    setStyleSheet("background:transparent; border:none;");
    auto *ll = new QVBoxLayout(this);
    ll->setContentsMargins(0, 0, 0, 0);
    ll->setSpacing(4);

    // 基础信息
    m_lineTypeCombo = new QComboBox;
    m_lineTypeCombo->addItem("直线", "line");
    m_lineTypeCombo->addItem("贝塞尔曲线", "curve");
    ll->addWidget(mkFormRow("线路类型:", m_lineTypeCombo));

    m_lineIdEdit = new QLineEdit;
    m_lineIdEdit->setReadOnly(true);
    m_lineIdEdit->setStyleSheet("background:#12151e; color:#627284; border:1px solid #232b3b; border-radius:3px; padding:2px 5px; font-size:11px;");
    ll->addWidget(mkFormRow("线路编号:", m_lineIdEdit));

    m_lineNameEdit = new QLineEdit;
    ll->addWidget(mkCheckableRow(m_chkLineName, "线路名称:", m_lineNameEdit));

    // 起止端点
    m_lineStartLabel = new QLabel("--");
    m_lineStartLabel->setStyleSheet("background:#12151e; color:#79a2c9; border:1px solid #232b3b; border-radius:3px; padding:2px 5px; font-size:11px;");
    ll->addWidget(mkFormRow("起点 (Pos1):", m_lineStartLabel));

    m_lineEndLabel = new QLabel("--");
    m_lineEndLabel->setStyleSheet("background:#12151e; color:#79a2c9; border:1px solid #232b3b; border-radius:3px; padding:2px 5px; font-size:11px;");
    ll->addWidget(mkFormRow("终点 (Pos2):", m_lineEndLabel));

    // 运动方向
    auto *dirRow = new QWidget;
    dirRow->setStyleSheet("background:transparent; border:none;");
    auto *dirl = new QHBoxLayout(dirRow);
    dirl->setContentsMargins(0, 0, 0, 0);
    dirl->setSpacing(4);
    m_lineDirectCombo = new QComboBox;
    m_lineDirectCombo->addItem("双向 ↔", kDirectionBidirectional);
    m_lineDirectCombo->addItem("单向前进 →", kDirectionForward);
    m_lineDirectCombo->addItem("单向后退 ←", kDirectionBackward);
    m_lineFlipDirBtn = new QPushButton("⇄ 翻转");
    m_lineFlipDirBtn->setToolTip("翻转单向行进方向或交换端点");
    m_lineFlipDirBtn->setFixedWidth(52);
    m_lineFlipDirBtn->setStyleSheet(
        "QPushButton{background:#1e2736; border:1px solid #2e3d54; border-radius:3px; color:#8fa6bf; font-size:11px; padding:2px 4px;}"
        "QPushButton:hover{background:#2a374c; color:#fff;}");
    connect(m_lineFlipDirBtn, &QPushButton::clicked, this, [this]() {
        int curDirect = m_lineDirectCombo->currentData().toInt();
        if (curDirect == kDirectionForward)       m_lineDirectCombo->setCurrentIndex(2);
        else if (curDirect == kDirectionBackward) m_lineDirectCombo->setCurrentIndex(1);
        else {
            QString t1 = m_lineStartLabel->text();
            m_lineStartLabel->setText(m_lineEndLabel->text());
            m_lineEndLabel->setText(t1);
        }
    });
    dirl->addWidget(m_lineDirectCombo, 1);
    dirl->addWidget(m_lineFlipDirBtn);
    ll->addWidget(mkCheckableRow(m_chkLineDirect, "运动方向:", dirRow));

    // 运动控制参数与避障参数卡片组
    m_motionGroup = new MapMotionFormGroup(this);
    ll->addWidget(m_motionGroup);
}

void MapLinePropsPanel::syncMotionSpinBoxRanges()
{
    if (m_motionGroup) {
        m_motionGroup->syncMotionSpinBoxRanges();
    }
}

void MapLinePropsPanel::populate(const QJsonObject &obj, const QString &type, const QString &id)
{
    m_lineTypeCombo->setCurrentIndex(type == "line" ? 0 : 1);
    m_lineIdEdit->setText(id);
    m_lineNameEdit->setText(obj.value("name").toString());

    QString sId = obj.value("startid").toString();
    QString eId = obj.value("endid").toString();
    m_lineStartLabel->setText(sId.isEmpty() ? "--" : sId);
    m_lineEndLabel->setText(eId.isEmpty() ? "--" : eId);

    int direct = obj.value("direct").toInt(kDirectionBidirectional);
    int dIdx = (direct == kDirectionBidirectional) ? 0 : ((direct == kDirectionForward) ? 1 : 2);
    m_lineDirectCombo->setCurrentIndex(dIdx);

    restoreCheckedProps(obj.value("checked_props").toArray(), {
        {m_chkLineName, "name"},
        {m_chkLineDirect, "direct"}
    });

    if (m_motionGroup) {
        m_motionGroup->populate(obj, false);
    }
}

void MapLinePropsPanel::applyEdit(MapEditDataModel *model, const QString &selType, const QString &selId)
{
    if (!model) return;
    QJsonObject o = model->findFeature(selType, selId);
    if (o.isEmpty()) return;

    if (m_chkLineName && m_chkLineName->isChecked()) {
        o["name"] = m_lineNameEdit->text().trimmed();
    }
    if (m_chkLineDirect && m_chkLineDirect->isChecked()) {
        o["direct"] = m_lineDirectCombo->currentData().toInt();
    }

    QJsonArray baseChecked = collectCheckedProps({
        {m_chkLineName, "name"},
        {m_chkLineDirect, "direct"}
    });
    o["checked_props"] = baseChecked;

    if (m_motionGroup) {
        m_motionGroup->applyTo(o, false);
    }

    model->updateFeature(selType, selId, o);
    model->syncLineObstaclesToEndpoints();
}

void MapLinePropsPanel::clear()
{
    m_lineIdEdit->clear();
    m_lineNameEdit->clear();
    m_lineStartLabel->setText("--");
    m_lineEndLabel->setText("--");
    m_lineDirectCombo->setCurrentIndex(0);
    if (m_motionGroup) {
        m_motionGroup->clear();
    }
}
