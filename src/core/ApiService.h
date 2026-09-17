#ifndef APISERVICE_H
#define APISERVICE_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QHttpMultiPart>
#include <functional>

/**
 * @brief 健壮从 QJsonValue 提取 ID 字符串的内联辅助函数 (向下兼容整数与字符串两种格式)
 */
inline QString jsonIdStr(const QJsonValue &v) {
    if (v.isString()) {
        return v.toString().trimmed();
    }
    if (v.isDouble()) {
        return QString::number(v.toInteger());
    }
    return QString();
}

/**
 * @brief 全局 HTTP / RESTful API 客户端核心服务 (Singleton)
 * 
 * 架构规范与特性：
 * 1. 统一管理后端请求头，具备自动注入 Bearer Token 鉴权能力；
 * 2. 具备传输超时控制 (TransferTimeout) 防网络挂死；
 * 3. 统一错误捕获与日志节流防刷屏；
 * 4. 消除各接口中冗余的 QUrlQuery 影子逻辑；
 * 5. 100% 保持所有原有业务层 API 签名向下兼容。
 */
class ApiService final : public QObject
{
    Q_OBJECT

public:
    static ApiService &instance();

    using Callback     = std::function<void(int code, const QJsonObject &data)>;
    using RawCallback  = std::function<void(int code, const QByteArray &data)>;
    using FileCallback = std::function<void(bool ok, const QByteArray &data)>;

    static constexpr int kDefaultTimeoutMs = 15000;  ///< 默认常规接口超时 (15s)
    static constexpr int kUploadTimeoutMs  = 60000;  ///< 地图与大文件上传超时 (60s)

    // ========== 通用基础请求 ==========
    void get(const QString &url, const Callback &cb, int port = 8048, bool withToken = true);
    void post(const QString &url, const QJsonObject &data, const Callback &cb, int port = 8048, bool withToken = true);
    void put(const QString &url, const QJsonObject &data, const Callback &cb, int port = 8048, bool withToken = true);
    void del(const QString &url, const Callback &cb, int port = 8048, bool withToken = true);

    // 文件上传与下载
    void uploadFile(const QString &url, const QString &filePath, const QString &fieldName,
                    const Callback &cb, int port = 8048);
    void uploadMapPgm(const QString &mapId, const QByteArray &pgmData, const Callback &cb);
    void downloadFile(const QString &url, const FileCallback &cb, int port = 8048, bool withToken = false);
    void getRaw(const QString &url, const RawCallback &cb, int port = 8048, bool withToken = false);

    // ========== AGV 控制接口 ==========
    // 端口 8062 控制通道
    void startMapping(const QString &mapName, const Callback &cb);
    void saveMap(const Callback &cb);
    void startReflectorMapping(const Callback &cb);
    void resetReflectorAll(const Callback &cb);
    void resetReflectorLocal(const Callback &cb);
    void stopReflectorMapping(const Callback &cb);
    void cancelMapping(const Callback &cb);
    void cancelReflectorMapping(const Callback &cb);
    // 端口 8061 工作通道
    void suspendWork(const Callback &cb);
    void continueWork(const Callback &cb);
    // 自检与终端通道
    void startCheck(const Callback &cb);
    void startTerminal(const Callback &cb);
    void stopTerminal(const Callback &cb);
    // 参数配置
    void getAllConfigsPreserve(const RawCallback &cb);
    void modifyParam(const QJsonObject &params, const Callback &cb);
    void getConfigSummary(const FileCallback &cb);
    void updateConfigSummary(const QString &filePath, const Callback &cb);

    // ========== 设备接口 ==========
    void listCarAll(const QJsonObject &query, const Callback &cb);
    void listCarAll(const Callback &cb);
    void listCar(const QJsonObject &query, const Callback &cb);
    void getCar(const QString &carId, const Callback &cb);
    void addCar(const QJsonObject &data, const Callback &cb);
    void linkCar(const QJsonObject &data, const Callback &cb);
    void updateCar(const QJsonObject &data, const Callback &cb);
    void changeCarStatus(const QString &carId, const QString &mapId, const QString &status, const Callback &cb);
    void delCar(const QString &carId, const Callback &cb);
    void checkCertification(const QString &ckey, const Callback &cb);
    void sendMqtt(const QJsonObject &data, const Callback &cb);
    void editCarMap(const QString &mapId, const Callback &cb);

    // ========== 地图接口 ==========
    void listMapAll(const QJsonObject &query, const Callback &cb);
    void listMap(const QJsonObject &query, const Callback &cb);
    void getMap(const QString &mapId, const Callback &cb);
    void updateMap(const QJsonObject &data, const Callback &cb);
    void delMap(const QString &mapId, const Callback &cb);
    void downloadMapResources(const QString &mapId, const FileCallback &cb);

    // ========== PCD / 地图文件接口 ==========
    struct IndoorMapUploadParams {
        QString mapName;
        QString pgmFilePath;
        QString yamlFilePath;
        QString pbStreamFilePath;
        QString reflectorBinFilePath;   ///< 可选
        QString reflectorJsonFilePath;  ///< 可选
    };
    void downloadPcd(const QJsonObject &query, const Callback &cb);
    void addAgvMap(const QJsonObject &data, const Callback &cb);
    void uploadIndoorMap(const IndoorMapUploadParams &params, const Callback &cb, int port = 8048);
    void uploadSmap(const QString &mapName, const QString &filePath, const Callback &cb, int port = 8048);

    // ========== 地图编辑接口 ==========
    void saveEditedMap(const QJsonObject &data, const Callback &cb);
    void loadEditedMap(const Callback &cb);
    void loadEditedMap(const QJsonObject &data, const Callback &cb);
    void getReflectorColumnInfo(const QString &mapId, const Callback &cb);

    // ========== 导航 / 位姿接口 ==========
    void initPose(const QString &carId, const QJsonObject &pose, const Callback &cb);
    void navigateTo(const QString &locationId, const Callback &cb);

    // ========== 任务接口 ==========
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

    // ========== 任务日志接口 ==========
    void listTasklog(const QJsonObject &query, const Callback &cb);
    void getTasklog(const QString &tasklogId, const Callback &cb);

    // ========== 登录认证接口 ==========
    void login(const QString &username, const QString &password, const Callback &cb);

private:
    explicit ApiService(QObject *parent = nullptr);
    ~ApiService() override;
    ApiService(const ApiService &) = delete;
    ApiService &operator=(const ApiService &) = delete;

    QString buildUrl(const QString &path, int port) const;
    QNetworkRequest buildRequest(const QString &url, bool withToken = true, int timeoutMs = kDefaultTimeoutMs) const;

    void handleReply(QNetworkReply *reply, const Callback &cb);
    void handleRawReply(QNetworkReply *reply, const RawCallback &cb);
    void handleFileReply(QNetworkReply *reply, const FileCallback &cb);

    static QString appendQuery(const QString &basePath, const QJsonObject &query);

    QNetworkAccessManager *m_manager = nullptr;
};

#endif // APISERVICE_H
