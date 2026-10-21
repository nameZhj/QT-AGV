/**
 * @file    MapMotionFormGroup.cpp
 * @layer   GUI
 * @brief   Implementation of reusable motion and obstacle parameter form card group.
 * @deps    MapMotionFormGroup.h, MapPropRowHelper.h, MotionConfigManager.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapMotionFormGroup.h"
#include "MapPropRowHelper.h"
#include "core/MotionConfigManager.h"

#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QVBoxLayout>

using namespace MapPropRowHelper;

MapMotionFormGroup::MapMotionFormGroup(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void MapMotionFormGroup::setupUI()
{
    setStyleSheet("background:transparent; border:none;");
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(4);

    // 运动控制参数卡片
    auto *motionCard = new QWidget;
    motionCard->setStyleSheet("background:#161c28; border:1px solid #232d3e; border-radius:4px;");
    auto *mcl = new QVBoxLayout(motionCard);
    mcl->setContentsMargins(6, 5, 6, 5);
    mcl->setSpacing(3);
    auto *mTitle = new QLabel("运动控制参数");
    mTitle->setStyleSheet("color:#627e9e; font-size:10px; font-weight:bold; border:none;");
    mcl->addWidget(mTitle);

    auto addSpin = [&](QCheckBox *&chk, const QString &lbl, QDoubleSpinBox *&spin, double step, const QString &unit) {
        spin = new QDoubleSpinBox;
        spin->setSingleStep(step);
        mcl->addWidget(mkCheckableRow(chk, lbl, spin, unit));
    };
    addSpin(m_chkMaxSpeed, "最大速度:", m_maxSpeedSpin, 0.05, "m/s");
    addSpin(m_chkMaxAcc, "最大加速度:", m_maxAccSpin, 0.05, "m/s²");
    m_maxDecSpin = new QDoubleSpinBox;
    m_maxDecSpin->setSingleStep(0.05);
    QWidget *rowMaxDec = mkFormRow("最大减速度:", m_maxDecSpin, "m/s²");
    rowMaxDec->setVisible(false);
    mcl->addWidget(rowMaxDec);
    addSpin(m_chkMaxRot, "最大角速度:", m_maxRotSpin, 5.0, "°/s");
    addSpin(m_chkMaxRotAcc, "最大角加减:", m_maxRotAccSpin, 5.0, "°/s²");
    addSpin(m_chkReachDist, "到达距离精度:", m_reachDistSpin, 0.01, "m");
    addSpin(m_chkReachAngle, "到达角度精度:", m_reachAngleSpin, 0.1, "°");
    addSpin(m_chkRoadWidth, "路线宽度:", m_roadWidthSpin, 0.5, "m");
    root->addWidget(motionCard);

    // 实时避障范围卡片
    auto *obsCard = new QWidget;
    obsCard->setStyleSheet("background:#161c28; border:1px solid #232d3e; border-radius:4px;");
    auto *ocl = new QVBoxLayout(obsCard);
    ocl->setContentsMargins(6, 5, 6, 5);
    ocl->setSpacing(3);

    auto *oTitleCont = new QWidget;
    oTitleCont->setStyleSheet("background:transparent; border:none;");
    auto *oth = new QHBoxLayout(oTitleCont);
    oth->setContentsMargins(0, 0, 0, 2);
    oth->setSpacing(6);
    m_chkObs = new QCheckBox("实时避障范围");
    m_chkObs->setStyleSheet(
        "QCheckBox { color:#627e9e; font-size:11px; font-weight:bold; background:transparent; }"
        "QCheckBox::indicator { width:14px; height:14px; border:1px solid #3d4a5d; border-radius:2px; background:#171c26; }"
        "QCheckBox::indicator:checked { background:#1e6fd4; border-color:#1e6fd4; image:url(:/check.svg); }"
    );
    oth->addWidget(m_chkObs);
    oth->addStretch();
    ocl->addWidget(oTitleCont);

    auto addObs = [&](QDoubleSpinBox *&spin, const QString &lbl, const QString &tip) {
        spin = new QDoubleSpinBox;
        spin->setRange(0.00, 5.00);
        spin->setSingleStep(0.05);
        spin->setValue(0.00);
        spin->setToolTip(tip);
        ocl->addWidget(mkFormRow(lbl, spin, "m"));
    };
    addObs(m_obsStopLenSpin, "正向停止膨胀:", "正向停止膨胀半径 (intrusion_stop_length)");
    addObs(m_obsStopWidSpin, "侧向停止膨胀:", "侧向停止膨胀半径 (intrusion_stop_width)");
    addObs(m_obsDecelLenSpin, "正向减速膨胀:", "正向减速膨胀半径 (intrusion_deceleration_length)");
    addObs(m_obsDecelWidSpin, "侧向减速膨胀:", "侧向减速膨胀半径 (intrusion_deceleration_width)");

    auto updateObsEnabled = [this](bool on) {
        m_obsStopLenSpin->setEnabled(on);
        m_obsStopWidSpin->setEnabled(on);
        m_obsDecelLenSpin->setEnabled(on);
        m_obsDecelWidSpin->setEnabled(on);
    };
    connect(m_chkObs, &QCheckBox::toggled, this, updateObsEnabled);
    updateObsEnabled(false);
    root->addWidget(obsCard);
}

void MapMotionFormGroup::syncMotionSpinBoxRanges()
{
    const auto &cfg = MotionConfigManager::instance();
    auto syncSpin = [](QDoubleSpinBox *spin, double minVal, double maxVal, const QString &tipFmt = QString(), double defVal = 0.0) {
        if (!spin) return;
        spin->setRange(minVal, maxVal);
        if (!tipFmt.isEmpty()) spin->setToolTip(tipFmt.arg(minVal, 0, 'f', 2).arg(maxVal, 0, 'f', 2));
        if (spin->value() == 0.0 && defVal > 0.0) spin->setValue(defVal);
        else if (spin->value() > maxVal) spin->setValue(maxVal);
        else if (spin->value() < minVal) spin->setValue(minVal);
    };

    syncSpin(m_maxSpeedSpin, cfg.paramMin("max_speed", 0.0), cfg.paramMax("max_speed", 1.0), "受全局配置范围控制 (范围: [%1, %2] m/s)", cfg.maxSpeed());
    syncSpin(m_maxAccSpin, cfg.paramMin("max_acceleration", 0.3), cfg.paramMax("max_acceleration", 0.5), "受全局配置范围控制 (范围: [%1, %2] m/s²)", cfg.maxAcceleration());
    syncSpin(m_maxDecSpin, cfg.paramMin("max_deceleration", 0.3), cfg.paramMax("max_deceleration", 0.5), "受全局配置范围控制 (范围: [%1, %2] m/s²)", cfg.maxDeceleration());
    if (m_maxRotSpin) {
        double mn = cfg.paramMin("max_angular_speed", 5.0), mx = cfg.paramMax("max_angular_speed", 60.0);
        m_maxRotSpin->setRange(mn, mx);
        m_maxRotSpin->setToolTip(QString("受全局配置范围控制 (范围: [%1, %2] °/s)").arg(mn, 0, 'f', 1).arg(mx, 0, 'f', 1));
        if (m_maxRotSpin->value() == 0.0) m_maxRotSpin->setValue(cfg.maxAngularSpeedDeg());
        else m_maxRotSpin->setValue(qBound(mn, m_maxRotSpin->value(), mx));
    }
    if (m_maxRotAccSpin) {
        double mn = cfg.paramMin("max_angular_acceleration", 10.0), mx = cfg.paramMax("max_angular_acceleration", 30.0);
        m_maxRotAccSpin->setRange(mn, mx);
        m_maxRotAccSpin->setToolTip(QString("受全局配置范围控制 (范围: [%1, %2] °/s²)").arg(mn, 0, 'f', 1).arg(mx, 0, 'f', 1));
        if (m_maxRotAccSpin->value() == 0.0) m_maxRotAccSpin->setValue(cfg.maxAngularAccelerationDeg());
        else m_maxRotAccSpin->setValue(qBound(mn, m_maxRotAccSpin->value(), mx));
    }
    if (m_reachDistSpin) {
        m_reachDistSpin->setRange(0.01, qMax(1.0, cfg.arrivalDistanceTolerance()));
        if (m_reachDistSpin->value() == 0.0) m_reachDistSpin->setValue(cfg.arrivalDistanceTolerance());
    }
    if (m_reachAngleSpin) {
        m_reachAngleSpin->setRange(0.1, qMax(45.0, cfg.arrivalAngleToleranceDeg()));
        if (m_reachAngleSpin->value() == 0.0) m_reachAngleSpin->setValue(cfg.arrivalAngleToleranceDeg());
    }
    if (m_roadWidthSpin) {
        m_roadWidthSpin->setRange(0.5, qMax(10.0, cfg.routeWidth()));
        if (m_roadWidthSpin->value() == 0.0) m_roadWidthSpin->setValue(cfg.routeWidth());
    }
}

void MapMotionFormGroup::populate(const QJsonObject &obj, bool isArea)
{
    const auto &mc = MotionConfigManager::instance();
    syncMotionSpinBoxRanges();

    auto extract = [&](const QString &k1, const QString &k2, double defVal) -> double {
        if (obj.contains(k1)) return obj.value(k1).toDouble(defVal);
        if (!k2.isEmpty() && obj.contains(k2)) return obj.value(k2).toDouble(defVal);
        return defVal;
    };

    m_maxSpeedSpin->setValue(qBound(mc.paramMin("max_speed", 0.0), extract("maxspeed", "max_speed", mc.maxSpeed()), mc.paramMax("max_speed", 1.0)));
    m_maxAccSpin->setValue(qBound(mc.paramMin("max_acceleration", 0.3), extract("max_acceleration", "maxacc", mc.maxAcceleration()), mc.paramMax("max_acceleration", 0.5)));
    m_maxDecSpin->setValue(qBound(mc.paramMin("max_deceleration", 0.3), extract("max_deceleration", "maxdec", mc.maxDeceleration()), mc.paramMax("max_deceleration", 0.5)));
    m_maxRotSpin->setValue(qBound(mc.paramMin("max_angular_speed", 5.0), extract("max_angular_speed", "maxrot", mc.maxAngularSpeedDeg()), mc.paramMax("max_angular_speed", 60.0)));
    m_maxRotAccSpin->setValue(qBound(mc.paramMin("max_angular_acceleration", 10.0), extract("max_angular_acceleration", "maxrotacc", mc.maxAngularAccelerationDeg()), mc.paramMax("max_angular_acceleration", 30.0)));

    if (isArea) {
        m_reachDistSpin->setValue(extract("arrival_distance_tolerance", "reachdist", mc.arrivalDistanceTolerance()));
        m_reachAngleSpin->setValue(extract("arrival_angle_tolerance", "reachangle", mc.arrivalAngleToleranceDeg()));
        m_roadWidthSpin->setValue(extract("route_width", "width", mc.routeWidth()));
    } else {
        m_reachDistSpin->setValue(obj.value("reachdist").toDouble(mc.arrivalDistanceTolerance()));
        m_reachAngleSpin->setValue(obj.value("reachangle").toDouble(mc.arrivalAngleToleranceDeg()));
        m_roadWidthSpin->setValue(obj.value("width").toDouble(mc.routeWidth()));
    }

    m_obsStopLenSpin->setValue(extract("intrusion_stop_length", "", isArea ? 0.0 : mc.defaultObsStopLength()));
    m_obsStopWidSpin->setValue(extract("intrusion_stop_width", "", isArea ? 0.0 : mc.defaultObsStopWidth()));
    m_obsDecelLenSpin->setValue(extract("intrusion_deceleration_length", "", isArea ? 0.0 : mc.defaultObsDecelLength()));
    m_obsDecelWidSpin->setValue(extract("intrusion_deceleration_width", "", isArea ? 0.0 : mc.defaultObsDecelWidth()));

    restoreCheckedProps(obj.value("checked_props").toArray(), {
        {m_chkMaxSpeed, "maxspeed"}, {m_chkMaxAcc, "maxacc"}, {m_chkMaxRot, "maxrot"},
        {m_chkMaxRotAcc, "maxrotacc"}, {m_chkReachDist, "reachdist"}, {m_chkReachAngle, "reachangle"}, {m_chkRoadWidth, "width"}
    });

    QSet<QString> chkSet;
    for (const auto &v : obj.value("checked_props").toArray()) chkSet.insert(v.toString());
    bool obsChecked = chkSet.contains("intrusion_stop_length") || chkSet.contains("intrusion_stop_width") ||
                      chkSet.contains("intrusion_deceleration_length") || chkSet.contains("intrusion_deceleration_width") ||
                      (!isArea && obj.value("is_custom_obstacle").toBool());
    if (m_chkObs) m_chkObs->setChecked(obsChecked);
}

void MapMotionFormGroup::applyTo(QJsonObject &obj, bool isArea)
{
    const auto &mc = MotionConfigManager::instance();
    double spd = qBound(mc.paramMin("max_speed", 0.0), m_maxSpeedSpin->value(), mc.paramMax("max_speed", 1.0));
    double acc = qBound(mc.paramMin("max_acceleration", 0.3), m_maxAccSpin->value(), mc.paramMax("max_acceleration", 0.5));
    double rot = qBound(mc.paramMin("max_angular_speed", 5.0), m_maxRotSpin->value(), mc.paramMax("max_angular_speed", 60.0));
    double rotAcc = qBound(mc.paramMin("max_angular_acceleration", 10.0), m_maxRotAccSpin->value(), mc.paramMax("max_angular_acceleration", 30.0));

    if (m_chkMaxSpeed && m_chkMaxSpeed->isChecked()) obj["maxspeed"] = spd;
    if (isArea) obj.remove("max_speed");

    struct DualKey { QCheckBox *chk; double val; const char *k1; const char *k2; };
    const DualKey duals[] = {
        {m_chkMaxAcc, acc, "max_acceleration", "maxacc"},
        {m_chkMaxRot, rot, "max_angular_speed", "maxrot"},
        {m_chkMaxRotAcc, rotAcc, "max_angular_acceleration", "maxrotacc"}
    };
    for (const auto &d : duals) {
        if (d.chk && d.chk->isChecked()) { obj[d.k1] = d.val; obj[d.k2] = d.val; }
    }

    if (isArea) {
        if (m_chkReachDist && m_chkReachDist->isChecked()) obj["arrival_distance_tolerance"] = m_reachDistSpin->value();
        if (m_chkReachAngle && m_chkReachAngle->isChecked()) obj["arrival_angle_tolerance"] = m_reachAngleSpin->value();
        if (m_chkRoadWidth && m_chkRoadWidth->isChecked()) obj["route_width"] = m_roadWidthSpin->value();
    } else {
        if (m_chkReachDist && m_chkReachDist->isChecked()) obj["reachdist"] = m_reachDistSpin->value();
        if (m_chkReachAngle && m_chkReachAngle->isChecked()) obj["reachangle"] = m_reachAngleSpin->value();
        if (m_chkRoadWidth && m_chkRoadWidth->isChecked()) obj["width"] = m_roadWidthSpin->value();
    }

    bool obsOn = m_chkObs && m_chkObs->isChecked();
    if (obsOn) {
        obj["intrusion_stop_length"] = m_obsStopLenSpin->value();
        obj["intrusion_stop_width"] = m_obsStopWidSpin->value();
        obj["intrusion_deceleration_length"] = m_obsDecelLenSpin->value();
        obj["intrusion_deceleration_width"] = m_obsDecelWidSpin->value();
        if (!isArea) obj["is_custom_obstacle"] = true;
    } else if (!isArea) {
        obj["is_custom_obstacle"] = false;
    }

    QJsonArray checkedProps = obj.value("checked_props").toArray();
    for (const auto &v : collectCheckedProps({
        {m_chkMaxSpeed, "maxspeed"}, {m_chkMaxAcc, "maxacc"}, {m_chkMaxRot, "maxrot"},
        {m_chkMaxRotAcc, "maxrotacc"}, {m_chkReachDist, "reachdist"}, {m_chkReachAngle, "reachangle"}, {m_chkRoadWidth, "width"}
    })) {
        checkedProps.append(v);
    }
    if (obsOn) {
        checkedProps.append("intrusion_stop_length");
        checkedProps.append("intrusion_stop_width");
        checkedProps.append("intrusion_deceleration_length");
        checkedProps.append("intrusion_deceleration_width");
    }
    obj["checked_props"] = checkedProps;
}

void MapMotionFormGroup::clear()
{
    m_maxSpeedSpin->setValue(0.0);
    m_maxAccSpin->setValue(0.0);
    m_maxDecSpin->setValue(0.0);
    m_maxRotSpin->setValue(0.0);
    m_maxRotAccSpin->setValue(0.0);
    m_reachDistSpin->setValue(0.0);
    m_reachAngleSpin->setValue(0.0);
    m_roadWidthSpin->setValue(0.0);
    m_obsStopLenSpin->setValue(0.0);
    m_obsStopWidSpin->setValue(0.0);
    m_obsDecelLenSpin->setValue(0.0);
    m_obsDecelWidSpin->setValue(0.0);
    if (m_chkObs) m_chkObs->setChecked(false);
}

bool MapMotionFormGroup::isObsChecked() const
{
    return m_chkObs && m_chkObs->isChecked();
}
