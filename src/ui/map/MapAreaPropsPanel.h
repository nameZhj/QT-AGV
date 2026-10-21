/**
 * @file    MapAreaPropsPanel.h
 * @layer   GUI
 * @brief   Property editing panel for high level advanced areas.
 * @deps    QWidget, QJsonObject, MapEditDataModel, MapMotionFormGroup
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QWidget>
#include <QJsonObject>

class QLineEdit;
class QCheckBox;
class QLabel;
class MapEditDataModel;
class MapMotionFormGroup;

class MapAreaPropsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit MapAreaPropsPanel(QWidget *parent = nullptr);
    ~MapAreaPropsPanel() override = default;

    void populate(const QJsonObject &obj);
    void applyEdit(MapEditDataModel *model, const QString &selId);
    void syncMotionSpinBoxRanges();
    void clear();

private:
    void setupUI();

    // === [Section: UI Controls] ===
    QCheckBox          *m_chkAreaName = nullptr;
    QLineEdit          *m_areaNameEdit = nullptr;
    QLabel             *m_areaBoundsLabel = nullptr;

    MapMotionFormGroup *m_motionGroup = nullptr;
};
