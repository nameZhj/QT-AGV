/**
 * @file    MapLinePropsPanel.h
 * @layer   GUI
 * @brief   Property editing panel for straight lines and Bezier curves.
 * @deps    QWidget, QJsonObject, MapEditDataModel, MapMotionFormGroup
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QWidget>
#include <QJsonObject>

class QComboBox;
class QLineEdit;
class QCheckBox;
class QLabel;
class QPushButton;
class MapEditDataModel;
class MapMotionFormGroup;

class MapLinePropsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit MapLinePropsPanel(QWidget *parent = nullptr);
    ~MapLinePropsPanel() override = default;

    void populate(const QJsonObject &obj, const QString &type, const QString &id);
    void applyEdit(MapEditDataModel *model, const QString &selType, const QString &selId);
    void syncMotionSpinBoxRanges();
    void clear();

private:
    void setupUI();

    // === [Section: UI Controls] ===
    QComboBox          *m_lineTypeCombo = nullptr;
    QLineEdit          *m_lineIdEdit = nullptr;
    QCheckBox          *m_chkLineName = nullptr;
    QLineEdit          *m_lineNameEdit = nullptr;
    QLabel             *m_lineStartLabel = nullptr;
    QLabel             *m_lineEndLabel = nullptr;

    QCheckBox          *m_chkLineDirect = nullptr;
    QComboBox          *m_lineDirectCombo = nullptr;
    QPushButton        *m_lineFlipDirBtn = nullptr;

    MapMotionFormGroup *m_motionGroup = nullptr;
};
