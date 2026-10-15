#pragma once

#include <QString>

/**
 * @brief 全局应用级别常量集中管理类
 * 消除 UI 尺寸、定时器轮询周期、文件过滤器及静态资源路径的硬编码。
 */
namespace AppConstants {

// ── 软件基础信息 ──
inline constexpr auto AppName       = "QT-AGV";
inline constexpr auto AppTitle     = "QT-AGV  移动机器人管理平台";
inline constexpr auto AppVersion   = "1.0.0";
inline constexpr auto LogFileName  = "qt-agv.log";

// ── 窗口尺寸规范 ──
namespace Window {
    inline constexpr int DefaultWidth  = 1440;
    inline constexpr int DefaultHeight = 900;
    inline constexpr int MinWidth      = 1024;
    inline constexpr int MinHeight     = 640;
}

// ── 定时器与心跳轮询间隔 (单位: ms) ──
namespace Intervals {
    inline constexpr int HeartbeatMs        = 1000;  ///< 设备心跳检测周期
    inline constexpr int DeviceScanMs       = 3000;  ///< 设备自动扫描周期
    inline constexpr int StatusRefreshMs    = 500;   ///< 机器人遥测状态刷新周期
    inline constexpr int TaskStatusPollMs   = 1000;  ///< 任务执行状态轮询周期
    inline constexpr int TeleopKeepAliveMs  = 100;   ///< 遥控指令心跳保护下发周期
    inline constexpr int ToastDefaultMs     = 2500;  ///< Toast 默认显示时长
}

// ── 文件扩展名与选择器过滤器 ──
namespace Filters {
    inline const QString PgmFilter   = QStringLiteral("PGM 地图文件 (*.pgm);;所有文件 (*.*)");
    inline const QString YamlFilter  = QStringLiteral("YAML 配置文件 (*.yaml *.yml);;所有文件 (*.*)");
    inline const QString SmapFilter  = QStringLiteral("SMAP 地图包 (*.smap);;所有文件 (*.*)");
    inline const QString ZipFilter   = QStringLiteral("ZIP 压缩文件 (*.zip);;所有文件 (*.*)");
    inline const QString ImageFilter = QStringLiteral("地图图像 (*.png *.jpg *.bmp *.pgm);;所有文件 (*.*)");
    inline const QString AudioFilter = QStringLiteral("音频文件 (*.wav *.mp3);;所有文件 (*.*)");
}

// ── 统一 Qt 内部资源 URI 路径 ──
namespace Resources {
    inline const QString AppIconSvg    = QStringLiteral(":/app-icon.svg");
    inline const QString RobotIconSvg  = QStringLiteral(":/ic-robot1.svg");
    inline const QString CheckSvg      = QStringLiteral(":/check.svg");
    inline const QString ResetViewSvg  = QStringLiteral(":/ic-reset-view.svg");
}

} // namespace AppConstants
