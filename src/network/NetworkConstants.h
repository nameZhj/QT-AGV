/**
 * @file    NetworkConstants.h
 * @module  DataComm
 * @brief   Centralized definitions of REST endpoints, MQTT topics, and JSON payload keys.
 * @deps    None (Header-only constants)
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QString>

/**
 * @brief 集中管理网络 API 端点、HTTP 字段名、MQTT Topic 及 JSON 键名常量
 * 消除散落在 C++ 代码中的硬编码字符串。
 */
namespace NetworkConstants {

// ── HTTP 请求端点 (Endpoints) ──
namespace Endpoints {

    // 鉴权与系统登录
    namespace Auth {
        inline const QString Login              = QStringLiteral("/login");
        inline const QString CheckCertBase      = QStringLiteral("/robot/car/checkC/");
    }

    // 车辆设备接口 (HTTP 默认 8048)
    namespace Car {
        inline const QString ListAll            = QStringLiteral("/robot/car/listAll");
        inline const QString List               = QStringLiteral("/robot/car/list");
        inline const QString Base               = QStringLiteral("/robot/car/");
        inline const QString Root               = QStringLiteral("/robot/car");
        inline const QString Link               = QStringLiteral("/robot/car/link");
        inline const QString ChangeStatus       = QStringLiteral("/robot/car/changeStatus");
        inline const QString SendMqtt           = QStringLiteral("/robot/mqtt/pub");
        inline const QString EditMapBase        = QStringLiteral("/robot/car/edit/map/");
    }

    // 地图接口 (HTTP 默认 8048)
    namespace Map {
        inline const QString ListAll            = QStringLiteral("/robot/map/listAll");
        inline const QString List               = QStringLiteral("/robot/map/list");
        inline const QString Base               = QStringLiteral("/robot/map/");
        inline const QString Root               = QStringLiteral("/robot/map");
        inline const QString DownloadResources  = QStringLiteral("/robot/map/download/resources/");
        inline const QString DownloadPcd        = QStringLiteral("/common/download/resourcePCDpath");
        inline const QString AddAgvMap          = QStringLiteral("/robot/indoor/map/web/add");
        inline const QString ReflectorColumnInfo= QStringLiteral("/robot/indoor/map/reflectorColumnInfo/get");
    }

    // 任务编排接口 (HTTP 默认 8048)
    namespace Task {
        inline const QString List               = QStringLiteral("/robot/task/list");
        inline const QString Base               = QStringLiteral("/robot/task/");
        inline const QString Root               = QStringLiteral("/robot/task");
        inline const QString InfoBase           = QStringLiteral("/robot/task/taskInfo/");
        inline const QString ListAllStatusBase  = QStringLiteral("/robot/navigation/listAllStatus/");
        inline const QString CurrentTask        = QStringLiteral("/robot/navigation/currentTask");
        inline const QString LoadQueue          = QStringLiteral("/robot/navigation/loadTaskQueue");
        inline const QString Start              = QStringLiteral("/robot/navigation/start");
        inline const QString QuitQueue          = QStringLiteral("/robot/navigation/quitTaskQueue");
        inline const QString Command            = QStringLiteral("/robot/navigation/command");
        inline const QString LogList            = QStringLiteral("/tasklog/list");
        inline const QString LogBase            = QStringLiteral("/tasklog/");
    }

    // 任务链接口 (HTTP 默认 8048)
    namespace TaskChain {
        inline const QString Save               = QStringLiteral("/task/chain/save");
        inline const QString BatchSave          = QStringLiteral("/task/chain/batchSave");
        inline const QString Base               = QStringLiteral("/task/chain/");
        inline const QString List               = QStringLiteral("/task/chain/list");
        inline const QString SendTask           = QStringLiteral("/task/chain/sendTask");
        inline const QString Control            = QStringLiteral("/task/chain/control");
        inline const QString SendNextTask       = QStringLiteral("/task/chain/sendNextTask");
        inline const QString TaskLog            = QStringLiteral("/task/chain/taskLog");
    }

    // 音频管理 (HTTP 默认 8048)
    namespace Audio {
        inline const QString List               = QStringLiteral("/robot/audio/list");
        inline const QString Upload             = QStringLiteral("/robot/audio/upload");
        inline const QString DownloadBase       = QStringLiteral("/robot/audio/download/");
        inline const QString Base               = QStringLiteral("/robot/audio/");
    }

