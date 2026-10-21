/**
 * @file    CreateMapWorkflowController.cpp
 * @layer   GUI
 * @brief   SLAM and high-order reflector mapping lifecycle workflow implementation.
 * @deps    CreateMapWorkflowController.h, CreateMapApiHelper.h, ApiService.h, LoadingDialog.h, Toast.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "CreateMapWorkflowController.h"
#include "CreateMapApiHelper.h"
#include "network/ApiService.h"
#include "ui/common/LoadingDialog.h"
#include "ui/common/Toast.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>

using CreateMapApiHelper::checkApiError;

CreateMapWorkflowController::CreateMapWorkflowController(QWidget *parent)
    : QObject(parent), m_parentWidget(parent)
{
}

void CreateMapWorkflowController::startAdaptiveMapping(std::function<void(bool success)> onDone)
{
    QInputDialog dlg(m_parentWidget);
    dlg.setWindowTitle("请输入地图名称");
    dlg.setLabelText("地图名称:");
    dlg.setTextValue("");
    dlg.setOkButtonText("确定");
    dlg.setCancelButtonText("取消");
    bool ok = (dlg.exec() == QDialog::Accepted);
    QString name = dlg.textValue().trimmed();
    if (!ok || name.isEmpty()) {
        if (ok) QMessageBox::warning(m_parentWidget, "提示", "地图名称不能为空");
        if (onDone) onDone(false);
        return;
    }

    auto *loading = new LoadingDialog("正在启动建图，请稍候...", m_parentWidget->window(), Qt::ApplicationModal);
    loading->show();
    ApiService::instance().startMapping(name, [this, loading, onDone](int code, const QJsonObject &res) {
        loading->close(); loading->deleteLater();
        if (code == 200) {
            QString err = checkApiError(res);
            if (!err.isEmpty()) { QMessageBox::warning(m_parentWidget, "错误", err); if (onDone) onDone(false); return; }
            Toast::success(m_parentWidget, "建图已启动");
            if (onDone) onDone(true);
        } else {
            QMessageBox::warning(m_parentWidget, "错误", "启动建图失败");
            if (onDone) onDone(false);
        }
    });
}

void CreateMapWorkflowController::stopAdaptiveMapping(std::function<void(bool finished, bool saved)> onDone)
{
    QMessageBox msgBox(m_parentWidget);
    msgBox.setWindowTitle("保存地图");
    msgBox.setText("是否保存当前自适应建图地图？");
    QPushButton *saveBtn   = msgBox.addButton("保存", QMessageBox::YesRole);
    QPushButton *cancelBtn = msgBox.addButton("取消", QMessageBox::RejectRole);
    msgBox.addButton("不保存", QMessageBox::NoRole);
    msgBox.exec();
    auto *clicked = msgBox.clickedButton();
    if (clicked == cancelBtn) return;

    bool isSave = (clicked == saveBtn);
    QString tip = isSave ? "正在保存地图，请稍候..." : "正在取消建图，请稍候...";
    auto *loading = new LoadingDialog(tip, m_parentWidget->window(), Qt::ApplicationModal);
    loading->show();

    auto cb = [this, loading, isSave, onDone](int code, const QJsonObject &res) {
        loading->close(); loading->deleteLater();
        if (code == 200) {
            QString err = checkApiError(res);
            if (!err.isEmpty()) { QMessageBox::warning(m_parentWidget, "错误", err); return; }
            Toast::success(m_parentWidget, isSave ? "地图保存成功" : "建图已取消");
            if (onDone) onDone(true, isSave);
        } else {
            QMessageBox::warning(m_parentWidget, "错误", isSave ? "保存地图失败" : "取消建图失败");
        }
    };
    if (isSave) ApiService::instance().saveMap(cb);
    else ApiService::instance().cancelMapping(cb);
}

void CreateMapWorkflowController::startHighOrderMapping(std::function<void(bool success)> onDone)
{
    QMessageBox confirmBox(m_parentWidget);
    confirmBox.setWindowTitle("提示");
    confirmBox.setText("确定要开始高阶建图吗？");
    QPushButton *okBtn = confirmBox.addButton("确定", QMessageBox::YesRole);
    confirmBox.addButton("取消", QMessageBox::RejectRole);
    confirmBox.exec();
    if (confirmBox.clickedButton() != okBtn) { if (onDone) onDone(false); return; }

    auto *loading = new LoadingDialog("正在启动高阶建图，请稍候...", m_parentWidget->window(), Qt::ApplicationModal);
    loading->show();
    ApiService::instance().startReflectorMapping([this, loading, onDone](int code, const QJsonObject &res) {
        loading->close(); loading->deleteLater();
        if (code == 200) {
            QString err = checkApiError(res);
            if (!err.isEmpty()) { QMessageBox::warning(m_parentWidget, "错误", err); if (onDone) onDone(false); return; }
            Toast::success(m_parentWidget, "高阶建图已启动");
            if (onDone) onDone(true);
        } else {
            QMessageBox::warning(m_parentWidget, "错误", "启动高阶建图失败");
            if (onDone) onDone(false);
        }
    });
}

void CreateMapWorkflowController::stopHighOrderMapping(std::function<void(bool finished, bool saved)> onDone)
{
    QMessageBox msgBox(m_parentWidget);
    msgBox.setWindowTitle("保存高阶地图");
    msgBox.setText("是否保存当前高阶建图地图？");
    QPushButton *saveBtn   = msgBox.addButton("保存", QMessageBox::YesRole);
    QPushButton *cancelBtn = msgBox.addButton("取消", QMessageBox::RejectRole);
    msgBox.addButton("不保存", QMessageBox::NoRole);
    msgBox.exec();
    auto *clicked = msgBox.clickedButton();
    if (clicked == cancelBtn) return;

    bool isSave = (clicked == saveBtn);
    QString tip = isSave ? "正在停止高阶建图，请稍候..." : "正在取消高阶建图，请稍候...";
    auto *loading = new LoadingDialog(tip, m_parentWidget->window(), Qt::ApplicationModal);
    loading->show();

    auto cb = [this, loading, isSave, onDone](int code, const QJsonObject &res) {
        loading->close(); loading->deleteLater();
        if (code == 200) {
            QString err = checkApiError(res);
            if (!err.isEmpty()) { QMessageBox::warning(m_parentWidget, "错误", err); return; }
            Toast::success(m_parentWidget, isSave ? "高阶地图保存成功" : "高阶建图已取消");
            if (onDone) onDone(true, isSave);
        } else {
            QMessageBox::warning(m_parentWidget, "错误", isSave ? "停止高阶建图失败" : "取消高阶建图失败");
        }
    };
    if (isSave) ApiService::instance().stopReflectorMapping(cb);
    else ApiService::instance().cancelReflectorMapping(cb);
}

void CreateMapWorkflowController::resetReflectorAll()
{
    auto *loading = new LoadingDialog("正在全局重置反光柱...", m_parentWidget);
    loading->show();
    ApiService::instance().resetReflectorAll([this, loading](int code, const QJsonObject &res) {
        loading->close(); loading->deleteLater();
        if (code == 200) {
            QString err = checkApiError(res);
            if (!err.isEmpty()) { QMessageBox::warning(m_parentWidget, "错误", err); return; }
            Toast::success(m_parentWidget, "全局重置反光柱成功");
        } else {
            QMessageBox::warning(m_parentWidget, "错误", "全局重置反光柱失败");
        }
    });
}

void CreateMapWorkflowController::resetReflectorLocal()
{
    auto *loading = new LoadingDialog("正在局部重置反光柱...", m_parentWidget);
    loading->show();
    ApiService::instance().resetReflectorLocal([this, loading](int code, const QJsonObject &res) {
        loading->close(); loading->deleteLater();
        if (code == 200) {
            QString err = checkApiError(res);
            if (!err.isEmpty()) { QMessageBox::warning(m_parentWidget, "错误", err); return; }
            Toast::success(m_parentWidget, "局部重置反光柱成功");
        } else {
            QMessageBox::warning(m_parentWidget, "错误", "局部重置反光柱失败");
        }
    });
}

void CreateMapWorkflowController::toggleCruise(bool currentSuspended, std::function<void(bool newSuspended)> onDone)
{
    QString action = currentSuspended ? QStringLiteral("continue navigation") : QStringLiteral("pause navigation");
    QString msg = currentSuspended ? "正在恢复巡航..." : "正在暂停巡航...";
    auto *loading = new LoadingDialog(msg, m_parentWidget);
    loading->show();
    ApiService::instance().controlNavigation(action, [this, loading, currentSuspended, onDone](int code, const QJsonObject &res) {
        loading->close(); loading->deleteLater();
        if (code == 200) {
            QString err = checkApiError(res);
            if (!err.isEmpty()) { QMessageBox::warning(m_parentWidget, "错误", err); return; }
            bool nextState = !currentSuspended;
            Toast::success(m_parentWidget, nextState ? "巡航已暂停" : "巡航已恢复");
            if (onDone) onDone(nextState);
        } else {
            QMessageBox::warning(m_parentWidget, "错误", currentSuspended ? "恢复巡航失败" : "暂停巡航失败");
        }
    });
}

bool CreateMapWorkflowController::tryExitMapping(bool mappingActive, bool highMappingActive, std::function<void()> onExitAllowed)
{
    if (!mappingActive && !highMappingActive) return true;

    QMessageBox msgBox(m_parentWidget);
    msgBox.setWindowTitle(highMappingActive ? "保存高阶地图" : "保存自适应地图");
    msgBox.setText(highMappingActive ? "是否保存当前高阶建图地图？" : "是否保存当前自适应建图地图？");

    QPushButton *saveBtn   = msgBox.addButton("保存地图", QMessageBox::YesRole);
    QPushButton *cancelBtn = msgBox.addButton("取消保存", QMessageBox::NoRole);
    QPushButton *abortBtn  = msgBox.addButton("返回", QMessageBox::RejectRole);
    msgBox.exec();
    auto *clicked = msgBox.clickedButton();
    if (clicked == abortBtn) return false;

    bool isSave = (clicked == saveBtn);
    QString txt = isSave ? (highMappingActive ? "正在保存高阶地图，请稍候..." : "正在保存自适应地图，请稍候...")
                         : (highMappingActive ? "正在取消高阶建图，请稍候..." : "正在取消自适应建图，请稍候...");
    auto *loading = new LoadingDialog(txt, m_parentWidget->window(), Qt::ApplicationModal);
    loading->show();

    auto cb = [this, loading, highMappingActive, isSave, onExitAllowed](int code, const QJsonObject &res) {
        loading->close(); loading->deleteLater();
        if (code == 200) {
            QString err = checkApiError(res);
            if (!err.isEmpty()) { QMessageBox::warning(m_parentWidget, "错误", err); return; }
            QString okMsg = isSave ? (highMappingActive ? "高阶地图保存成功" : "自适应地图保存成功")
                                   : (highMappingActive ? "高阶建图已取消" : "自适应建图已取消");
            Toast::success(m_parentWidget, okMsg);
            if (onExitAllowed) onExitAllowed();
        } else {
            QString failMsg = isSave ? (highMappingActive ? "保存高阶地图失败，请重试" : "保存自适应地图失败，请重试")
                                     : (highMappingActive ? "取消高阶建图失败，请重试" : "取消自适应建图失败，请重试");
            QMessageBox::warning(m_parentWidget, "错误", failMsg);
        }
    };

    if (highMappingActive) {
        if (isSave) ApiService::instance().stopReflectorMapping(cb);
        else ApiService::instance().cancelReflectorMapping(cb);
    } else {
        if (isSave) ApiService::instance().saveMap(cb);
        else ApiService::instance().cancelMapping(cb);
    }
    return false;
}
