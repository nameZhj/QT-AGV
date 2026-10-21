/**
 * @file    MapPropsManager.h
 * @layer   GUI
 * @brief   Composite property manager widget coordinating station, line, and area inspector panels.
 * @deps    QWidget, QJsonObject, MapEditDataModel
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QWidget>
#include <QJsonObject>

class QLabel;
class QPushButton;
class MapStationPropsPanel;
class MapLinePropsPanel;
class MapAreaPropsPanel;
class MapEditDataModel;

class MapPropsManager : public QWidget
{
    Q_OBJECT

public:
    explicit MapPropsManager(QWidget *parent = nullptr);
    ~MapPropsManager() override = default;

    void showProperties(const QString &type, const QString &id, const QJsonObject &obj);
    void applyEdit(MapEditDataModel *model);
    void syncMotionSpinBoxRanges();
    void clearSelection();

    QString selectedType() const { return m_selType; }
    QString selectedId() const { return m_selId; }

signals:
    void applyClicked();
    void deleteClicked();
    void featureUpdated(const QString &type, const QString &id);

private:
    void setupUI();
    void updateBadge(const QString &type);

    QLabel               *m_propHeaderTitle = nullptr;
    QLabel               *m_propHeaderBadge = nullptr;
    QPushButton          *m_propApplyBtn = nullptr;
    QPushButton          *m_propDeleteBtn = nullptr;

    MapStationPropsPanel *m_stationProps = nullptr;
    MapLinePropsPanel    *m_lineProps = nullptr;
    MapAreaPropsPanel    *m_areaProps = nullptr;

    QString               m_selType;
    QString               m_selId;
};
