/**
 * @file    MapViewStationPropsPanel.h
 * @layer   GUI
 * @brief   Property panel for inspecting station attributes, types, poses, and EPC calibration.
 * @deps    QWidget, QJsonObject, QLabel, QCheckBox, QGroupBox
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QWidget>
#include <QLabel>
#include <QCheckBox>
#include <QGroupBox>
#include <QJsonObject>

class MapViewStationPropsPanel : public QWidget
{
    Q_OBJECT
public:
    explicit MapViewStationPropsPanel(QWidget *parent = nullptr);
    ~MapViewStationPropsPanel() override = default;

    void showStationDetails(const QJsonObject &pt, const QString &type);
    void clearStationDetails();

private:
    void setupUI();
    static QWidget* mkCheckRow(QCheckBox *&chk, const QString &text, QLabel *valLbl);

    QGroupBox *m_stationGroup         = nullptr;
    QLabel    *m_stationBadge         = nullptr;
    QLabel    *m_stationEmptyHint     = nullptr;
    QWidget   *m_stationContentWidget = nullptr;
    QCheckBox *m_chkStationType       = nullptr;
    QLabel    *m_stationTypeVal       = nullptr;
    QCheckBox *m_chkStationId         = nullptr;
    QLabel    *m_stationIdVal         = nullptr;
    QCheckBox *m_chkStationName       = nullptr;
    QLabel    *m_stationNameVal       = nullptr;
    QCheckBox *m_chkStationPos        = nullptr;
    QLabel    *m_stationPosVal        = nullptr;
    QCheckBox *m_chkStationYaw        = nullptr;
    QLabel    *m_stationYawVal        = nullptr;
    QCheckBox *m_chkStationDock       = nullptr;
    QLabel    *m_stationDockVal       = nullptr;
    QCheckBox *m_chkStationRotate     = nullptr;
    QLabel    *m_stationRotateVal     = nullptr;
    QCheckBox *m_chkStationPgv        = nullptr;
    QLabel    *m_stationPgvVal        = nullptr;
    QWidget   *m_stationEpcGroup      = nullptr;
    QLabel    *m_stationEpcPosVal     = nullptr;
    QLabel    *m_stationEpcYawVal     = nullptr;
};
