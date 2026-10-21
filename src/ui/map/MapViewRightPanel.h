/**
 * @file    MapViewRightPanel.h
 * @layer   GUI
 * @brief   Right collapsible sidebar hosting Property tabs, Element search lists, and Layer checkboxes.
 * @deps    QWidget, MapViewDevicePropsPanel.h, MapViewStationPropsPanel.h, MapViewRoutePropsPanel.h, MapViewMapPropsPanel.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QWidget>
#include <QStackedWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QListWidget>
#include <QCheckBox>
#include <QJsonArray>
#include <functional>
#include "MapViewDevicePropsPanel.h"
#include "MapViewStationPropsPanel.h"
#include "MapViewRoutePropsPanel.h"
#include "MapViewMapPropsPanel.h"

class MapViewRightPanel : public QWidget
{
    Q_OBJECT
public:
    enum RightTab { TabProps = 0, TabElements = 1, TabLayers = 2 };

    explicit MapViewRightPanel(QWidget *parent = nullptr);
    ~MapViewRightPanel() override = default;

    MapViewDevicePropsPanel* devicePanel() const { return m_devicePanel; }
    MapViewStationPropsPanel* stationPanel() const { return m_stationPanel; }
    MapViewRoutePropsPanel* routePanel() const { return m_routePanel; }
    MapViewMapPropsPanel* mapPanel() const { return m_mapPanel; }

    int currentTab() const { return m_rightTab; }
    void setRightTab(int tab);
    void switchPropsView(int view);

    void refreshElementList(const QJsonArray &pts, const QJsonArray &hp,
                            const QJsonArray &chg, const QJsonArray &lines,
                            const QJsonArray &curves, const QJsonArray &areas,
                            const std::function<QString(const QJsonObject &)> &routeNameFn);
    void syncElementListSelection(const QString &type, const QString &id);

    void selectFeature(const QString &type, const QString &id,
                       const QJsonArray &pts, const QJsonArray &hp,
                       const QJsonArray &chg, const QJsonArray &lines,
                       const QJsonArray &curves, const QJsonArray &areas,
                       const std::function<QString(const QString &)> &nameLookup);
    void clearSelection();

    bool isHideStationsChecked() const { return m_hideStationsChk && m_hideStationsChk->isChecked(); }
    bool isHideRoutesChecked() const { return m_hideRoutesChk && m_hideRoutesChk->isChecked(); }
    bool isHideSelectedChecked() const { return m_hideSelectedChk && m_hideSelectedChk->isChecked(); }
    void resetLayerCheckboxes();

signals:
    void tabChanged(int tab);
    void featureClicked(const QString &type, const QString &id);
    void hideSelectedToggled(bool checked);
    void layerVisibilityChanged();
    void showAllElementsRequested();

private:
    void setupUI();

    int             m_rightTab  = TabProps;
    int             m_propsView = 0;

    QPushButton    *m_tabPropsBtn    = nullptr;
    QPushButton    *m_tabElementsBtn = nullptr;
    QPushButton    *m_tabLayersBtn   = nullptr;
    QStackedWidget *m_rightStack     = nullptr;

    MapViewDevicePropsPanel  *m_devicePanel  = nullptr;
    MapViewStationPropsPanel *m_stationPanel = nullptr;
    MapViewRoutePropsPanel   *m_routePanel   = nullptr;
    MapViewMapPropsPanel     *m_mapPanel     = nullptr;

    QLineEdit   *m_elemSearchEdit = nullptr;
    QListWidget *m_elemList       = nullptr;

    QCheckBox   *m_hideSelectedChk = nullptr;
    QCheckBox   *m_hideStationsChk = nullptr;
    QCheckBox   *m_hideRoutesChk   = nullptr;
    QPushButton *m_showAllElemsBtn = nullptr;
};
