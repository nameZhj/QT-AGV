/**
 * @file    EditConfigWidget.h
 * @layer   GUILayer
 * @brief   Hierarchical tree widget for editing and synchronizing robot motion and system configs.
 * @deps    QWidget, ConfigMetadataHelper, ApiService
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QWidget>
#include <QJsonObject>
#include <QJsonArray>
#include <functional>

class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;
class QTableWidget;
class QLineEdit;

class EditConfigDataLoader;
class EditConfigRouteSync;
class EditConfigTableBuilder;
class EditConfigParamEditor;

/**
 * @brief 机器人及硬件各模块配置参数树形编辑器视图 (对应 Vue 端 edit-config)
 * 包含加载/保存/下载/上传配置文件，参数分组显示和可视化编辑。
 */
class EditConfigWidget : public QWidget
{
    Q_OBJECT
    friend class EditConfigDataLoader;
    friend class EditConfigRouteSync;
    friend class EditConfigTableBuilder;
    friend class EditConfigParamEditor;

public:
    explicit EditConfigWidget(QWidget *parent = nullptr);

    /**
     * @brief 刷新参数配置数据 (由 MainWindow 在切换菜单时调度)
     */
    void refreshData();

private slots:
    void onSave();
    void onDownload();
    void onUpload();
    void onFilterChanged(const QString &text);
    void onGroupItemClicked(QTreeWidgetItem *item, int column);
    void onParamCellChanged(int row, int col);

private:
    void setupUI();
    void loadAll();
    void refreshGroups();
    void refreshParams(const QString &groupKey);
    void refreshParamsForConfig(int configIdx);
    void addParamRows(const QJsonObject &params, int configIdx,
                      const QString &prefix, const QString &filter);

    void parseMetadata(const QByteArray &rawJson);
    void updateDefaultValuesInTable();
    void syncObstacleParamsToRouteNetwork(double stopDx, double stopDy,
                                          double decelDx, double decelDy,
                                          std::function<void(bool ok)> onDone = nullptr);

signals:
    void routeNetworkUpdated();

private:
    // UI 控件
    QLineEdit   *m_filterEdit   = nullptr;
    QTreeWidget *m_groupTree    = nullptr;
    QTableWidget*m_paramTable   = nullptr;
    QToolButton *m_saveBtn      = nullptr;
    QToolButton *m_downloadBtn  = nullptr;
    QToolButton *m_uploadBtn    = nullptr;

    // 数据缓存
    QJsonArray  m_configs;
    QJsonArray  m_metadataList;     ///< /get_config_metadata 元数据
    QJsonObject m_originalConfigs;  ///< 用于跟踪原始值
    QString     m_currentGroupKey;
    int         m_currentConfigIdx = -1; ///< -1 表示显示整个分组，>=0 表示显示特定配置文件
};
