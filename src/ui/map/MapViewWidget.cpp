/**
 * @file    MapViewWidget.cpp
 * @layer   GUI
 * @brief   Main coordinator widget for 2D robot map viewing, telemetry, and navigation.
 * @deps    MapViewWidget.h, MapViewDataLoader.h, MapViewSceneRenderer.h, MapViewDialogHelper.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapViewWidget.h"
#include "app/AppConfig.h"
#include "core/TeleopController.h"
#include "core/RouteFeatureSelect.h"
#include "MapViewCanvasHandler.h"
#include "MapViewDataLoader.h"
#include "MapViewMarkHelper.h"
#include "MapViewNavInteraction.h"
#include "MapViewRelocController.h"
#include "MapViewRobotMarker.h"
#include "MapViewSceneRenderer.h"
#include "MapViewDialogHelper.h"
#include "RobotStatusDialog.h"
#include "AudioManagerDialog.h"
#include "ui/common/Toast.h"
#include "MapCanvas.h"
#include <QVBoxLayout>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QCoreApplication>
#include <QDir>

MapViewWidget::MapViewWidget(QWidget *parent) : QWidget(parent)
{
    setupUI();
    connect(m_telemetryCtrl, &MapViewTelemetryController::telemetryUpdated, this,
            [this](const MapViewRobotState &st, const RobotInfo &info) {
        if (m_statusDialog && m_statusDialog->isVisible()) {
            m_statusDialog->updateFromRobotInfo(info);
            m_statusDialog->updateFromRobotState(st.carId, st.wx, st.wy, st.yawRad, st.speed, st.battery, st.covScore, st.posValid);
        }
        if (m_rightPanel) {
            m_rightPanel->devicePanel()->updateCarInfo(st, info);
            m_rightPanel->devicePanel()->updateDeviceMonitor(info, m_mapName);
        }
        if (st.posValid && m_mapReady && m_sceneRenderer && m_robotMarker) {
            double ppm = (m_worldR > m_worldL && m_imgW > 0) ? (m_imgW / (m_worldR - m_worldL)) : 20.0;
            m_robotMarker->updateRobot(m_scene, m_sceneRenderer->worldToScene(st.wx, st.wy), st.yawRad, ppm);
        }
    });

    connect(m_navInteraction, &MapViewNavInteraction::statusHint, this, [this](const QString &m, const QString &c) { setStatus(m, c); });
    connect(m_navInteraction, &MapViewNavInteraction::navButtonsUpdateRequested, this, &MapViewWidget::updateNavButtons);
    connect(&TeleopController::instance(), &TeleopController::activeChanged, this, &MapViewWidget::onTeleopActiveChanged);
    connect(&TeleopController::instance(), &TeleopController::userPreemptedChanged, this, [this](bool) { updateNavButtons(); });
    connect(m_canvasArea.gridBtn, &QPushButton::toggled, this, [this](bool chk) {
        m_showGrid = chk; if (m_sceneRenderer) m_sceneRenderer->renderAxesAndGrid(m_scene, m_showGrid, m_gridItems);
    });
    connect(m_rightPanel, &MapViewRightPanel::featureClicked, this, [this](const QString &t, const QString &id) { selectFeatureOnMap(t, id, true); });
    connect(m_rightPanel, &MapViewRightPanel::layerVisibilityChanged, this, &MapViewWidget::applyLayerVisibility);
    connect(m_rightPanel, &MapViewRightPanel::showAllElementsRequested, this, [this]() {
        m_rightPanel->resetLayerCheckboxes();
        if (m_layerHiddenItem) { m_layerHiddenItem->setVisible(true); m_layerHiddenItem = nullptr; }
        applyLayerVisibility();
    });
}

MapViewWidget::~MapViewWidget()
{
    if (m_telemetryCtrl) m_telemetryCtrl->disconnectMqtt(AppConfig::instance().agvId());
}

void MapViewWidget::setupUI()
{
    m_scene = new QGraphicsScene(this); m_dataLoader = new MapViewDataLoader(this);
    m_sceneRenderer = new MapViewSceneRenderer(this); m_robotMarker = new MapViewRobotMarker(this);
    m_telemetryCtrl = new MapViewTelemetryController(this); m_relocCtrl = new MapViewRelocController(this);
    m_navInteraction = new MapViewNavInteraction(this); m_markHelper = new MapViewMarkHelper(this, this);
    m_canvasHandler = new MapViewCanvasHandler(this);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0); root->setSpacing(0);
    auto *toolbarWidget = new QWidget(this);
    toolbarWidget->setFixedHeight(60);
    toolbarWidget->setStyleSheet("background:#12151a; border-bottom:1px solid #2d3340;");
    m_toolBtns = MapViewToolbarHelper::createToolbar(toolbarWidget, this);
    root->addWidget(toolbarWidget);

    m_statusLabel = new QLabel(QStringLiteral("准备就绪"), this);
    m_statusLabel->setFixedHeight(24);
    m_statusLabel->setStyleSheet("background:#0d1117; color:#8898a8; font-size:11px; padding:0 10px; border-bottom:1px solid #2d3340;");
    root->addWidget(m_statusLabel);

    m_canvasArea = MapViewLayoutHelper::setupCanvasArea(this, m_scene, this);
    m_rightPanel = new MapViewRightPanel(this);
    root->addWidget(MapViewLayoutHelper::setupSplitter(this, m_canvasArea.mapArea, m_rightPanel), 1);
}

void MapViewWidget::clearRouteData()
{
    m_points = QJsonArray(); m_lines = QJsonArray(); m_curves = QJsonArray();
    m_charges = QJsonArray(); m_highPrecisionPoints = QJsonArray(); m_highLevelAreas = QJsonArray();
    if (m_sceneRenderer) m_sceneRenderer->clearRouteItems(m_scene, m_routeTopLevelItems, m_selItem);
    if (!m_rightPanel) return;
    m_rightPanel->refreshElementList(m_points, m_highPrecisionPoints, m_charges, m_lines, m_curves, m_highLevelAreas, [this](const QJsonObject &o) { return routeFeatureName(o); });
    if (m_rightPanel->mapPanel()) m_rightPanel->mapPanel()->updateMapProps(m_mapId, m_mapName, m_imgW, m_imgH, m_worldL, m_worldB, m_worldR, m_worldT, 0, 0, 0, 0, 0, 0);
}

void MapViewWidget::refreshData()
{
    if (!m_dataLoader) return;
    if (m_telemetryCtrl) m_telemetryCtrl->onRobotInfo(RobotInfoManager::instance().currentInfo());
    m_rightPanel->devicePanel()->setViewVisible(true);

    if (m_mapReady && !m_mapId.isEmpty()) {
        m_dataLoader->loadRoute(m_mapId, m_mapName, [this](bool ok, const QString &j) {
            if (!ok || j.trimmed().isEmpty()) return;
            clearRouteData();
            MapViewDialogHelper::unpackRouteJson(j, m_points, m_lines, m_curves, m_charges, m_highPrecisionPoints, m_highLevelAreas);
            renderAll();
        });
    }

    m_dataLoader->fetchMapNameList([this]() {
        m_dataLoader->loadCurrentCarMap([this](const QString &carName, const QString &mapId) {
            bool mapChanged = (m_mapId != mapId);
            m_mapId = mapId; m_mapName = m_dataLoader->lookupMapName(mapId.toInt(), mapId);
            if (m_telemetryCtrl) {
                m_telemetryCtrl->setCarName(carName);
                m_telemetryCtrl->setCarId(AppConfig::instance().agvId());
                m_telemetryCtrl->initMqtt(AppConfig::instance().agvId());
            }
            if (mapChanged) clearRouteData();
            if (m_mapId.isEmpty() || m_mapId == "0") {
                clearRouteData();
                m_mapReady = false; m_mapLoadFailed = false; emit mapStatusChanged(); return;
            }
            m_dataLoader->loadMapById(m_mapId, m_mapName, this,
                [this](const QString &m, const QString &c) { setStatus(m, c); },
                [this](const QString &fn, const QPixmap &pix, double wL, double wB, double wR, double wT) {
                    m_mapName = fn; m_worldL = wL; m_worldB = wB; m_worldR = wR; m_worldT = wT;
                    m_imgW = pix.width(); m_imgH = pix.height();
                    m_mapReady = true; m_mapLoadFailed = false; emit mapStatusChanged();
                    if (m_mapBg) { m_scene->removeItem(m_mapBg); delete m_mapBg; m_mapBg = nullptr; }
                    m_scene->setSceneRect(0, 0, m_imgW, m_imgH);
                    m_mapBg = m_scene->addPixmap(pix); m_mapBg->setZValue(-100);
                    m_dataLoader->loadRoute(m_mapId, m_mapName, [this](bool ok, const QString &j) {
                        if (!ok || j.trimmed().isEmpty()) return;
                        clearRouteData();
                        MapViewDialogHelper::unpackRouteJson(j, m_points, m_lines, m_curves, m_charges, m_highPrecisionPoints, m_highLevelAreas);
                        renderAll(); onResetView();
                    });
                });
        }, [this](const QString &m, const QString &c) { setStatus(m, c); });
    });
}

void MapViewWidget::forceRefreshAfterMapping() {
    clearRouteData();
    if (m_dataLoader) {
        m_dataLoader->clearLoadingState();
        if (!m_mapId.isEmpty()) {
            QDir dir(QCoreApplication::applicationDirPath() + "/maps/" + m_mapId);
            if (dir.exists()) {
                for (const QString &f : dir.entryList({"*.yaml", "*.yml", "*.pgm", "*.zip", "meta.json"}, QDir::Files)) dir.remove(f);
            }
        }
    }
    refreshData();
}

QString MapViewWidget::mapStatusText() const {
    return m_mapReady ? QStringLiteral("成功") : (m_mapLoadFailed ? QStringLiteral("失败") : (m_mapId.isEmpty() ? QStringLiteral("未加载") : QStringLiteral("加载中...")));
}
QString MapViewWidget::mapStatusColor() const {
    return m_mapReady ? "#22c55e" : (m_mapLoadFailed ? "#ef4444" : (m_mapId.isEmpty() ? "#8898a8" : "#eab308"));
}

void MapViewWidget::showEvent(QShowEvent *e) { QWidget::showEvent(e); refreshData(); }
void MapViewWidget::hideEvent(QHideEvent *e) { QWidget::hideEvent(e); }
void MapViewWidget::onCreateMap() { emit navigateRequested("create_map", QString()); }
void MapViewWidget::onUploadMap() { MapViewDialogHelper::showUploadMap(this, [this]() { onRefreshMap(); }); }
void MapViewWidget::onUploadSmap() { MapViewDialogHelper::showUploadSmap(this, [this]() { onRefreshMap(); }); }
void MapViewWidget::onRefreshMap() { refreshData(); setStatus("地图已刷新"); }
void MapViewWidget::onSwitchMap() { MapViewDialogHelper::showSwitchMap(this, m_mapId, [this](const QString &) { clearRouteData(); refreshData(); }); }
void MapViewWidget::onRelocalize() { MapViewDialogHelper::handleRelocalize(m_relocCtrl, m_navInteraction, m_canvasArea.canvas, m_scene, m_toolBtns, [this](const QString &m, const QString &c) { setStatus(m, c); }); }
void MapViewWidget::onToggleNavMode() { MapViewDialogHelper::handleToggleNavMode(m_navInteraction, m_relocCtrl, m_canvasArea.canvas, m_scene, m_toolBtns, [this](const QString &m, const QString &c) { setStatus(m, c); }); updateNavButtons(); }
void MapViewWidget::onToggleMoveMode() { m_navInteraction->toggleMoveMode(this); }
void MapViewWidget::onCancelNavigation() { m_navInteraction->cancelNavigation(); updateNavButtons(); }
void MapViewWidget::onPauseNavigation() { m_navInteraction->pauseOrResumeNavigation(); updateNavButtons(); }
void MapViewWidget::onTeleopClicked() {
    bool next = !TeleopController::instance().isActive();
    TeleopController::instance().setUserPreempted(next);
    TeleopController::instance().setActive(next);
}
void MapViewWidget::onTeleopActiveChanged(bool active) {
    if (m_toolBtns.teleopBtn) {
        m_toolBtns.teleopBtn->setStyleSheet(active ? MapViewToolbarHelper::kToolBtnDanger : MapViewToolbarHelper::kToolBtnStyle);
        if (active) Toast::info(this, "已抢占控制权：WASD 移动 | Space 刹车 | Ctrl 加速 | P/L 升降");
    }
    updateNavButtons();
}
void MapViewWidget::onShowRobotStatus() { MapViewDialogHelper::showRobotStatus(this, m_statusDialog, m_mapId, m_mapName, m_telemetryCtrl->robotState()); }
void MapViewWidget::onShowMoreMenu() {
    if (m_toolBtns.moreMenu && m_toolBtns.moreBtn) m_toolBtns.moreMenu->exec(m_toolBtns.moreBtn->mapToGlobal(QPoint(0, m_toolBtns.moreBtn->height())));
}
void MapViewWidget::onShowAudioManager() { AudioManagerDialog dlg(this); dlg.exec(); }
void MapViewWidget::onResetView() {
    if (!m_canvasArea.canvas || !m_scene) return;
    m_canvasArea.canvas->resetTransform();
    m_canvasArea.canvas->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatioByExpanding);
    onViewTransformChanged();
}
void MapViewWidget::onEditMap() {
    if (m_mapId.isEmpty()) { setStatus("请先加载地图", "#f0a060"); return; }
    emit navigateRequested("map_edit", m_mapId);
}
void MapViewWidget::onDownloadSource() { MapViewDialogHelper::showDownloadSource(m_mapId); }
void MapViewWidget::onShowConfigDialog() { MapViewDialogHelper::showConfig(this, m_configDialog, m_editConfigWidget, [this]() { refreshData(); }); }
void MapViewWidget::onViewTransformChanged() {
    m_canvasHandler->onTransformChanged(m_canvasArea.canvas, m_canvasArea.coordLabel, m_sceneRenderer, [this]() { updateRulers(); });
}
void MapViewWidget::onSceneClicked(const QPointF &p, Qt::MouseButton b) {
    m_canvasHandler->onSceneClicked(p, b, m_canvasArea.canvas, m_scene, m_canvasArea.coordLabel, m_sceneRenderer,
        m_relocCtrl, m_navInteraction, m_markHelper, m_rightPanel->currentTab(),
        [this](const QString &t, const QString &id) { selectFeatureOnMap(t, id); },
        [this]() { clearMapSelection(); }, [this](const QString &m, const QString &c) { setStatus(m, c); },
        [this]() {
            const auto &st = m_telemetryCtrl->robotState();
            m_markHelper->showRobotContextMenu(QCursor::pos(), m_mapId, st.carId, st.posValid, st.wx, st.wy, st.yawRad,
                m_points, m_charges, m_highPrecisionPoints, [this](std::function<void(bool)> done) {
                    QJsonObject geo; m_dataLoader->saveRouteToServer(m_mapId, m_mapName, geo, this, done);
                });
        }, this);
}
void MapViewWidget::onSceneDoubleClicked(const QPointF &p, Qt::MouseButton b) {
    m_canvasHandler->onSceneDoubleClicked(p, b, m_canvasArea.canvas, m_scene, m_sceneRenderer, m_rightPanel->currentTab(),
        [this](const QString &t, const QString &id) { selectFeatureOnMap(t, id, true); }, [this]() { updateRulers(); });
}
void MapViewWidget::onSceneMoved(const QPointF &p) {
    m_canvasHandler->onSceneMoved(p, m_canvasArea.coordLabel, m_sceneRenderer, m_relocCtrl, m_scene);
}
void MapViewWidget::onSceneReleased(const QPointF &p, Qt::MouseButton b) {
    const auto &st = m_telemetryCtrl->robotState();
    m_canvasHandler->onSceneReleased(p, b, m_sceneRenderer, m_relocCtrl, m_scene, st.carId, m_mapId, st.yawRad,
        [this](const QString &m, const QString &c) { setStatus(m, c); }, [this]() { updateNavButtons(); }, this);
}
void MapViewWidget::updateRulers() {
    if (!m_canvasArea.topRuler || !m_canvasArea.leftRuler || !m_canvasArea.canvas) return;
    m_canvasArea.topRuler->syncWithView(m_canvasArea.canvas, m_worldL, m_worldB, m_worldR, m_worldT, m_imgW, m_imgH);
    m_canvasArea.leftRuler->syncWithView(m_canvasArea.canvas, m_worldL, m_worldB, m_worldR, m_worldT, m_imgW, m_imgH);
}
void MapViewWidget::renderAll() {
    if (!m_sceneRenderer) return;
    m_sceneRenderer->setExtent(m_worldL, m_worldB, m_worldR, m_worldT, m_imgW, m_imgH);
    m_sceneRenderer->clearGridItems(m_scene, m_gridItems);
    m_sceneRenderer->renderAxesAndGrid(m_scene, m_showGrid, m_gridItems);
    m_sceneRenderer->clearRouteItems(m_scene, m_routeTopLevelItems, m_selItem);
    m_sceneRenderer->renderRoute(m_scene, m_points, m_lines, m_curves, m_charges, m_highPrecisionPoints, m_highLevelAreas, m_routeTopLevelItems, m_selItem);
    applyLayerVisibility();
    m_rightPanel->refreshElementList(m_points, m_highPrecisionPoints, m_charges, m_lines, m_curves, m_highLevelAreas, [this](const QJsonObject &o) { return routeFeatureName(o); });
    if (m_rightPanel->mapPanel()) m_rightPanel->mapPanel()->updateMapProps(m_mapId, m_mapName, m_imgW, m_imgH, m_worldL, m_worldB, m_worldR, m_worldT, m_points.size(), m_highPrecisionPoints.size(), m_charges.size(), m_lines.size(), m_curves.size(), m_highLevelAreas.size());
    updateRulers();
}
void MapViewWidget::selectFeatureOnMap(const QString &type, const QString &id, bool center) {
    QGraphicsItem *found = nullptr;
    for (QGraphicsItem *it : m_routeTopLevelItems) {
        if (it && it->data(0).toString() == type && it->data(1).toString() == id) { found = it; break; }
    }
    if (!found) return;
    if (found != m_selItem) { RouteFeatureSelect::clearSelectColor(m_selItem); m_selItem = found; }
    found->setVisible(true);
    RouteFeatureSelect::applySelectColor(found);
    m_rightPanel->selectFeature(type, id, m_points, m_highPrecisionPoints, m_charges, m_lines, m_curves, m_highLevelAreas, [this](const QString &sId) { return stationNameById(sId); });
    if (center && m_canvasArea.canvas) { m_canvasArea.canvas->centerOn(found->sceneBoundingRect().center()); updateRulers(); }
}
void MapViewWidget::clearMapSelection() { RouteFeatureSelect::clearSelectColor(m_selItem); m_selItem = nullptr; }
void MapViewWidget::applyLayerVisibility() {
    bool hideStations = m_rightPanel->isHideStationsChecked(), hideRoutes = m_rightPanel->isHideRoutesChecked();
    for (QGraphicsItem *it : m_routeTopLevelItems) {
        if (!it || it == m_layerHiddenItem) continue;
        QString t = it->data(0).toString();
        if (t == "highLevelArea") { it->setVisible(true); continue; }
        it->setVisible((t == "line" || t == "curve") ? !hideRoutes : !hideStations);
    }
}
void MapViewWidget::setStatus(const QString &msg, const QString &color) {
    if (m_statusLabel) {
        m_statusLabel->setText(msg);
        m_statusLabel->setStyleSheet(QString("background:#0d1117; color:%1; font-size:11px; padding:0 10px; border-bottom:1px solid #2d3340;").arg(color));
    }
}
void MapViewWidget::updateNavButtons() {
    MapViewToolbarHelper::updateNavButtonsState(m_toolBtns, m_navInteraction->isNavActive(), m_navInteraction->isNavPaused(),
                                              TeleopController::instance().isUserPreempted(), m_navInteraction->isNavMode());
}
QString MapViewWidget::stationNameById(const QString &id) const { return MapViewDialogHelper::findStationName(id, m_points, m_charges, m_highPrecisionPoints); }
QString MapViewWidget::routeFeatureName(const QJsonObject &obj) const { return MapViewDialogHelper::formatRouteName(obj, [this](const QString &id) { return stationNameById(id); }); }
