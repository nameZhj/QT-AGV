#ifndef TASKCHAINWIDGET_H
#define TASKCHAINWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QToolButton>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QListWidget>
#include <QCheckBox>
#include <QJsonObject>
#include <QJsonArray>
#include <QDialog>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>

class QVBoxLayout;

// 动作标签定义: 标签名 + 类别 + 参数定义字段
struct ActionDef {
    QString name;       // 显示名, 如 "移动到目标点"
    QString category;   // 类别, 用于同组去重
    struct ParamDef {
        QString key;
        QString label;
        QString type;       // "text", "double", "int", "combo"
        QStringList options;
        QString unit;
        double defaultValue = 0;
    };
    QList<ParamDef> params;
};

class TaskChainWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TaskChainWidget(QWidget *parent = nullptr);

signals:
    void goBack();

private slots:
    // 面板1: 任务链列表
    void onChainNew();
    void onChainLoad();
    void onChainSave();
    void onChainItemChanged(QListWidgetItem *current);

    // 面板2: 任务模块
    void onTaskNew();
    void onTaskDelete();
    void onTaskSelectAll();
    void onTaskItemChanged(QListWidgetItem *item);

    // 面板3: 组模块
    void onGroupAdd();
    void onGroupInsert();
    void onGroupDelete();
    void onGroupSelectAll();
    void onGroupLoopToggled(bool checked);
    void onGroupSendSelected();
    void onGroupSendAll();

    // 面板4: 可拖拽标签
    void onActionTriggered(const QString &actionName);

    // 底部状态栏控制按钮
    void onSuspendResume();
    void onCancelChain();
    void onExecuteNext();

protected:
    void showEvent(QShowEvent *event) override;

private:
    void executeSingleAction(const QJsonObject &action);
    void setupUI();
    void setupPanel1Chain(QVBoxLayout *layout);
    void setupPanel2Task(QVBoxLayout *layout);
    void setupPanel3Group(QVBoxLayout *layout);
    void setupPanel4DragDrop(QVBoxLayout *layout);
    void setupBottomBar(QVBoxLayout *layout);
    void updateBottomBar();
    void updateTaskSelectBtn();   // 全选任务/取消选中 互斥切换
    void updateGroupSelectBtn();  // 全选组/取消选中 互斥切换

    // 路网站点
    struct StationItem {
        QString id;
        QString name;
    };
    QList<StationItem> m_stations;
    void loadMapStations();

    // 数据操作
    void addChainItem(const QString &name);
    void selectChain(int index);
    int  currentChainIndex() const;
    void refreshTaskList();
    void refreshGroupList();
    void refreshActionTags();
    void updateStatus(const QString &text);
    void refreshChainList();
    void createChainRowWidget(QListWidgetItem *item, int chainIndex);

    // 组相关
    void createGroupRowWidget(QListWidgetItem *item, int groupIndex, bool checked = false);
    QString openActionDefDialog(const ActionDef &def, const QJsonObject &existingParams);

    // 动作定义表
    void initActionDefs();
    ActionDef* findActionDef(const QString &name);
    QList<ActionDef> m_actionDefs;

    // ── 面板1: 任务链列表 ──
    QListWidget *m_chainList = nullptr;
    QPushButton *m_chainNewBtn  = nullptr;
    QPushButton *m_chainLoadBtn = nullptr;
    QPushButton *m_chainSaveBtn = nullptr;
    QLabel      *m_chainStatus  = nullptr;

    // ── 面板2: 任务模块 ──
    QListWidget *m_taskList = nullptr;
    QPushButton *m_taskNewBtn     = nullptr;
    QPushButton *m_taskDeleteBtn   = nullptr;
    QPushButton *m_taskSelectAllBtn = nullptr;
    QLabel      *m_taskCountLabel  = nullptr;

    // ── 面板3: 组模块 ──
    QListWidget *m_groupList = nullptr;
    QPushButton *m_groupAddBtn      = nullptr;
    QPushButton *m_groupInsertBtn   = nullptr;
    QPushButton *m_groupDeleteBtn   = nullptr;
    QPushButton *m_groupSelectAllBtn = nullptr;
    QCheckBox   *m_groupLoopCheck   = nullptr;
    QPushButton *m_groupSendSelBtn   = nullptr;
    QPushButton *m_groupSendAllBtn   = nullptr;
    QLabel      *m_groupHintLabel   = nullptr;

    // ── 面板4: 可拖拽标签 ──
    QListWidget *m_actionTagList = nullptr;
    QLabel      *m_dragHintLabel  = nullptr;

    // ── 底部公共状态栏 ──
    QLabel      *m_bottomInfoLabel  = nullptr;
    QPushButton *m_suspendResumeBtn  = nullptr;  // 暂停/继续 (互斥)
    QPushButton *m_cancelBtn        = nullptr;  // 取消
    QPushButton *m_executeNextBtn   = nullptr;  // 执行Next任务

    // 执行状态
    enum ChainStatus { NotStarted, Running, Suspended, Completed };
    ChainStatus m_execStatus = NotStarted;

    // 数据
    struct ChainData {
        QString name;
        QJsonArray tasks;       // 任务列表, 每个任务含 name/note/checked/groups
    };
    QList<ChainData> m_chains;
    int m_currentChainIndex = -1;
    int m_currentTaskIndex = -1;    // 当前选中的任务

    // 辅助: 获取当前任务的 groups 数组
    QJsonArray currentTaskGroups() const;
    void setCurrentTaskGroups(const QJsonArray &groups);

    // 标准 JSON 数据结构转换 (包含 task_name / sub_task_info / group_info / enable / actions)
    QJsonObject exportChainToJson(const ChainData &chain, bool onlyChecked = false) const;
    ChainData   importChainFromJson(const QJsonObject &json) const;
};

#endif // TASKCHAINWIDGET_H