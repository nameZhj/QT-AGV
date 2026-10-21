/**
 * @file    MapViewRightPanel.cpp
 * @layer   GUI
 * @brief   Sidebar tabs, element list filter, layer checkboxes, and sub-panel coordination.
 * @deps    MapViewRightPanel.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapViewRightPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QJsonObject>

MapViewRightPanel::MapViewRightPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void MapViewRightPanel::setupUI()
{
    auto *outerLay = new QVBoxLayout(this);
    outerLay->setContentsMargins(0, 0, 0, 0); outerLay->setSpacing(0);

    auto *tabBar = new QWidget(this);
    tabBar->setFixedHeight(30);
    tabBar->setStyleSheet("background:#171b21; border-bottom:1px solid #2d3340;");
    auto *tabLay = new QHBoxLayout(tabBar);
    tabLay->setContentsMargins(6, 3, 6, 3); tabLay->setSpacing(4);

    auto mkTab = [&](const QString &text, int idx) -> QPushButton* {
        auto *b = new QPushButton(text, tabBar);
        b->setCheckable(true); b->setFocusPolicy(Qt::NoFocus); b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(
            "QPushButton{background:transparent; color:#8898a8; border:none; border-radius:3px;"
            "padding:3px 10px; font-size:12px;}"
            "QPushButton:hover{background:rgba(255,255,255,12);}"
            "QPushButton:checked{background:#22303f; color:#d8e4f0; font-weight:600;}");
        connect(b, &QPushButton::clicked, this, [this, idx]() { setRightTab(idx); });
        tabLay->addWidget(b);
        return b;
    };

    m_tabPropsBtn    = mkTab("属性", TabProps);
    m_tabElementsBtn = mkTab("图元", TabElements);
    m_tabLayersBtn   = mkTab("图层", TabLayers);
    tabLay->addStretch();
    outerLay->addWidget(tabBar);

    m_rightStack = new QStackedWidget(this);
    m_rightStack->setStyleSheet("background: transparent;");
    outerLay->addWidget(m_rightStack, 1);

    // Tab 0: Props Page
    auto *propsPage = new QWidget;
    auto *propsLay  = new QVBoxLayout(propsPage);
    propsLay->setContentsMargins(0, 0, 0, 0); propsLay->setSpacing(0);

    auto *scrollArea = new QScrollArea(propsPage);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setStyleSheet(
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical { background: #12151a; width: 6px; }"
        "QScrollBar::handle:vertical { background: #2d3340; border-radius: 3px; min-height: 20px; }"
        "QScrollBar::handle:vertical:hover { background: #3d4758; }"
        "QScrollBar:horizontal { background: #12151a; height: 6px; }"
        "QScrollBar::handle:horizontal { background: #2d3340; border-radius: 3px; min-width: 20px; }"
        "QScrollBar::handle:horizontal:hover { background: #3d4758; }");

    auto *container = new QWidget;
    container->setStyleSheet("background: transparent;");
    auto *contLay = new QVBoxLayout(container);
    contLay->setContentsMargins(10, 10, 10, 10); contLay->setSpacing(10);

    m_devicePanel  = new MapViewDevicePropsPanel(container);
    m_stationPanel = new MapViewStationPropsPanel(container);
    m_routePanel   = new MapViewRoutePropsPanel(container);
    m_mapPanel     = new MapViewMapPropsPanel(container);

    contLay->addWidget(m_devicePanel);
    contLay->addWidget(m_stationPanel);
    contLay->addWidget(m_routePanel);
    contLay->addWidget(m_mapPanel);
    contLay->addStretch();

    scrollArea->setWidget(container);
    propsLay->addWidget(scrollArea);
    m_rightStack->addWidget(propsPage);

    // Tab 1: Element Search Page
    auto *elemPage = new QWidget;
    auto *elemLay  = new QVBoxLayout(elemPage);
    elemLay->setContentsMargins(8, 8, 8, 8); elemLay->setSpacing(6);

    m_elemSearchEdit = new QLineEdit(elemPage);
    m_elemSearchEdit->setPlaceholderText("输入搜索关键字");
    m_elemSearchEdit->setClearButtonEnabled(true);
    m_elemSearchEdit->setStyleSheet(
        "QLineEdit{background:#12151a; border:1px solid #2d3340; border-radius:3px;"
        " padding:3px 6px; color:#c8d0d8; font-size:12px;}"
        "QLineEdit:focus{border:1px solid #2563eb;}");
    elemLay->addWidget(m_elemSearchEdit);

    m_elemList = new QListWidget(elemPage);
    m_elemList->setStyleSheet(
        "QListWidget{background:#12151a; border:1px solid #2d3340; border-radius:3px;"
        " color:#c8d0d8; font-size:12px; outline:none;}"
        "QListWidget::item{padding:3px 5px;}"
        "QListWidget::item:selected{background:#1e3a5f; color:#d8e4f0;}");
    elemLay->addWidget(m_elemList, 1);
    m_rightStack->addWidget(elemPage);

    // Tab 2: Layers Page
    auto *layerPage = new QWidget;
    auto *layerLay  = new QVBoxLayout(layerPage);
    layerLay->setContentsMargins(8, 8, 8, 8); layerLay->setSpacing(6);

    auto mkLayerChk = [&](const QString &text) {
        auto *c = new QCheckBox(text, layerPage);
        c->setStyleSheet("QCheckBox{color:#c8d0d8; font-size:12px; padding:2px 0;}");
        layerLay->addWidget(c);
        return c;
    };
    m_hideSelectedChk = mkLayerChk("隐藏选中图元");
    m_hideStationsChk = mkLayerChk("隐藏所有站点");
    m_hideRoutesChk   = mkLayerChk("隐藏所有线路");

    m_showAllElemsBtn = new QPushButton("显示所有图元", layerPage);
    m_showAllElemsBtn->setCursor(Qt::PointingHandCursor);
    m_showAllElemsBtn->setStyleSheet(
        "QPushButton{background:#1e2530; color:#c8d0d8; border:1px solid #2d3340;"
        " border-radius:3px; padding:4px 8px; font-size:12px;}"
        "QPushButton:hover{background:#26303c;}");
    layerLay->addWidget(m_showAllElemsBtn);
    layerLay->addStretch();
    m_rightStack->addWidget(layerPage);

    connect(m_elemSearchEdit, &QLineEdit::textChanged, this, [this](const QString &kw) {
        for (int i = 0; i < m_elemList->count(); ++i) {
            QListWidgetItem *it = m_elemList->item(i);
            it->setHidden(!kw.isEmpty() && !it->text().contains(kw, Qt::CaseInsensitive));
        }
    });
    connect(m_elemList, &QListWidget::itemClicked, this, [this](QListWidgetItem *it) {
        if (it) emit featureClicked(it->data(Qt::UserRole).toString(), it->data(Qt::UserRole + 1).toString());
    });
    connect(m_hideSelectedChk, &QCheckBox::toggled, this, &MapViewRightPanel::hideSelectedToggled);
    connect(m_hideStationsChk, &QCheckBox::toggled, this, &MapViewRightPanel::layerVisibilityChanged);
    connect(m_hideRoutesChk,   &QCheckBox::toggled, this, &MapViewRightPanel::layerVisibilityChanged);
    connect(m_showAllElemsBtn, &QPushButton::clicked, this, &MapViewRightPanel::showAllElementsRequested);

    setRightTab(TabProps);
}

void MapViewRightPanel::setRightTab(int tab)
{
    m_rightTab = tab;
    m_rightStack->setCurrentIndex(tab);
    m_tabPropsBtn->setChecked(tab == TabProps);
    m_tabElementsBtn->setChecked(tab == TabElements);
    m_tabLayersBtn->setChecked(tab == TabLayers);

    if (tab == TabProps) switchPropsView(m_propsView);
    emit tabChanged(tab);
}

void MapViewRightPanel::switchPropsView(int view)
{
    m_propsView = view;
    const bool onProps = (m_rightTab == TabProps);
    m_devicePanel->setVisible(onProps && (view == 0));
    m_devicePanel->setViewVisible(onProps && (view == 0));
    m_stationPanel->setVisible(onProps && (view == 1));
    m_routePanel->setVisible(onProps && (view == 2 || view == 4));
    if (view == 2) m_routePanel->showRouteView();
    if (view == 4) m_routePanel->showAreaView();
    m_mapPanel->setVisible(onProps && (view == 3));
}

void MapViewRightPanel::refreshElementList(const QJsonArray &pts, const QJsonArray &hp,
                                          const QJsonArray &chg, const QJsonArray &lines,
                                          const QJsonArray &curves, const QJsonArray &areas,
                                          const std::function<QString(const QJsonObject &)> &routeNameFn)
{
    m_elemList->clear();
    int n = 0;
    auto add = [&](const QJsonObject &o, const QString &type, const QString &disp) {
        ++n;
        auto *it = new QListWidgetItem(QString("%1  %2").arg(n).arg(disp));
        it->setData(Qt::UserRole, type);
        it->setData(Qt::UserRole + 1, o.value("id").toString());
        m_elemList->addItem(it);
    };
    auto label = [](const QJsonObject &o) {
        QString name = o.value("name").toString();
        return name.isEmpty() ? o.value("id").toString() : name;
    };

    for (const auto &v : pts)    { QJsonObject o = v.toObject(); add(o, "point", label(o)); }
    for (const auto &v : hp)     { QJsonObject o = v.toObject(); add(o, "highPrecision", label(o)); }
    for (const auto &v : chg)    { QJsonObject o = v.toObject(); add(o, "charge", label(o)); }
    for (const auto &v : lines)  { QJsonObject o = v.toObject(); add(o, "line", routeNameFn(o)); }
    for (const auto &v : curves) { QJsonObject o = v.toObject(); add(o, "curve", routeNameFn(o)); }
    for (const auto &v : areas) {
        QJsonObject o = v.toObject();
        add(o, "highLevelArea", o.value("name").toString("AdvancedArea") + " (高级区域)");
    }

    const QString kw = m_elemSearchEdit ? m_elemSearchEdit->text() : QString();
    for (int i = 0; i < m_elemList->count(); ++i) {
        QListWidgetItem *it = m_elemList->item(i);
        it->setHidden(!kw.isEmpty() && !it->text().contains(kw, Qt::CaseInsensitive));
    }
}

void MapViewRightPanel::syncElementListSelection(const QString &type, const QString &id)
{
    for (int i = 0; i < m_elemList->count(); ++i) {
        QListWidgetItem *it = m_elemList->item(i);
        if (it->data(Qt::UserRole).toString() == type && it->data(Qt::UserRole + 1).toString() == id) {
            m_elemList->setCurrentItem(it);
            return;
        }
    }
}

void MapViewRightPanel::selectFeature(const QString &type, const QString &id,
                                      const QJsonArray &pts, const QJsonArray &hp,
                                      const QJsonArray &chg, const QJsonArray &lines,
                                      const QJsonArray &curves, const QJsonArray &areas,
                                      const std::function<QString(const QString &)> &nameLookup)
{
    auto findObj = [](const QJsonArray &arr, const QString &fid) {
        for (const auto &v : arr) if (v.toObject().value("id").toString() == fid) return v.toObject();
        return QJsonObject();
    };
    if (type == "highLevelArea") {
        m_routePanel->showAreaDetails(findObj(areas, id));
        switchPropsView(4);
    } else if (type == "line" || type == "curve") {
        m_routePanel->showRouteDetails(findObj(type == "line" ? lines : curves, id), type, nameLookup);
        switchPropsView(2);
    } else {
        const QJsonArray &arr = (type == "charge") ? chg : ((type == "highPrecision") ? hp : pts);
        m_stationPanel->showStationDetails(findObj(arr, id), type);
        switchPropsView(1);
    }
    syncElementListSelection(type, id);
}

void MapViewRightPanel::clearSelection()
{
    m_stationPanel->clearStationDetails();
    m_routePanel->clearAll();
}

void MapViewRightPanel::resetLayerCheckboxes()
{
    if (m_hideSelectedChk) m_hideSelectedChk->setChecked(false);
    if (m_hideStationsChk) m_hideStationsChk->setChecked(false);
    if (m_hideRoutesChk)   m_hideRoutesChk->setChecked(false);
}
