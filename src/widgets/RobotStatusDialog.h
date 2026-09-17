#ifndef ROBOTSTATUSDIALOG_H
#define ROBOTSTATUSDIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QLabel>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QFrame>
#include <QTimer>
#include <QDateTime>
#include <QJsonObject>

/**
 * @brief 运行状态弹窗：展示机器人基本信息、运动状态、运行状态、导航状态以及网络连接详情
 * 动态读取上位机真实网络、Wi-Fi 信号/网卡、当前地图以及 MQTT "/info" 话题数据。
 * 无对应版本/参数的字段严格显示为 "--"，有真实数据的字段实时展现真实值。
 */
class RobotStatusDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RobotStatusDialog(QWidget *parent = nullptr);
    ~RobotStatusDialog() override;

    // 更新当前地图上下文
    void setMapInfo(const QString &mapId, const QString &mapName);

    // 从 MapViewWidget 同步机器人状态
    void updateFromRobotState(const QString &carId, double x, double y, double yawRad,
                              double speed, double battery, double covScore, bool posValid);

    // 接收并解析 MQTT 原始报文
    void onMqttMessage(const QString &topic, const QByteArray &payload);

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void onPeriodicRefresh();

private:
    void setupUi();
    QWidget *createBasicInfoTab();
    QWidget *createPlaceholderTab(const QString &tabName);

    void initMqttSubscriptions();
    void refreshNetworkAndWifiInfo();
    void parseMqttPayload(const QJsonObject &d);
    void parseNearestObstaclePayload(const QJsonObject &d);

    enum class BadgeType {
        Green,   // 绿色正常
        Orange,  // 橙色警戒
        Red,     // 红色告警
        Gray     // 灰色中性/未生效/--
    };

    QLabel *createBadge(const QString &text, BadgeType type);
    void updateBadge(QLabel *lbl, const QString &text, BadgeType type);

    void addRow(QGridLayout *grid, int row, const QString &label, const QString &value,
                QLabel **outValueLbl = nullptr, const QString &valColor = "#e2e8f0");
    void addBadgeRow(QGridLayout *grid, int row, const QString &label, const QString &badgeText,
                     BadgeType type, QLabel **outBadgeLbl = nullptr);
    void addMultiBadgeRow(QGridLayout *grid, int row, const QString &label,
                          const QList<QPair<QString, BadgeType>> &badges,
                          QList<QLabel*> *outBadgeList = nullptr);

    // ── 选项卡 ──
    QTabWidget *m_tabWidget = nullptr;

    // ── 第 1 列：机器人运行状态 ──
    QLabel *m_valRunMode       = nullptr;
    QLabel *m_valTotalDist     = nullptr;
    QLabel *m_valTodayDist     = nullptr;
    QLabel *m_valTotalTime     = nullptr;
    QLabel *m_valThisTime      = nullptr;
    QLabel *m_valCtrlVolt      = nullptr;
    QLabel *m_valCtrlTemp      = nullptr;
    QLabel *m_valCtrlHumi      = nullptr;
    QLabel *m_valBatteryPct    = nullptr;
    QLabel *m_valBatteryTemp   = nullptr;
    QLabel *m_valBatteryVolt   = nullptr;
    QLabel *m_valBatteryCurr   = nullptr;
    QLabel *m_valCycleCount    = nullptr;
    QLabel *m_valBatteryCustom = nullptr;
    QLabel *m_valChargeStatus  = nullptr;
    QLabel *m_valMaxChargeVolt = nullptr;
    QLabel *m_valMaxChargeCurr = nullptr;

    // ── 第 2 列：机器人运动状态 ──
    QLabel *m_valPosition      = nullptr;
    QLabel *m_valActualSpeed   = nullptr;
    QLabel *m_valTargetSpeed   = nullptr;
    QLabel *m_badgeConfidence  = nullptr;
    QLabel *m_badgeBlocked     = nullptr;
    QLabel *m_valNearestObs    = nullptr;
    QLabel *m_valDynObs        = nullptr;
    QLabel *m_valObsFrameId    = nullptr;
    QLabel *m_valObsStamp      = nullptr;
    QLabel *m_valBlockSensorId = nullptr;
    QLabel *m_badgeBrake       = nullptr;
    QLabel *m_badgeEstop1      = nullptr;
    QLabel *m_badgeEstop2      = nullptr;
    QLabel *m_valGyro          = nullptr;
    QLabel *m_valControlMode   = nullptr;
    QLabel *m_badgeStopped     = nullptr;

    // ── 第 3 列：机器人导航状态 ──
    QLabel *m_valNavType          = nullptr;
    QLabel *m_valNavStatus        = nullptr;
    QLabel *m_valCurStation       = nullptr;
    QLabel *m_valTargetStation    = nullptr;
    QLabel *m_valTargetCoord      = nullptr;
    QLabel *m_valDistToTarget     = nullptr;
    QLabel *m_valPassedStations   = nullptr;
    QLabel *m_valUnpassedStations = nullptr;
    QLabel *m_valLocStatus        = nullptr;
    QLabel *m_valMapLoadStatus    = nullptr;
    QLabel *m_valSlamStatus       = nullptr;
    QLabel *m_valArea             = nullptr;
    QLabel *m_valCurMap           = nullptr;

    // ── 第 4 列：机器人基本信息 ──
    QLabel *m_valRobotId          = nullptr;
    QLabel *m_valRobotName        = nullptr;
    QLabel *m_valRobotRemark      = nullptr;
    QLabel *m_valRobotModel       = nullptr;
    QLabel *m_valRobokitVer       = nullptr;
    QLabel *m_valFirmwareVer      = nullptr;
    QLabel *m_valGyroVer          = nullptr;
    QLabel *m_valTcpApiVer        = nullptr;
    QLabel *m_valModbusVer        = nullptr;
    QLabel *m_valMapVer           = nullptr;
    QLabel *m_valModelVer         = nullptr;
    QLabel *m_valMac              = nullptr;
    QLabel *m_valWiredNicModel    = nullptr;
    QLabel *m_valWirelessMac      = nullptr;
    QLabel *m_valWirelessNicModel = nullptr;
    QLabel *m_valBaseFrameId      = nullptr;
    QLabel *m_valBaseStamp        = nullptr;

    // ── 下部客户端连接与网络信息 ──
    QLabel *m_lblClientTitle      = nullptr;
    QLabel *m_lblConnList         = nullptr;
    QLabel *m_lblDetail           = nullptr;
    QLabel *m_lblFooter           = nullptr;

    // ── 内部状态与定时器 ──
    QTimer    *m_timer            = nullptr;
    qint64     m_startTimeMs      = 0;
    QString    m_mapId;
    QString    m_mapName;
    bool       m_hasMqttData      = false;
};

#endif // ROBOTSTATUSDIALOG_H
