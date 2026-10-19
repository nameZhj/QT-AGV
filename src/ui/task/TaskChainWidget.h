#pragma once

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
class QTimer;

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
    ~TaskChainWidget() override;

    void reorderGroup(int fromIdx, int toIdx);
    void reorderTask(int fromIdx, int toIdx);

signals:
    void goBack();

private slots:
    // 面板1: 任务链列表
    void onChainNew();
    void onChainLoad();
    void onChainSave();
    void onChainExport();
    void onChainItemChanged(QListWidgetItem *current);

    // 面板2: 任务模块
    void onTaskNew();
    void onTaskDelete();
    void onTaskSelectAll();

    // 面板3: 组模块
    void onGroupAdd();
    void onGroupInsert();
    void onGroupDelete();
    void onGroupSelectAll();
    void onGroupSendSelected();
    void onGroupSendAll();

    // 底部状态栏控制按钮
    void onSuspendResume();
    void onCancelChain();
    void onExecuteNext();

protected:
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

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

    // 组复制与粘贴 (Ctrl+C / Ctrl+V)
    void copySelectedGroup();
    void pasteGroup();
    void updateGroupCardSelectionHighlight();

    // 路网站点
    struct StationItem {
        QString id;
        QString name;
    };
    QList<StationItem> m_stations;
    void loadMapStations();
    QString stationName(const QString &idOrName) const;   // 路网点 id -> 显示名称

    // 本地缓存 (exe 同级 taskChainCache.json) + 服务器列表
    void loadChainsFromCacheAndServer();
    void scheduleCacheWrite();
    void writeCache();

    // 任务链运行状态 (MQTT /taskchain/status)
    void subscribeChainStatus();
    void onChainStatusMessage(const QByteArray &payload);

    // 任务链执行进度 (MQTT /taskchain/currentTask): {"currentTask":"sub_task_1","currentGroup":"group_2"}
    void subscribeChainCurrentTask();
    void onChainCurrentTaskMessage(const QByteArray &payload);

    // 执行期间锁定任务链/任务/组列表 (robotWorking=false 时解锁)
    void setListsLocked(bool locked);
    void updateLockOverlayGeometry();
    void installLockFilter(QWidget *w, bool install);
    void applyRuntimeSelection();          // 按 MQTT 上报的当前任务/组设置选中项
    void updateRuntimeCardHighlights(int currentTaskIdx, int currentGroupIdx);
    bool matchSubTaskName(int taskIdx, const QString &targetName) const;
    bool matchGroupName(int groupIdx, const QString &targetName) const;
    QString subTaskNameAt(int taskIdx) const;
    QString groupNameAt(int groupIdx) const;

    // 数据操作
    void selectChain(int index);
    int  currentChainIndex() const;
    void refreshTaskList();
    void refreshGroupList();
    void refreshActionTags();
    void updateStatus(const QString &text);
    void refreshChainList();
    void createChainRowWidget(QListWidgetItem *item, int chainIndex);
    void updateChainCardHighlight();

    // 组相关
    void createGroupRowWidget(QListWidgetItem *item, int groupIndex, bool checked = false);
    QString openActionDefDialog(const ActionDef &def, const QJsonObject &existingParams);

    // 音频名称下拉 (数据来自 GET /robot/audio/list)
    QStringList fetchAudioFiles(int timeoutMs = 2500);
    static QString audioDisplayName(const QString &fullName);
    QStringList m_audioFiles;

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
    QCheckBox   *m_loopCheck        = nullptr;
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
    QJsonArray m_copiedGroups;      // 复制的组数据缓存

    // 缓存状态: m_cacheLoaded 保证每个运行周期只从缓存+服务器载入一次
    // m_didSave 标记本次运行是否点击过“保存”, 未保存则退出时清理本地缓存
    QTimer *m_cacheTimer = nullptr;
    bool m_cacheLoaded = false;
    bool m_didSave = false;

    // MQTT /taskchain/status: robotWorking 有任务在运行, robotPause 已暂停
    bool m_chainWorking = false;
    bool m_chainPaused  = false;
    bool m_statusSubscribed = false;

    // MQTT /taskchain/currentTask: 当前执行到的任务/组名称
    bool    m_currentSubscribed = false;
    QString m_currentTaskName;
    QString m_currentGroupName;
    // 列表锁定标记: 下发任务链后锁定, robotWorking=false 时解锁
    bool    m_listLocked = false;
    // 覆盖整个编辑区域的半透明遮罩控件与文本
    QWidget *m_panelsContainer   = nullptr;
    QWidget *m_lockOverlay       = nullptr;
    QLabel  *m_overlayTextLabel  = nullptr;

protected:
    void resizeEvent(QResizeEvent *event) override;

    // 辅助: 获取当前任务的 groups 数组
    QJsonArray currentTaskGroups() const;
    void setCurrentTaskGroups(const QJsonArray &groups);

    // 标准 JSON 数据结构转换 (包含 task_name / sub_task_info / group_info / enable / actions)
    // forExecution: true=用于向后台下发执行(enable强制置为true, 过滤空组任务), false=用于本地保存(保留原本勾选状态)
    QJsonObject exportChainToJson(const ChainData &chain, bool onlyChecked = false, bool forExecution = false) const;
    ChainData   importChainFromJson(const QJsonObject &json) const;
};

