/**
 * @file    MapAreaPropsPanel.cpp
 * @layer   GUI
 * @brief   Implementation of high level advanced area property editing panel.
 * @deps    MapAreaPropsPanel.h, MapPropRowHelper.h, MapEditDataModel.h, MapMotionFormGroup.h, RouteNetworkValidator.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapAreaPropsPanel.h"
#include "MapPropRowHelper.h"
#include "MapEditDataModel.h"
#include "MapMotionFormGroup.h"
#include "core/RouteNetworkValidator.h"

#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QVBoxLayout>

using namespace MapPropRowHelper;

MapAreaPropsPanel::MapAreaPropsPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void MapAreaPropsPanel::setupUI()
{
    setStyleSheet("background:transparent; border:none;");
    auto *al = new QVBoxLayout(this);
    al->setContentsMargins(0, 0, 0, 0);
    al->setSpacing(4);

    // 基础信息
    m_areaNameEdit = new QLineEdit("AdvancedArea");
    al->addWidget(mkCheckableRow(m_chkAreaName, "区域名称:", m_areaNameEdit));

    m_areaBoundsLabel = new QLabel("--");
    m_areaBoundsLabel->setStyleSheet("background:#12151e; color:#79a2c9; border:1px solid #232b3b; border-radius:3px; padding:2px 5px; font-size:11px;");
    al->addWidget(mkFormRow("区域范围:", m_areaBoundsLabel));

    // 运动控制参数与实时避障参数
    m_motionGroup = new MapMotionFormGroup(this);
    al->addWidget(m_motionGroup);
}

void MapAreaPropsPanel::syncMotionSpinBoxRanges()
{
    if (m_motionGroup) {
        m_motionGroup->syncMotionSpinBoxRanges();
    }
}

void MapAreaPropsPanel::populate(const QJsonObject &obj)
{
    m_areaNameEdit->setText(obj.value("name").toString("AdvancedArea"));
    double minX = 0, maxX = 0, minY = 0, maxY = 0;
    RouteNetworkValidator::extractAreaBounds(obj, minX, maxX, minY, maxY);
    m_areaBoundsLabel->setText(QString("X: [%1, %2]  Y: [%3, %4]")
        .arg(minX, 0, 'f', 2).arg(maxX, 0, 'f', 2).arg(minY, 0, 'f', 2).arg(maxY, 0, 'f', 2));

    restoreCheckedProps(obj.value("checked_props").toArray(), {
        {m_chkAreaName, "name"}
    });

    if (m_motionGroup) {
        m_motionGroup->populate(obj, true /* isArea */);
    }
}

void MapAreaPropsPanel::applyEdit(MapEditDataModel *model, const QString &selId)
{
    if (!model) return;
    QJsonObject areaObj = model->findFeature("highLevelArea", selId);
    if (areaObj.isEmpty()) return;

    if (m_chkAreaName && m_chkAreaName->isChecked()) {
        areaObj["name"] = m_areaNameEdit->text().trimmed();
    }

    QJsonArray baseChecked = collectCheckedProps({
        {m_chkAreaName, "name"}
    });
    areaObj["checked_props"] = baseChecked;

    if (m_motionGroup) {
        m_motionGroup->applyTo(areaObj, true /* isArea */);
    }

    double aMinX = 0, aMaxX = 0, aMinY = 0, aMaxY = 0;
    RouteNetworkValidator::extractAreaBounds(areaObj, aMinX, aMaxX, aMinY, aMaxY);
    QJsonArray aCoords;
    QJsonArray aTL, aBR;
    aTL.append(aMinX); aTL.append(aMaxY);
    aBR.append(aMaxX); aBR.append(aMinY);
    aCoords.append(aTL); aCoords.append(aBR);
    areaObj["coordinates"] = aCoords;
    areaObj.remove("minX");
    areaObj.remove("maxX");
    areaObj.remove("minY");
    areaObj.remove("maxY");

    model->updateFeature("highLevelArea", selId, areaObj);
}

void MapAreaPropsPanel::clear()
{
    m_areaNameEdit->setText("AdvancedArea");
    m_areaBoundsLabel->setText("--");
    if (m_motionGroup) {
        m_motionGroup->clear();
    }
}