    // 机器人导航与位姿 (HTTP 默认 8048)
    namespace Navigation {
        inline const QString Move               = QStringLiteral("/robot/navigation/move");
        inline const QString Control            = QStringLiteral("/robot/navigation/control");
        inline const QString InitPose           = QStringLiteral("/robot/navigation/initPose");
        inline const QString NavigateTo         = QStringLiteral("/robot/navigation/navigateTo");
    }

    // AGV 控制器硬件协议 (Port 8061)
    namespace Controller8061 {
        inline const QString ActionControl      = QStringLiteral("/action_control");
        inline const QString SetRobotFixPath    = QStringLiteral("/set_robot_fix_path");
        inline const QString SetThirdFixPath    = QStringLiteral("/set_third_fix_path");
        inline const QString RobotRunStatus     = QStringLiteral("/robot_run_status");
        inline const QString QueryActionId      = QStringLiteral("/query_action_id");
        inline const QString SetInitPose        = QStringLiteral("/set_init_pose");
        inline const QString SetSpeed           = QStringLiteral("/set_speed");
        inline const QString ModifyPathSpeed    = QStringLiteral("/modify_path_speed");
    }

    // 建图与参数管理通道 (Port 8062)
    namespace Mapping8062 {
        inline const QString StartMapping       = QStringLiteral("/start_mapping");
        inline const QString SaveMap            = QStringLiteral("/save_map");
        inline const QString CancelMapping      = QStringLiteral("/cancel_mapping");
        inline const QString StartReflector     = QStringLiteral("/start_reflector_mapping");
        inline const QString StopReflector      = QStringLiteral("/stop_reflector_mapping");
        inline const QString ResetReflectorAll  = QStringLiteral("/reset_reflector_all");
        inline const QString ResetReflectorLocal= QStringLiteral("/reset_reflector_local");
        inline const QString CancelReflector    = QStringLiteral("/cancel_reflector_mapping");
        inline const QString GetAllConfigs      = QStringLiteral("/get_all_configs_preserve");
        inline const QString GetConfigMetadata  = QStringLiteral("/get_config_metadata");
        inline const QString ModifyParam        = QStringLiteral("/modify_param");
        inline const QString GetConfigSummary   = QStringLiteral("/get_config_summary");
        inline const QString UpdateConfigSummary= QStringLiteral("/update_config_summary");
    }
}

// ── MQTT 主题模板 (MQTT Topics) ──
namespace MqttTopics {
    inline const QString Online                 = QStringLiteral("/rob/online");
    inline const QString NavigateStatus         = QStringLiteral("/rob/navigate/status");
    inline const QString WildcardRobotInfo       = QStringLiteral("/rob/+/info");
    inline const QString WildcardRobotPose       = QStringLiteral("/rob/+/pose");
    inline const QString LegacyInfo              = QStringLiteral("/info");

    inline QString robotInfo(const QString &carId = QStringLiteral("1")) {
        return QStringLiteral("/rob/%1/info").arg(carId.isEmpty() ? QStringLiteral("1") : carId);
    }
    inline QString robotPose(const QString &carId) {
        return QStringLiteral("/rob/%1/pose").arg(carId);
    }
    inline QString robotCrud(const QString &carId) {
        return QStringLiteral("/rob/%1/crud").arg(carId);
    }
    inline QString robotPointType(const QString &carId) {
        return QStringLiteral("/rob/%1/pointType").arg(carId);
    }
}

// ── JSON 报文常用键名 (JSON Keys) ──
namespace JsonKeys {
    inline constexpr auto Code        = "code";
    inline constexpr auto Msg         = "msg";
    inline constexpr auto Data        = "data";
    inline constexpr auto Rows        = "rows";
    inline constexpr auto Total       = "total";

    inline constexpr auto CarId       = "carId";
    inline constexpr auto MapId       = "mapId";
    inline constexpr auto MapName     = "mapName";
    inline constexpr auto TaskId      = "taskId";
    inline constexpr auto TaskName    = "taskName";

    inline constexpr auto Status      = "status";
    inline constexpr auto Type        = "type";
    inline constexpr auto ActionType  = "action_type";
    inline constexpr auto ActionId    = "action_id";

    inline constexpr auto X           = "x";
    inline constexpr auto Y           = "y";
    inline constexpr auto Yaw         = "yaw";
    inline constexpr auto Speed       = "speed";
}

} // namespace NetworkConstants
