/**
 * @file    MapStationPropsPanel.h
 * @layer   GUI
 * @brief   Property editing panel for stations (LM), charging points (CP), and high precision points (TL).
 * @deps    QWidget, QJsonObject, MapEditDataModel
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QWidget>
#include <QJsonObject>

class QComboBox;
class QLineEdit;
class QDoubleSpinBox;
class QCheckBox;
class QLabel;
class MapEditDataModel;

class MapStationPropsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit MapStationPropsPanel(QWidget *parent = nullptr);
    ~MapStationPropsPanel() override = default;

    void populate(const QJsonObject &obj, const QString &type, const QString &id);
    void applyEdit(MapEditDataModel *model, QString &selType, QString &selId);
    void clear();

private:
    void setupUI();

    // === [Section: UI Controls] ===
    QCheckBox      *m_chkStationType = nullptr;
    QComboBox      *m_stationTypeCombo = nullptr;
    QLineEdit      *m_stationIdEdit = nullptr;
    QCheckBox      *m_chkStationLabel = nullptr;
    QLineEdit      *m_stationLabelEdit = nullptr;
    QCheckBox      *m_chkStationX = nullptr;
    QDoubleSpinBox *m_stationXSpin = nullptr;
    QCheckBox      *m_chkStationY = nullptr;
    QDoubleSpinBox *m_stationYSpin = nullptr;
    QCheckBox      *m_chkStationYaw = nullptr;
    QDoubleSpinBox *m_stationYawSpin = nullptr;
    QCheckBox      *m_chkStationDockable = nullptr;
    QCheckBox      *m_stationDockableChk = nullptr;
    QCheckBox      *m_chkStationRotate = nullptr;
    QCheckBox      *m_stationRotateChk = nullptr;
    QCheckBox      *m_chkStationPgv = nullptr;
    QCheckBox      *m_stationPgvChk = nullptr;

    // EPC calibration display
    QWidget        *m_stationEpcGroup = nullptr;
    QLabel         *m_stationEpcXLabel = nullptr;
    QLabel         *m_stationEpcYLabel = nullptr;
};
