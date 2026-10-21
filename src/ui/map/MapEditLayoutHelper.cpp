/**
 * @file    MapEditLayoutHelper.cpp
 * @layer   GUI
 * @brief   Factory implementation assembling splitters, viewports, and signal routing.
 * @deps    MapEditLayoutHelper.h, MapEditWidget.h, MapCanvas.h, MapViewWidget.h, ToggleSwitch.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapEditLayoutHelper.h"
#include "MapEditWidget.h"
#include "MapCanvas.h"
#include "MapViewWidget.h"
#include "RulerWidget.h"
#include "DirectionPadWidget.h"
#include "ToggleSwitch.h"
#include "MapPropsManager.h"
#include "MapFeatureTablePanel.h"
#include "MapEditDataModel.h"
#include "MapDrawController.h"
#include "MapDragController.h"
#include "MapRasterPaintController.h"
#include "MapEditTelemetryController.h"
#include "Toast.h"
#include "MotionConfigManager.h"
#include "TeleopController.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QGraphicsScene>
#include <QLabel>
#include <QShortcut>

namespace MapEditLayoutHelper {

CentralUI buildCentralUI(QWidget *parent, QVBoxLayout *root)
{
    CentralUI ui;
    auto *content = new QWidget(parent);
    auto *cl = new QHBoxLayout(content);
    cl->setContentsMargins(0, 0, 0, 0); cl->setSpacing(0);

    ui.leftWidgets = MapEditToolBarHelper::setupLeftToolbar(parent, cl);

    auto *splitter = new QSplitter(Qt::Horizontal, content);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(5);
    splitter->setStyleSheet("QSplitter::handle{background:#232733;} QSplitter::handle:hover{background:#3b82f6;}");

    ui.scene = new QGraphicsScene(parent);
    ui.mapView = new MapCanvas(ui.scene, parent);
    ui.mapView->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing);
    ui.mapView->setBackgroundBrush(QColor("#0f1520"));
    ui.mapView->setFrameStyle(QFrame::NoFrame);
    ui.mapView->setDragMode(QGraphicsView::NoDrag);

    ui.coordLabel = new QLabel("X: --  Y: --", ui.mapView);
    ui.coordLabel->setStyleSheet("background:rgba(10,16,26,195); color:#00c8a0; padding:3px 10px; border-radius:3px; font-size:11px; font-family:Consolas,monospace;");
    ui.coordLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    ui.directionPad = new DirectionPadWidget(ui.mapView);
    ui.topRuler = new RulerWidget(RulerWidget::Horizontal);
    ui.leftRuler = new RulerWidget(RulerWidget::Vertical);

    auto *mapArea = new QWidget(splitter);
    auto *maVBox = new QVBoxLayout(mapArea);
    maVBox->setContentsMargins(0, 0, 0, 0); maVBox->setSpacing(0);
    maVBox->addWidget(ui.topRuler);
    auto *mapInner = new QWidget(mapArea);
    auto *miHBox = new QHBoxLayout(mapInner);
    miHBox->setContentsMargins(0, 0, 0, 0); miHBox->setSpacing(0);
    miHBox->addWidget(ui.leftRuler);
    miHBox->addWidget(ui.mapView, 1);
    maVBox->addWidget(mapInner, 1);
    splitter->addWidget(mapArea);

    auto *rightCont = new QWidget(splitter);
    rightCont->setMinimumWidth(340); rightCont->setMaximumWidth(700);
    auto *rl = new QVBoxLayout(rightCont);
    rl->setContentsMargins(0, 0, 0, 0);
    ui.propsMgr = new MapPropsManager(rightCont);
    ui.tablePanel = new MapFeatureTablePanel(rightCont);
    ui.tablePanel->setVisible(false);
    rl->addWidget(ui.propsMgr, 1);
    rl->addWidget(ui.tablePanel, 1);
    splitter->addWidget(rightCont);

    splitter->setStretchFactor(0, 1); splitter->setStretchFactor(1, 0);
    splitter->setSizes({880, 360});
    cl->addWidget(splitter, 1);
    root->addWidget(content, 1);

    return ui;
}

void wireSignals(MapEditWidget *w,
                 const MapEditToolBarHelper::TopBarWidgets &top,
                 const MapEditToolBarHelper::SubToolBarWidgets &sub,
                 const CentralUI &ui,
                 MapEditDataModel *model,
                 MapDrawController *drawCtrl,
                 MapDragController *dragCtrl,
                 MapRasterPaintController *paintCtrl,
                 MapEditTelemetryController *telemetryCtrl)
{
    QObject::connect(top.backBtn, &QPushButton::clicked, w, [w]() { if (w->checkUnsavedChanges()) emit w->goBack(); });
    QObject::connect(top.saveBtn, &QPushButton::clicked, w, [w]() { w->onSaveMap(); });
    QObject::connect(top.importSmapBtn, &QPushButton::clicked, w, &MapEditWidget::onImportSmap);
    QObject::connect(top.editMapBtn, &QPushButton::clicked, w, &MapEditWidget::onEditMap);
    QObject::connect(top.undoBtn, &QPushButton::clicked, w, &MapEditWidget::onUndo);
    QObject::connect(top.redoBtn, &QPushButton::clicked, w, &MapEditWidget::onRedo);
    QObject::connect(top.resetViewBtn, &QPushButton::clicked, w, &MapEditWidget::fitView);
    QObject::connect(top.gridBtn, &QPushButton::toggled, w, [w](bool on) { w->onGridToggled(on); });
    QObject::connect(top.robotBtn, &ToggleSwitch::toggled, w, [telemetryCtrl](bool on) { telemetryCtrl->setRobotVisible(on); });
    QObject::connect(top.reflectorBtn, &ToggleSwitch::toggled, w, [w](bool on) { w->onReflectorToggled(on); });
    QObject::connect(top.teleopBtn, &QPushButton::clicked, w, [telemetryCtrl, &top]() { telemetryCtrl->toggleTeleop(top.teleopBtn); });
    QObject::connect(top.helpBtn, &QPushButton::clicked, w, &MapEditWidget::showHelpDialog);

    for (auto it = ui.leftWidgets.toolBtns.begin(); it != ui.leftWidgets.toolBtns.end(); ++it) {
        int m = it.key(); QObject::connect(it.value(), &QPushButton::clicked, w, [w, m]() { w->setDrawMode(m); });
    }
    for (auto it = sub.stationBtns.begin(); it != sub.stationBtns.end(); ++it) {
        int m = it.key(); QObject::connect(it.value(), &QPushButton::clicked, w, [w, m]() { w->setDrawMode(m); });
    }
    QObject::connect(ui.leftWidgets.tableModeBtn, &QPushButton::clicked, w, &MapEditWidget::onTableToggle);

    QObject::connect(model, &MapEditDataModel::dataChanged, w, [w]() { w->onDataChanged(); });
    QObject::connect(ui.propsMgr, &MapPropsManager::applyClicked, w, [w]() { w->applyPropsEdit(); });
    QObject::connect(ui.propsMgr, &MapPropsManager::deleteClicked, w, &MapEditWidget::deleteSelected);
    QObject::connect(ui.tablePanel, &MapFeatureTablePanel::featureSelected, w, [w](const QString &t, const QString &id) {
        w->showFeatureProps(t, id); w->focusFeature(t, id);
    });
    QObject::connect(ui.tablePanel, &MapFeatureTablePanel::deleteRequested, w, [w](const QString &t, const QString &id) {
        w->deleteFeatureFromTable(t, id);
    });

    QObject::connect(ui.mapView, &MapCanvas::transformChanged, w, &MapEditWidget::updateRulers);
    QObject::connect(ui.mapView, &MapCanvas::canvasResized, w, [pad = ui.directionPad](const QSize &sz) {
        if (pad) {
            pad->move(16, sz.height() - pad->height() - 16);
            pad->raise();
        }
    });

    QObject::connect(drawCtrl, &MapDrawController::dataChanged, w, [w]() { w->onDataChanged(); });
    QObject::connect(drawCtrl, &MapDrawController::warningToast, w, [w](const QString &msg) { Toast::warning(w, msg); });
    QObject::connect(drawCtrl, &MapDrawController::successToast, w, [w](const QString &msg) { Toast::success(w, msg); });
    QObject::connect(drawCtrl, &MapDrawController::showPropertiesRequested, w, [w](const QString &t, const QString &id) { w->showFeatureProps(t, id); });
    QObject::connect(drawCtrl, &MapDrawController::modeChangeRequested, w, [w](int m) { w->setDrawMode(m); });
    QObject::connect(dragCtrl, &MapDragController::featureSelected, w, [w](const QString &t, const QString &id) { w->showFeatureProps(t, id); });
    QObject::connect(dragCtrl, &MapDragController::dataChanged, w, [w]() { w->onDataChanged(); });
    QObject::connect(dragCtrl, &MapDragController::areaBoundsChanged, w, [w]() { w->onAreaBoundsChanged(); });

    QObject::connect(&MotionConfigManager::instance(), &MotionConfigManager::configChanged, w, &MapEditWidget::syncMotionSpinBoxRanges);
    QObject::connect(&TeleopController::instance(), &TeleopController::activeChanged, w, &MapEditWidget::onTeleopActiveChanged);
    w->onTeleopActiveChanged(TeleopController::instance().isActive());

    QObject::connect(paintCtrl, &MapRasterPaintController::historyChanged, w, [top, paintCtrl]() {
        top.undoBtn->setEnabled(paintCtrl->canUndo());
        top.redoBtn->setEnabled(paintCtrl->canRedo());
    });

    auto *saveSc = new QShortcut(QKeySequence::Save, w, nullptr, nullptr, Qt::WidgetWithChildrenShortcut);
    QObject::connect(saveSc, &QShortcut::activated, w, [w]() { w->onSaveMap(); });
    auto *helpSc = new QShortcut(QKeySequence::HelpContents, w, nullptr, nullptr, Qt::WidgetWithChildrenShortcut);
    QObject::connect(helpSc, &QShortcut::activated, w, &MapEditWidget::showHelpDialog);
}

} // namespace MapEditLayoutHelper
