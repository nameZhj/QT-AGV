#ifndef EDITCONFIGWIDGET_H
#define EDITCONFIGWIDGET_H

#include <QWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QJsonObject>
#include <QJsonArray>

// 对应 edit-config/index.vue - 配置参数编辑
// 包含加载/保存/下载/上传配置文件，参数分组显示和编辑
class EditConfigWidget : public QWidget
{
    Q_OBJECT
public:
    explicit EditConfigWidget(QWidget *parent = nullptr);
    // 刷新参数配置数据(由 MainWindow 在切换菜单时调用)
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
    QString determineProfile(const QJsonObject &entry) const;
    QString getProfileName(const QString &key) const;
    QString getChildDisplayName(const QJsonObject &entry) const;
    QString basename(const QString &path) const;
    static QPair<QString,QString> fieldMetaFor(const QString &key);

    void parseMetadata(const QByteArray &rawJson);
    QJsonObject findMetadataForConfig(const QJsonObject &configEntry) const;
    QString getDefaultValueString(const QJsonObject &metaObj, const QString &fullPath) const;
    void updateDefaultValuesInTable();

    // UI
    QLineEdit *m_filterEdit = nullptr;
    QTreeWidget *m_groupTree = nullptr;
    QTableWidget *m_paramTable = nullptr;
    QToolButton *m_saveBtn = nullptr;
    QToolButton *m_downloadBtn = nullptr;
    QToolButton *m_uploadBtn = nullptr;

    // 数据
    QJsonArray m_configs;
    QJsonArray m_metadataList;     // /rbrainrobot/get_config_metadata 元数据
    QJsonObject m_originalConfigs; // 用于跟踪原始值
    QString m_currentGroupKey;
    int m_currentConfigIdx = -1; // -1 表示显示整个分组，>=0 表示显示特定配置文件
};

#endif // EDITCONFIGWIDGET_H
