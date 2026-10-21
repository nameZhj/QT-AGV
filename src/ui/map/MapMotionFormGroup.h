/**
 * @file    MapMotionFormGroup.h
 * @layer   GUI
 * @brief   Reusable motion and obstacle parameter form card group for lines and areas.
 * @deps    QWidget, QJsonObject, QJsonArray
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QWidget>
#include <QJsonObject>
#include <QJsonArray>

class QDoubleSpinBox;
class QCheckBox;

class MapMotionFormGroup : public QWidget
{
    Q_OBJECT

public:
    explicit MapMotionFormGroup(QWidget *parent = nullptr);
    ~MapMotionFormGroup() override = default;

    void syncMotionSpinBoxRanges();
    void populate(const QJsonObject &obj, bool isArea = false);
    void applyTo(QJsonObject &obj, bool isArea = false);
    void clear();

    bool isObsChecked() const;

private:
    void setupUI();

    // === [Section: Motion SpinBoxes & CheckBoxes] ===
    QCheckBox      *m_chkMaxSpeed = nullptr;
    QDoubleSpinBox *m_maxSpeedSpin = nullptr;
    QCheckBox      *m_chkMaxAcc = nullptr;
    QDoubleSpinBox *m_maxAccSpin = nullptr;
    QDoubleSpinBox *m_maxDecSpin = nullptr;
    QCheckBox      *m_chkMaxRot = nullptr;
    QDoubleSpinBox *m_maxRotSpin = nullptr;
    QCheckBox      *m_chkMaxRotAcc = nullptr;
    QDoubleSpinBox *m_maxRotAccSpin = nullptr;
    QCheckBox      *m_chkReachDist = nullptr;
    QDoubleSpinBox *m_reachDistSpin = nullptr;
    QCheckBox      *m_chkReachAngle = nullptr;
    QDoubleSpinBox *m_reachAngleSpin = nullptr;
    QCheckBox      *m_chkRoadWidth = nullptr;
    QDoubleSpinBox *m_roadWidthSpin = nullptr;

    // === [Section: Obstacle SpinBoxes & CheckBoxes] ===
    QCheckBox      *m_chkObs = nullptr;
    QDoubleSpinBox *m_obsStopLenSpin = nullptr;
    QDoubleSpinBox *m_obsStopWidSpin = nullptr;
    QDoubleSpinBox *m_obsDecelLenSpin = nullptr;
    QDoubleSpinBox *m_obsDecelWidSpin = nullptr;
};
