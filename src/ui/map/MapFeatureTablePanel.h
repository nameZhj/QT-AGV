/**
 * @file    MapFeatureTablePanel.h
 * @layer   GUI
 * @brief   Feature table widget listing points, lines, curves, and special stations.
 * @deps    QWidget, QTableWidget, MapEditDataModel
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QWidget>

class QComboBox;
class QTableWidget;
class QPushButton;
class MapEditDataModel;

class MapFeatureTablePanel : public QWidget
{
    Q_OBJECT

public:
    explicit MapFeatureTablePanel(QWidget *parent = nullptr);
    ~MapFeatureTablePanel() override = default;

    void refresh(const MapEditDataModel *model);
    void selectFeature(const QString &type, const QString &id);
    QString currentTableType() const;
    void setCurrentTableType(const QString &type);

signals:
    void featureSelected(const QString &type, const QString &id);
    void deleteRequested(const QString &type, const QString &id);

private:
    void setupUI();

    QComboBox    *m_tableTypeCombo = nullptr;
    QTableWidget *m_featureTable = nullptr;
    QPushButton  *m_delBtn = nullptr;
};
