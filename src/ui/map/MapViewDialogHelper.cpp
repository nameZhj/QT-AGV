/**
 * @file    MapViewDialogHelper.cpp
 * @layer   GUI
 * @brief   Dialog popup implementations and interaction routing for MapView.
 * @deps    MapViewDialogHelper.h, MapUploadDialog.h, SmapUploadDialog.h, MapViewSwitchMapDialog.h, RobotStatusDialog.h, EditConfigWidget.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapViewDialogHelper.h"
#include "MapUploadDialog.h"
#include "SmapUploadDialog.h"
#include "MapViewSwitchMapDialog.h"
#include "RobotStatusDialog.h"
#include "EditConfigWidget.h"
#include "MapViewRelocController.h"
#include "MapViewNavInteraction.h"
#include "MapViewToolbarHelper.h"
#include "MapCanvas.h"
#include <QDialog>
#include <QVBoxLayout>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QUrl>
#include <QJsonDocument>
#include <QGraphicsScene>

void MapViewDialogHelper::showUploadMap(QWidget *parent, std::function<void()> onSuccess)
{
    auto *d = new MapUploadDialog(parent);
    d->setAttribute(Qt::WA_DeleteOnClose);
    if (onSuccess) QObject::connect(d, &MapUploadDialog::uploadSucceeded, d, onSuccess);
    d->exec();
}

void MapViewDialogHelper::showUploadSmap(QWidget *parent, std::function<void()> onSuccess)
{
    auto *d = new SmapUploadDialog(parent);
    d->setAttribute(Qt::WA_DeleteOnClose);
    if (onSuccess) QObject::connect(d, &SmapUploadDialog::uploadSucceeded, d, onSuccess);
    d->exec();
}

void MapViewDialogHelper::showSwitchMap(QWidget *parent, const QString &mapId, std::function<void(const QString &newMapId)> onSwitched)
{
    MapViewSwitchMapDialog dlg(mapId, parent);
    if (dlg.exec() == QDialog::Accepted && !dlg.selectedMapId().isEmpty()) {
        if (onSwitched) onSwitched(dlg.selectedMapId());
    }
}

void MapViewDialogHelper::showRobotStatus(QWidget *parent, RobotStatusDialog *&dlg, const QString &mapId, const QString &mapName, const MapViewRobotState &st)
{
    if (!dlg) dlg = new RobotStatusDialog(parent);
    dlg->setMapInfo(mapId, mapName);
    dlg->updateFromRobotState(st.carId, st.wx, st.wy, st.yawRad, st.speed, st.battery, st.covScore, st.posValid);
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}

void MapViewDialogHelper::showConfig(QWidget *parent, QDialog *&configDlg, EditConfigWidget *&editWidget, std::function<void()> onUpdate)
{
    if (!configDlg) {
        configDlg = new QDialog(parent);
        configDlg->setWindowTitle("参数配置");
        configDlg->resize(1100, 700);
        configDlg->setMinimumSize(850, 520);
        auto *l = new QVBoxLayout(configDlg);
        l->setContentsMargins(10, 10, 10, 10);
        editWidget = new EditConfigWidget(configDlg);
        l->addWidget(editWidget);
        if (onUpdate) QObject::connect(editWidget, &EditConfigWidget::routeNetworkUpdated, editWidget, onUpdate);
    }
    if (editWidget) editWidget->refreshData();
    configDlg->show();
    configDlg->raise();
    configDlg->activateWindow();
}

void MapViewDialogHelper::showDownloadSource(const QString &mapId)
{
    QString p = QCoreApplication::applicationDirPath() + "/maps/" + (mapId.isEmpty() ? "" : mapId);
    QDir().mkpath(p);
    QDesktopServices::openUrl(QUrl::fromLocalFile(p));
}

void MapViewDialogHelper::unpackRouteJson(const QString &jsonStr, QJsonArray &pts, QJsonArray &lns, QJsonArray &crvs,
                                         QJsonArray &chgs, QJsonArray &hps, QJsonArray &areas)
{
    pts   = QJsonArray();
    lns   = QJsonArray();
    crvs  = QJsonArray();
    chgs  = QJsonArray();
    hps   = QJsonArray();
    areas = QJsonArray();

    if (jsonStr.trimmed().isEmpty()) return;
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    if (!doc.isObject()) return;
    QJsonObject root = doc.object();

    auto parseAreasFromVal = [&](const QJsonValue &v) {
        if (v.isArray()) {
            areas = v.toArray();
        } else if (v.isObject() && !v.toObject().isEmpty()) {
            areas.append(v.toObject());
        }
    };

    if (root.contains("highLevelArea")) {
        parseAreasFromVal(root.value("highLevelArea"));
    } else if (root.contains("highLevel Area")) {
        parseAreasFromVal(root.value("highLevel Area"));
    } else if (root.contains("highLevelAreas")) {
        parseAreasFromVal(root.value("highLevelAreas"));
    }

    for (int i = 0; i < areas.size(); ++i) {
        QJsonObject a = areas[i].toObject();
        if (a.value("id").toString().isEmpty()) {
            a["id"] = QString("highLevelArea_%1").arg(i + 1);
        }
        if (a.value("name").toString().isEmpty()) {
            a["name"] = QString("AdvancedArea %1").arg(i + 1);
        }
        if (!a.contains("coordinates") && a.contains("minX")) {
            double minX = a.value("minX").toDouble();
            double maxX = a.value("maxX").toDouble();
            double minY = a.value("minY").toDouble();
            double maxY = a.value("maxY").toDouble();
            QJsonArray coords, pTL, pBR;
            pTL.append(minX); pTL.append(maxY);
            pBR.append(maxX); pBR.append(minY);
            coords.append(pTL); coords.append(pBR);
            a["coordinates"] = coords;
        }
        areas[i] = a;
    }

    if (root.contains("features") && root.value("features").isArray()) {
        QJsonArray features = root.value("features").toArray();
        for (const auto &fv : features) {
            QJsonObject f = fv.toObject();
            QJsonObject props = f.value("properties").toObject();
            QJsonObject geom  = f.value("geometry").toObject();
            QString gtype = geom.value("type").toString();
            QString id    = props.value("id").toString();
            QJsonArray coords = geom.value("coordinates").toArray();

            if (gtype == "Point") {
                QJsonObject pt = props;
                pt["id"] = id;
                if (coords.size() >= 2) {
                    pt["x"] = coords[0].toDouble();
                    pt["y"] = coords[1].toDouble();
                }
                if (pt.value("name").toString().isEmpty()) {
                    pt["name"] = props.value("name").toString();
                }
                if (id.startsWith("c_") || props.value("typeName").toString() == "ChargePoint")
                    chgs.append(pt);
                else if (id.startsWith("hp_") || props.value("typeName").toString() == "ActionPoint")
                    hps.append(pt);
                else
                    pts.append(pt);
            } else if (gtype == "LineString") {
                QJsonObject ln = props;
                ln["id"] = id;
                if (coords.size() >= 2) {
                    QJsonArray c0 = coords[0].toArray();
                    QJsonArray c1 = coords[1].toArray();
                    ln["x1"] = c0[0].toDouble(); ln["y1"] = c0[1].toDouble();
                    ln["x2"] = c1[0].toDouble(); ln["y2"] = c1[1].toDouble();
                }
                lns.append(ln);
            } else if (gtype == "MultiPoint") {
                QJsonObject cv = props;
                cv["id"] = id;
                QJsonArray ctrlArr;
                for (const auto &c : coords) {
                    QJsonArray ca = c.toArray();
                    if (ca.size() < 2) continue;
                    QJsonObject p; p["x"] = ca[0].toDouble(); p["y"] = ca[1].toDouble();
                    ctrlArr.append(p);
                }
                cv["controlPoints"] = ctrlArr;
                if (coords.size() >= 2) {
                    QJsonArray a0 = coords[0].toArray(), aN = coords[coords.size()-1].toArray();
                    cv["x1"] = a0[0].toDouble(); cv["y1"] = a0[1].toDouble();
                    cv["x2"] = aN[0].toDouble(); cv["y2"] = aN[1].toDouble();
                }
                if (coords.size() == 3) {
                    QJsonArray a1 = coords[1].toArray();
                    cv["cx"] = a1[0].toDouble(); cv["cy"] = a1[1].toDouble();
                }
                crvs.append(cv);
            }
        }
    } else {
        if (root.contains("points")) pts = root.value("points").toArray();
        if (root.contains("lines")) lns = root.value("lines").toArray();
        if (root.contains("curves")) crvs = root.value("curves").toArray();
        if (root.contains("charges")) chgs = root.value("charges").toArray();
        if (root.contains("highPrecisionPoints")) hps = root.value("highPrecisionPoints").toArray();
    }
}

QString MapViewDialogHelper::findStationName(const QString &id, const QJsonArray &pts, const QJsonArray &chgs, const QJsonArray &hps)
{
    if (id.isEmpty()) return QString();
    for (const auto *a : { &pts, &chgs, &hps }) {
        for (const auto &v : *a) {
            if (v.toObject().value("id").toString() == id) {
                QString n = v.toObject().value("name").toString();
                return n.isEmpty() ? id : n;
            }
        }
    }
    return id;
}

QString MapViewDialogHelper::formatRouteName(const QJsonObject &obj, const std::function<QString(const QString&)> &nameLookup)
{
    QString n = obj.value("name").toString();
    if (!n.isEmpty()) return n;
    QString s = nameLookup ? nameLookup(obj.value("startid").toString()) : obj.value("startid").toString();
    QString e = nameLookup ? nameLookup(obj.value("endid").toString()) : obj.value("endid").toString();
    return (!s.isEmpty() && !e.isEmpty()) ? QString("%1 ⟷ %2").arg(s, e) : obj.value("id").toString();
}

void MapViewDialogHelper::handleRelocalize(MapViewRelocController *relocCtrl, MapViewNavInteraction *navInteraction,
                                          MapCanvas *canvas, QGraphicsScene *scene, MapViewToolbarButtons &btns,
                                          std::function<void(const QString&, const QString&)> setStatus)
{
    if (!relocCtrl) return;
    if (relocCtrl->isActive()) {
        relocCtrl->exitRelocMode(scene);
        if (setStatus) setStatus("已退出重定位模式", "#8898a8");
        if (canvas) canvas->setPanWithLeftButton(!navInteraction->isNavMode());
        if (btns.relocBtn) btns.relocBtn->setChecked(false);
    } else {
        relocCtrl->enterRelocMode();
        if (navInteraction->isNavMode()) {
            navInteraction->setNavMode(false);
            if (btns.navModeBtn) {
                btns.navModeBtn->setChecked(false);
                btns.navModeBtn->setStyleSheet(MapViewToolbarHelper::kToolBtnStyle);
            }
        }
        if (canvas) { canvas->setPanWithLeftButton(false); canvas->setCursor(Qt::CrossCursor); }
        if (btns.relocBtn) btns.relocBtn->setChecked(true);
        if (setStatus) setStatus("重定位模式: 点击并在地图上拖拽设定初始位姿", "#00bcd4");
    }
}

void MapViewDialogHelper::handleToggleNavMode(MapViewNavInteraction *navInteraction, MapViewRelocController *relocCtrl,
                                             MapCanvas *canvas, QGraphicsScene *scene, MapViewToolbarButtons &btns,
                                             std::function<void(const QString&, const QString&)> setStatus)
{
    bool on = btns.navModeBtn && btns.navModeBtn->isChecked();
    navInteraction->setNavMode(on);
    if (on) {
        if (btns.navModeBtn) btns.navModeBtn->setStyleSheet(MapViewToolbarHelper::kToolBtnPrimary);
        if (relocCtrl && relocCtrl->isActive()) {
            relocCtrl->exitRelocMode(scene);
            if (btns.relocBtn) btns.relocBtn->setChecked(false);
        }
        if (canvas) { canvas->setPanWithLeftButton(false); canvas->setCursor(Qt::PointingHandCursor); }
        if (setStatus) setStatus("路径导航模式已开启: 请在地图上点击任意路网点发起导航", "#00bcd4");
    } else {
        if (btns.navModeBtn) btns.navModeBtn->setStyleSheet(MapViewToolbarHelper::kToolBtnStyle);
        if (canvas) {
            canvas->setPanWithLeftButton(!relocCtrl || !relocCtrl->isActive());
            canvas->setCursor(Qt::CrossCursor);
        }
        if (setStatus) setStatus("已退出路径导航模式", "#8898a8");
    }
}
