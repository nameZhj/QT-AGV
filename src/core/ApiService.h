#ifndef APISERVICE_H
#define APISERVICE_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QHttpMultiPart>
#include <functional>

// 辅助函数: 从 QJsonValue 获取字符串形式的 ID (处理数字和字符串两种类型)
inline QString jsonIdStr(const QJsonValue &v) {
    if (v.isString()) return v.toString();
    if (v.isDouble()) return QString::number(v.toInt());
    return QString();
}

// HTTP API 服务封装，对应 Electron 中的 @/utils/request.js (axios)
// 所有后端接口统一通过此类调用
class ApiService : public QObject
{
    Q_OBJECT
public:
    static ApiService &instance();

    using Callback = std::function<void(int code, const QJsonObject &data)>;
    using RawCallback = std::function<void(int code, const QByteArray &data)>;
    using FileCallback = std::function<void(bool ok, const QByteArray &data)>;

    // ========== 通用请求 ==========
    void get(const QString &url, const Callback &cb, int port = 8048, bool withToken = true);
    void post(const QString &url, const QJsonObject &data, const Callback &cb, int port = 8048, bool withToken = true);
    void put(const QString &url, const QJsonObject &data, const Callback &cb, int port = 8048, bool withToken = true);
    void del(const QString &url, const Callback &cb, int port = 8048, bool withToken = true);

    // 文件上传
    void uploadFile(const QString &url, const QString &filePath, const QString &fieldName,
                    const Callback &cb, int port = 8048);

    // 编辑地图：上传修改后的 pgm 栅格图(内存字节流，不写本地临时文件)
    // 对应 Vue editMap.vue 的 uploadPgm() / POST /robot/indoor/map/pgm/update
    void uploadMapPgm(const QString &mapId, const QByteArray &pgmData, const Callback &cb);

    // 文件下载 (返回原始字节) — withToken=false for static resource servers
    void downloadFile(const QString &url, const FileCallback &cb, int port = 8048, bool withToken = false);

    // 原始 GET (返回文本，用于 getAllConfigsPreserve)
    void getRaw(const QString &url, const RawCallback &cb, int port = 8048, bool withToken = false);

    // ========== AGV 控制接口 (对应 @/api/agvControl.js) ==========
    // 端口 8062 的接口
    void startMapping(const QString &mapName, const Callback &cb);
    void saveMap(const Callback &cb);
    void startReflectorMapping(const Callback &cb);
    void resetReflectorAll(const Callback &cb);
    void resetReflectorLocal(const Callback &cb);
    void stopReflectorMapping(const Callback &cb);
    void cancelMapping(const Callback &cb);
    void cancelReflectorMapping(const Callback &cb);
    // 端口 8061 的接口
    void suspendWork(const Callback &cb);
    void continueWork(const Callback &cb);
    // 自检/终端
    void startCheck(const Callback &cb);
    void startTerminal(const Callback &cb);
    void stopTerminal(const Callback &cb);
    // 配置
    void getAllConfigsPreserve(const RawCallback &cb);
    void modifyParam(const QJsonObject &params, const Callback &cb);
    void getConfigSummary(const FileCallback &cb);
    void updateConfigSummary(const QString &filePath, const Callback &cb);

    // ========== 设备接口 (对应 @/api/robot/car.js) ==========
    void listCarAll(const QJsonObject &query, const Callback &cb);
    void listCarAll(const Callback &cb); // 便捷重载，无查询参数
    void listCar(const QJsonObject &query, const Callback &cb);
    void getCar(const QString &carId, const Callback &cb);
    void addCar(const QJsonObject &data, const Callback &cb);
    void addCarWithoutKey(const QJsonObject &data, const Callback &cb); // POST /robot/car/withoutCertificationKey
    void linkCar(const QJsonObject &data, const Callback &cb);
    void linkCarWithoutKey(const QJsonObject &data, const Callback &cb); // PUT /robot/car/link/withoutCertificationKey
    void updateCar(const QJsonObject &data, const Callback &cb);
    void changeCarStatus(const QString &carId, const QString &mapId, const QString &status, const Callback &cb);
    void delCar(const QString &carId, const Callback &cb);
    void checkCertification(const QString &ckey, const Callback &cb);
    void sendMqtt(const QJsonObject &data, const Callback &cb);

    // ========== 地图接口 (对应 @/api/robot/map.js) ==========
    void listMapAll(const QJsonObject &query, const Callback &cb);
    void listMap(const QJsonObject &query, const Callback &cb);
    void getMap(const QString &mapId, const Callback &cb);
    void updateMap(const QJsonObject &data, const Callback &cb);
    void delMap(const QString &mapId, const Callback &cb);

    // ========== PCD / 地图文件接口 (对应 @/api/robot/pcd.js) ==========
    void downloadPcd(const QJsonObject &query, const Callback &cb);
    void addAgvMap(const QJsonObject &data, const Callback &cb);
    void sendMap2Car(const QJsonObject &params, const Callback &cb);

    // ========== 地图编辑接口 (对应 @/api/robot/mapEdit.js) ==========
    void saveEditedMap(const QJsonObject &data, const Callback &cb);
    void loadEditedMap(const QJsonObject &data, const Callback &cb);
    void getReflectorColumnInfo(const QString &mapId, const Callback &cb);

    // ========== 导航/位姿接口 ==========
    void initPose(const QString &carId, const QJsonObject &pose, const Callback &cb);
    void navigateTo(const QString &locationId, const Callback &cb);


    // ========== 任务接口 (对应 @/api/robot/task.js) ==========
    void listTask(const QJsonObject &query, const Callback &cb);
    void getTask(const QString &taskId, const Callback &cb);
    void getTaskInfo(const QString &taskId, const Callback &cb);
    void addTask(const QJsonObject &data, const Callback &cb);
    void updateTask(const QJsonObject &data, const Callback &cb);
    void delTask(const QString &taskId, const Callback &cb);
    void listTaskAllStatus(const QString &carId, const Callback &cb);
    void getCurrentTask(const QJsonObject &query, const Callback &cb);
    void loadTask(const QJsonObject &data, const Callback &cb);
    void startTask(const QJsonObject &data, const Callback &cb);
    void startTask(const QString &taskId, const Callback &cb);
    void removeTask(const QJsonObject &data, const Callback &cb);
    void commandTask(const QJsonObject &data, const Callback &cb);

    // ========== 任务日志接口 (对应 @/api/robot/tasklog.js) ==========
    void listTasklog(const QJsonObject &query, const Callback &cb);
    void getTasklog(const QString &tasklogId, const Callback &cb);

    // ========== 登录接口 ==========
    void login(const QString &username, const QString &password, const Callback &cb);
    void listCarAllNoLogin(const Callback &cb);

private:
    explicit ApiService(QObject *parent = nullptr);
    ~ApiService() override;
    ApiService(const ApiService &) = delete;
    ApiService &operator=(const ApiService &) = delete;

    QString buildUrl(const QString &path, int port) const;
    QNetworkRequest buildRequest(const QString &url, bool withToken = true) const;

    void handleReply(QNetworkReply *reply, const Callback &cb);
    void handleRawReply(QNetworkReply *reply, const RawCallback &cb);
    void handleFileReply(QNetworkReply *reply, const FileCallback &cb);

    QNetworkAccessManager *m_manager;
};

#endif // APISERVICE_H
