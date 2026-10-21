/**
 * @file    MapEditWidget.cpp
 * @layer   GUI
 * @brief   Topological map editor shell coordinating canvas, toolbars, and inspector panels.
 * @deps    MapEditWidget.h, MapCanvas.h, MapEditDataModel.h, MapSceneRenderer.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapEditWidget.h"
#include "MapCanvas.h"
#include "MapViewWidget.h"
#include "RulerWidget.h"
#include "DirectionPadWidget.h"
#include "MapEditDataModel.h"
#include "MapCurveHandleManager.h"
#include "MapSceneRenderer.h"
#include "MapDrawController.h"
#include "MapDragController.h"
#include "MapRasterPaintController.h"
#include "MapInteractionHandler.h"
#include "MapPropsManager.h"
#include "MapFeatureTablePanel.h"
#include "MapEditTelemetryController.h"
#include "MapEditDataLoader.h"
#include "MapEditHelpDialog.h"
#include "MapEditDialogs.h"
#include "MapEditKeyHandler.h"
#include "MapEditLayoutHelper.h"
#include "MapSmapImporter.h"
#include "MapEditStyle.h"
#include "core/RouteNetworkValidator.h"
#include "Toast.h"

#include <QVBoxLayout>
#include <QGraphicsScene>
#include <QFileDialog>
#include <QTimer>

using namespace MapEdit;

MapEditWidget::MapEditWidget(const QString &mapId, QWidget *parent)
    : QWidget(parent), m_mapId(mapId)
{
    m_model = new MapEditDataModel(this);
    m_drawCtrl = new MapDrawController(this);
    m_dragCtrl = new MapDragController(this);
    m_paintCtrl = new MapRasterPaintController(this);
    m_interactionHandler = new MapInteractionHandler(this);
    m_telemetryCtrl = new MapEditTelemetryController(this);

    setupUI();
    loadData();
}

MapEditWidget::~MapEditWidget()
{
    if (m_telemetryCtrl) m_telemetryCtrl->disconnectMqtt();
}

bool MapEditWidget::checkUnsavedChanges()
{
    return MapEditDialogs::checkUnsavedChanges(this, m_dirty,
        [this]() { onSaveMap([this](bool ok) { if (ok) emit goBack(); }); },
        [this]() { m_dirty = false; });
}

void MapEditWidget::setupUI()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0); root->setSpacing(0);

    m_topWidgets = MapEditToolBarHelper::setupTopBar(this, root);
    m_subWidgets = MapEditToolBarHelper::setupSubToolBars(this, root);

    auto ui = MapEditLayoutHelper::buildCentralUI(this, root);
    m_leftWidgets = ui.leftWidgets;
    m_scene = ui.scene;
    m_mapView = ui.mapView;
    m_topRuler = ui.topRuler;
    m_leftRuler = ui.leftRuler;
    m_coordLabel = ui.coordLabel;
    m_directionPad = ui.directionPad;
    m_propsMgr = ui.propsMgr;
    m_tablePanel = ui.tablePanel;

    auto w2s = [this](double x, double y) { return worldToScene(x, y); };
    auto s2w = [this](double x, double y) { return sceneToWorld(x, y); };
    auto lookup = [this](const QString &k) { return m_renderer ? m_renderer->getFeatureItem(k) : nullptr; };

    m_handleMgr = new MapCurveHandleManager(m_scene, m_model, w2s, s2w, lookup, this);
    m_renderer = new MapSceneRenderer(m_scene, m_model, w2s, this);
    m_renderer->setCurveHandleManager(m_handleMgr);
    m_drawCtrl->setScene(m_scene); m_drawCtrl->setModel(m_model); m_drawCtrl->setTransformFns(w2s, s2w);
    m_dragCtrl->setScene(m_scene); m_dragCtrl->setView(m_mapView); m_dragCtrl->setModel(m_model);
    m_dragCtrl->setHandleManager(m_handleMgr); m_dragCtrl->setTransformFns(w2s, s2w);
    m_interactionHandler->setView(m_mapView);
    m_interactionHandler->setControllers(m_drawCtrl, m_dragCtrl, m_paintCtrl);
    m_telemetryCtrl->init(m_scene, w2s);
    m_telemetryCtrl->setMapId(m_mapId);

    m_mapView->viewport()->installEventFilter(this);
    m_mapView->viewport()->setMouseTracking(true);

    MapEditLayoutHelper::wireSignals(this, m_topWidgets, m_subWidgets, ui,
                                     m_model, m_drawCtrl, m_dragCtrl, m_paintCtrl, m_telemetryCtrl);
}

void MapEditWidget::loadData()
{
    MapEditDataLoader::loadRasterMap(m_mapId, [this](const MapRasterData &data) {
        if (!data.ok) return;
        m_mapImage = data.image; m_originalMapImage = data.image; m_mapName = data.mapName;
        m_worldL = data.worldL; m_worldB = data.worldB; m_worldR = data.worldR; m_worldT = data.worldT;
        m_imgW = data.imgW; m_imgH = data.imgH;

        m_topWidgets.mapInfoLabel->setText(QString("路网编辑  %1").arg(m_mapName));
        m_scene->setSceneRect(0, 0, m_imgW, m_imgH);
        m_renderer->setBackgroundPixmap(QPixmap::fromImage(m_mapImage));
        m_telemetryCtrl->setMapDimensions(m_worldL, m_worldR, m_worldB, m_worldT, m_imgW, m_imgH);
        m_interactionHandler->setRasterContext(&m_mapImage, &m_mapBgItem, &m_brushSize);

        MapEditDataLoader::loadRouteData(m_model, [this](bool ok) {
            if (ok) { m_dirty = false; redrawMap(); QTimer::singleShot(0, this, [this]() { fitView(); }); }
        });
        MapEditDataLoader::loadReflectorData(m_mapId, m_model, [this](bool ok) { if (ok) redrawMap(); });
        m_telemetryCtrl->loadCarList();
    });
}

void MapEditWidget::redrawMap()
{
    if (m_imgW <= 0 || m_imgH <= 0) return;
    m_renderer->renderMap(m_mapImage, m_imgW, m_imgH, m_propsMgr->selectedType(), m_propsMgr->selectedId(),
                          m_showReflectors, m_drawStatus == Drag);
    m_renderer->updateGrid(m_showGrid, m_worldL, m_worldB, m_worldR, m_worldT, m_imgW, m_imgH);
    if (m_tablePanel->isVisible()) m_tablePanel->refresh(m_model);
    updateRulers();
}

void MapEditWidget::onSaveMap(std::function<void(bool success)> onFinished)
{
    MapEditDataLoader::executeSavePipeline(m_mapId, m_model, m_handleMgr, m_mapImage, m_originalMapImage,
                                           m_paintCtrl->canUndo(), this, [this, onFinished](bool ok) {
        if (ok) {
            m_dirty = false; m_originalMapImage = m_mapImage; m_paintCtrl->clearHistory();
            if (onFinished) onFinished(true);
            else MapEditDataLoader::loadRouteData(m_model, [this](bool) { redrawMap(); });
        } else if (onFinished) onFinished(false);
    });
}

void MapEditWidget::onImportSmap()
{
    QString fp = QFileDialog::getOpenFileName(this, "导入 .smap 地图路网文件", QString(), "SMAP 地图文件 (*.smap);;所有文件 (*.*)");
    if (fp.isEmpty()) return;
    auto res = MapSmapImporter::importFromFile(fp, m_model, m_worldL, m_worldB, m_worldR, m_worldT);
    if (!res.ok) { Toast::error(this, QString("导入失败: %1").arg(res.errorMessage)); return; }
    m_dirty = true; redrawMap();
    Toast::success(this, QString("成功导入 %1 个要素").arg(res.pointCount + res.lineCount + res.curveCount));
}

void MapEditWidget::onUndo() { m_mapImage = m_paintCtrl->undo(m_mapImage); m_renderer->setBackgroundPixmap(QPixmap::fromImage(m_mapImage)); }
void MapEditWidget::onRedo() { m_mapImage = m_paintCtrl->redo(m_mapImage); m_renderer->setBackgroundPixmap(QPixmap::fromImage(m_mapImage)); }
void MapEditWidget::onEditMap() { MapEditDialogs::showEditMapDialog(m_mapId, m_mapName, m_topWidgets.mapInfoLabel, this); }
void MapEditWidget::onTableToggle() { bool v = m_leftWidgets.tableModeBtn->isChecked(); m_tablePanel->setVisible(v); if (v) m_tablePanel->refresh(m_model); }
void MapEditWidget::showHelpDialog() { MapEditHelpDialog::showHelp(this); }
void MapEditWidget::onGridToggled(bool on) { m_showGrid = on; redrawMap(); }
void MapEditWidget::onReflectorToggled(bool on) { m_showReflectors = on; redrawMap(); }
void MapEditWidget::onDataChanged() { m_dirty = true; redrawMap(); }
void MapEditWidget::applyPropsEdit() { m_propsMgr->applyEdit(m_model); redrawMap(); }
void MapEditWidget::deleteFeatureFromTable(const QString &t, const QString &id) { m_model->removeFeature(t, id); redrawMap(); }
void MapEditWidget::onAreaBoundsChanged() { showFeatureProps(m_propsMgr->selectedType(), m_propsMgr->selectedId()); }
void MapEditWidget::cancelDraw() { m_drawCtrl->cancelDraw(); }
void MapEditWidget::syncMotionSpinBoxRanges() { m_propsMgr->syncMotionSpinBoxRanges(); }
void MapEditWidget::onTeleopActiveChanged(bool active) { m_telemetryCtrl->onTeleopActiveChanged(active, m_topWidgets.teleopBtn); }

void MapEditWidget::deleteSelected()
{
    QString t = m_propsMgr->selectedType(), id = m_propsMgr->selectedId();
    if (t.isEmpty() || id.isEmpty()) return;
    m_model->removeFeature(t, id);
    clearSelection(); redrawMap();
    Toast::success(this, "已删除图元");
}

void MapEditWidget::clearSelection()
{
    m_propsMgr->clearSelection();
    m_interactionHandler->setSelectedFeature(QString(), QString());
    redrawMap();
}

void MapEditWidget::setDrawMode(int mode)
{
    cancelDraw();
    m_drawStatus = mode;
    m_drawCtrl->setDrawMode(mode);
    for (auto it = m_leftWidgets.toolBtns.begin(); it != m_leftWidgets.toolBtns.end(); ++it) it.value()->setChecked(it.key() == mode);
    bool isStation = (mode == Point || mode == HighPrecision || mode == Charge);
    if (m_subWidgets.stationBar) m_subWidgets.stationBar->setVisible(isStation);
    for (auto it = m_subWidgets.stationBtns.begin(); it != m_subWidgets.stationBtns.end(); ++it) it.value()->setChecked(it.key() == mode);
    bool isArea = (mode == Area);
    if (m_subWidgets.areaBar) m_subWidgets.areaBar->setVisible(isArea);
    if (m_subWidgets.areaAdvBtn) m_subWidgets.areaAdvBtn->setChecked(isArea);
    m_handleMgr->setHandlesVisible(mode == Drag);
    m_mapView->viewport()->setCursor(mode == Drag ? Qt::OpenHandCursor : Qt::CrossCursor);
}

void MapEditWidget::fitView()
{
    QRectF r = m_scene->sceneRect(), vr = m_mapView->viewport()->rect();
    if (r.isEmpty() || vr.isEmpty()) return;
    m_mapView->resetTransform();
    double s = qMin(vr.width() / r.width(), vr.height() / r.height());
    m_mapView->scale(s, s); m_mapView->centerOn(r.center());
    m_hasInitialFit = true; updateRulers();
}

void MapEditWidget::updateRulers()
{
    if (m_topRuler) m_topRuler->syncWithView(m_mapView, m_worldL, m_worldB, m_worldR, m_worldT, m_imgW, m_imgH);
    if (m_leftRuler) m_leftRuler->syncWithView(m_mapView, m_worldL, m_worldB, m_worldR, m_worldT, m_imgW, m_imgH);
}

void MapEditWidget::showFeatureProps(const QString &type, const QString &id)
{
    m_interactionHandler->setSelectedFeature(type, id);
    m_propsMgr->showProperties(type, id, m_model->findFeature(type, id));
    redrawMap();
}

void MapEditWidget::focusFeature(const QString &type, const QString &id)
{
    showFeatureProps(type, id);
    if (auto *item = m_renderer->getFeatureItem(id)) m_mapView->centerOn(item);
}

QPointF MapEditWidget::worldToScene(double wx, double wy) const
{
    double mW = m_worldR - m_worldL, mH = m_worldT - m_worldB;
    return (m_imgW <= 0 || m_imgH <= 0 || mW <= 0 || mH <= 0) ? QPointF(wx, wy) : QPointF((wx - m_worldL) / mW * m_imgW, (m_worldT - wy) / mH * m_imgH);
}

QPointF MapEditWidget::sceneToWorld(double sx, double sy) const
{
    double mW = m_worldR - m_worldL, mH = m_worldT - m_worldB;
    return (m_imgW <= 0 || m_imgH <= 0 || mW <= 0 || mH <= 0) ? QPointF(sx, sy) : QPointF(sx / m_imgW * mW + m_worldL, m_worldT - sy / m_imgH * mH);
}

bool MapEditWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_mapView->viewport()) {
        if (event->type() == QEvent::MouseMove) {
            auto *me = static_cast<QMouseEvent*>(event);
            QPointF wp = sceneToWorld(m_mapView->mapToScene(me->pos()).x(), m_mapView->mapToScene(me->pos()).y());
            m_coordLabel->setText(QString("X: %1  Y: %2").arg(wp.x(), 0, 'f', 2).arg(wp.y(), 0, 'f', 2));
            m_coordLabel->adjustSize();
            m_coordLabel->move(8, m_mapView->viewport()->height() - m_coordLabel->height() - 8);
        }
        return m_interactionHandler->handleViewportEvent(watched, event);
    }
    return QWidget::eventFilter(watched, event);
}

void MapEditWidget::keyPressEvent(QKeyEvent *event)
{
    MapEditKeyHandler::Handlers h;
    h.onDelete = [this]() { deleteSelected(); };
    h.onEscape = [this]() { cancelDraw(); clearSelection(); setDrawMode(Drag); };
    h.onSetMode = [this](int m) { setDrawMode(m); };
    h.onHelp = [this]() { showHelpDialog(); };
    h.getCurrentMode = [this]() { return m_drawStatus; };

    if (!MapEditKeyHandler::processKeyPress(event, h)) QWidget::keyPressEvent(event);
}

void MapEditWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_directionPad && m_mapView) {
        m_directionPad->move(16, qMax(16, m_mapView->height() - m_directionPad->height() - 16));
        m_directionPad->raise();
    }
    if (!m_hasInitialFit && m_imgW > 0 && m_imgH > 0) {
        QTimer::singleShot(50, this, [this]() { fitView(); });
    }
}
