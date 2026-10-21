/**
 * @file    CreateMapTeleopPanel.h
 * @layer   GUI
 * @brief   Teleoperation control pad and HUD metrics panel for SLAM mapping.
 * @deps    QWidget, QLabel, QPushButton
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

class CreateMapTeleopPanel : public QWidget
{
    Q_OBJECT
public:
    explicit CreateMapTeleopPanel(QWidget *parent = nullptr);
    ~CreateMapTeleopPanel() override = default;

    void setLinearVelocity(double lv);
    void setAngularVelocity(double av);
    void setTeleopActive(bool active);
    void setBrakeActive(bool brake);

private:
    void setupUI();

    QLabel      *m_linearLabel     = nullptr;
    QLabel      *m_angularLabel    = nullptr;
    QLabel      *m_teleopLabel     = nullptr;
    QLabel      *m_brakeLabel      = nullptr;
    QPushButton *m_teleopEnableBtn = nullptr;
};
