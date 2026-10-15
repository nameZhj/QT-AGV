#include "ConfigMetadataHelper.h"
#include "core/MotionConfigManager.h"
#include "core/AlignConfigManager.h"

#include <QJsonDocument>
#include <QMap>

QPair<QString, QString> ConfigMetadataHelper::fieldMetaFor(const QString &key) {
  static const QMap<QString, QPair<QString, QString>> meta = {
      {"child_frame_id", {"子坐标系 ID", "子坐标系名称"}},
      {"frame_id", {"坐标系 ID", "坐标系名称"}},
      {"pose", {"安装位置", "x/y/偏转角（m/rad）"}},
      {"operational_mode", {"程序运行模式", "location=定位，mapping=建图"}},
      {"relocation_mode", {"自动重定位模式", "force=强制, auto=自动"}},
      {"if_opt_log", {"是否输出调试信息", ""}},
      {"if_pub_status", {"是否广播算法运行状态", ""}},
      {"if_pub_tf", {"是否发布 TF 树", ""}},
      {"if_pub_pose", {"是否发布位姿", ""}},
      {"if_enable_control_topic", {"是否允许 topic 控制算法", ""}},
      {"if_pub_MarkerArray", {"是否发布 MarkerArray", ""}},
      {"if_pub_high_reflection_pc", {"是否发布高反点云", ""}},
      {"if_map_editable", {"地图是否可编辑", ""}},
      {"if_pub_intersections_pc", {"是否发布拟合圆心", ""}},
      {"base_frame", {"参考坐标系名称", "全局地图坐标系"}},
      {"publish_frame", {"发布坐标系名称", "二次定位基坐标系"}},
      {"carto_frame", {"Carto 坐标系名称", ""}},
      {"Lidar_in_baselink_x", {"激光雷达安装位置 x", "单位 m"}},
      {"Lidar_in_baselink_y", {"激光雷达安装位置 y", "单位 m"}},
      {"Lidar_in_baselink_theta", {"激光雷达安装角度", "单位 rad"}},
      {"host", {"雷达 IP 地址", ""}},
      {"port", {"端口号", ""}},
      {"range_min", {"最小探测距离", "单位 m"}},
      {"range_max", {"最大探测距离", "单位 m"}},
      {"offset_angle", {"偏移角度", "单位度"}},
      {"filterswitch", {"滤波开关", ""}},
      {"NTPswitch", {"NTP 时间同步开关", ""}},
      {"NORswitch", {"NOR 模式开关", ""}},
      {"lidar_topic_name", {"激光雷达话题名称", ""}},
      {"pose_topic_name", {"位姿话题名称", ""}},
      {"status_topic_name", {"算法运行状态话题", ""}},
      {"map_save_path", {"地图存储路径", ""}},
      {"map_filename", {"地图文件名", ""}},
      {"intensity_down_threshold", {"回光强度中位数最低值", ""}},
      {"dsu_merge_max_distance", {"DSU 聚合最大距离", "单位 m"}},
      {"dsu_merge_max_points", {"DSU 聚合最大点数", ""}},
      {"dsu_merge_min_points", {"DSU 聚合最小点数", ""}},
      {"merge_distance", {"合并反光板最大距离", "单位 m"}},
      {"deadzone_radius", {"死区半径", "单位 m"}},
      {"reflect_cylinder_radius", {"反光柱半径", "单位 m"}},
      {"graph_edge_threshold", {"构造边最远距离", "单位 m"}},
      {"lidar_frequency", {"清理地图的周期", "单位 Hz"}},
      {"log_level", {"日志屏蔽等级", "1-4，4 为全部屏蔽"}},
      {"polling_interval", {"轮询时间间隔", "单位秒"}},
      {"if_load_demo", {"是否加载 demo", ""}},
      {"rotate_speed_x", {"旋转线速度", "单位 m/s"}},
      {"rotate_speed_theta", {"旋转速度", "单位 rad/s"}},
      {"low_speed_x", {"低速线速度", "单位 m/s"}},
      {"low_speed_theta", {"低速角速度", "单位 rad/s"}},
      {"high_speed_theta", {"高速角速度", "单位 rad/s"}},
      {"cruise_reset_deadzone", {"回到出发点判断阈值", "单位 m"}},
      {"deceleration_distance", {"减速距离", "单位 m"}},
      {"sp_P", {"线速度 P 参数", "PID 比例"}},
      {"sp_D", {"线速度 D 参数", "PID 微分"}},
      {"ang_P", {"角速度 P 参数", "PID 比例"}},
      {"ang_D", {"角速度 D 参数", "PID 微分"}},
      {"ang_I", {"角速度 I 参数", "PID 积分"}},
      {"dis_aim_error", {"距离目标误差", "单位 m"}},
      {"ang_aim_error", {"角度目标误差", "单位 rad"}},
      {"speed_max", {"最大容许速度", "单位 m/s"}},
      {"speed_min", {"最小容许速度", "单位 m/s"}},
      {"run_pid", {"行进过程参数", ""}},
      {"turn_pid", {"原地旋转参数", ""}},
      {"back_pid", {"后退参数", ""}},
      {"imu_pid", {"IMU 控制参数", ""}},
      {"topic_name", {"话题名称", ""}},
      {"command_name", {"命令名称", ""}},
      {"cmd_vel", {"cmd_vel 话题名称", ""}},
      {"stop_time", {"到点后停车间隔", "单位秒"}},
      {"timer_duration", {"定时器周期", "单位秒"}},
      {"JSON_FILE_PATH", {"JSON 文件路径", ""}},
      {"MAP_YAML_PATH", {"地图 YAML 路径", ""}},
      {"reflector_zone", {"反光柱区域", "xmax/xmin/ymax/ymin (m)"}},
      {"scanner_ip", {"雷达 IP 地址", ""}},
      {"local_ip", {"本机 IP 地址", ""}},
      {"lidar_pose", {"雷达在车体的位姿", "x/y/角度(rad)"}},
      {"half_body_size", {"车体半长半宽", "[半长, 半宽] 单位 m"}},
      {"wheel_base", {"车体轮距", "单位 m"}},
      {"wheel_radius", {"车轮半径", "单位 m"}},
      {"table_length", {"左右货架腿中心点距离", "单位 m"}},
      {"table_match_threshold", {"反光贴匹配阈值", "单位 m"}},
      {"max_detection_range", {"最远探测距离", "单位 m"}},
      {"min_point_threshold", {"最少检出点数", ""}},
      {"length_dock", {"出库距离", "单位 m"}},
      {"anti_align_depth", {"正走入深度", "单位米"}},
      {"cylinder_length", {"前后货架腿中心点距离", "单位 m"}},
      {"cylinder_recognition_range", {"货架腿识别范围", ""}},
      {"lift_topic_name", {"顶升话题名称", ""}},
      {"cylinder_width", {"左右货架腿中心点距离", "单位 m"}},
      {"dis_goal_error", {"接近点后直线调整距离", "单位 m"}},
      {"length_undock", {"二次出库调整距离", "单位 m"}},
      {"rambler_bezier_step", {"贝塞尔步长", ""}},
      {"straight_line_cruise_governor_polling_interval",
       {"直线巡航调速器轮询间隔", "单位秒"}},
      {"ignore_area_a", {" 屏蔽区域直径", "单位 m"}},
      {"ignore_area_x", {"屏蔽区域距离车体中心的纵向距离", "单位 m"}},
      {"ignore_area_y", {"屏蔽区域距离车体中心的横向距离", "单位 m"}},
      {"lead", {"顶升机构丝杠导程", "单位 m"}},
      {"nx", {"运动电机减速比", ""}},
      {"lift_nx", {"顶升电机减速比", ""}},
      {"lift_max", {"顶升最大高度", "单位m，可用于软保护"}},
      {"lift_min", {"顶升最小高度", "单位m，可用于软保护"}},
      {"lift_joy_speed", {"遥控顶升速度", "单位rad/s"}},

      // 运动控制配置
      {"max_speed", {"最大线速度配置值", "单位 m/s"}},
      {"max_acceleration", {"最大线加速度配置值", "单位 m/s²"}},
      {"max_deceleration", {"最大线减速度的幅值", "单位 m/s²，使用正数"}},
      {"max_angular_speed", {"最大角速度配置值", "单位 °/s；此处使用角度制"}},
      {"max_angular_acceleration", {"最大角加减速度配置值", "单位 °/s²；此处使用角度制"}},
      {"arrival_distance_tolerance", {"到达目标允许的距离误差", "单位 m"}},
      {"arrival_angle_tolerance", {"到达目标允许的角度误差", "单位 °；此处使用角度制"}},
      {"route_width", {"路线宽度配置值", "单位 m；全宽/半宽及控制用途尚未接入定义"}},

      // 障碍物配置
      {"id_stop_range_threshold",
       {"停车区间扩展量",
        "停车区间扩展量 [dx, dy]，分别为车体边界外纵向、横向的裕量，不是区域总长宽，单位 m"}},
      {"id_deceleration_range_threshold",
       {"减速区间基础扩展量",
        "减速区间基础扩展量 [dx, dy]；实际减速边界还可能根据速度和制动距离扩大，单位 m"}},
  };
  return meta.value(key, {"", ""});
}

QString ConfigMetadataHelper::determineProfile(const QJsonObject &entry) {
  QString name = entry.value("name").toString().toLower();
  QString path = entry.value("file_path").toString().toLower();

  if (name == "motion_control" || name.contains("motion_control") ||
      path.contains("motion_control") || name == "运动控制")
    return "MOTION_CONTROL";
  if (path.contains("dispatch/config/config.yaml"))
    return "DISPATCH";
  if (path.contains("secondary_localization_reflector/config/config.yaml"))
    return "SECONDARY_LOC";
  if (name.contains("r2000_params"))
    return "C200_LIDAR";
  if (name.contains("pid"))
    return "PID";
  if (name.contains("bringup"))
    return "BRINGUP";
  if (name.contains("mapping"))
    return "MAPPING";
  if (name.contains("dispatch"))
    return "DISPATCH";
  if (name.contains("laser") || name.contains("lidar"))
    return "LASER_LINK";
  if (name.contains("imu"))
    return "IMU_LINK";
  if (name.contains("location") || name.contains("localization"))
    return "LOCATION";
  if (name.contains("secondary") || name.contains("二次定位"))
    return "SECONDARY_LOC";
  if (name.contains("olei_params"))
    return "olei_params";
  if (name.contains("system_component"))
    return "system_component";
  if (name.contains("motor_control") || path.contains("motor_control"))
    return "MOTOR_CONTROL";
  if (name.contains("config") || path.contains("config.yaml"))
    return "OBSTACLE_CONFIG";
  return "UNKNOWN";
}

QString ConfigMetadataHelper::getProfileName(const QString &key) {
  static const QMap<QString, QString> names = {
      {"MOTION_CONTROL", "运动控制"},
      {"DISPATCH", "任务调度"},
      {"SECONDARY_LOC", "二次定位"},
      {"C200_LIDAR", "激光雷达驱动"},
      {"PID", "PID 控制"},
      {"BRINGUP", "Bringup"},
      {"MAPPING", "建图参数"},
      {"LASER_LINK", "雷达安装位姿"},
      {"IMU_LINK", "IMU 安装位姿"},
      {"LOCATION", "定位参数"},
      {"olei_params", "避障雷达"},
      {"system_component", "避障相关参数"},
      {"MOTOR_CONTROL", "底盘驱动"},
      {"OBSTACLE_CONFIG", "障碍物配置"}};
  return names.value(key, key);
}

QString ConfigMetadataHelper::basename(const QString &path) {
  int idx = path.lastIndexOf('/');
  return idx >= 0 ? path.mid(idx + 1) : path;
}

QString ConfigMetadataHelper::getChildDisplayName(const QJsonObject &entry) {
  QString name = entry.value("name")
                     .toString(basename(entry.value("file_path").toString()))
                     .toLower();
  if (name.contains("base_link_to_laser_back"))
    return "后雷达";
  if (name.contains("base_link_to_laser"))
    return "前雷达";
  return entry.value("name").toString(
      basename(entry.value("file_path").toString()));
}

QJsonObject ConfigMetadataHelper::findMetadataForConfig(const QJsonArray &metadataList, const QJsonObject &configEntry) {
  if (metadataList.isEmpty() || configEntry.isEmpty()) return QJsonObject();

  QString cfgPath = configEntry.value("file_path").toString().trimmed().toLower();
  QString cfgName = configEntry.value("name").toString().trimmed().toLower();
  QString cfgBase = basename(cfgPath).toLower();

  // 1. 完全匹配 file_path
  for (const auto &v : metadataList) {
    QJsonObject meta = v.toObject();
    QString mPath = meta.value("file_path").toString().trimmed().toLower();
    if (!cfgPath.isEmpty() && cfgPath == mPath) return meta;
  }

  // 2. 完全匹配 name
  for (const auto &v : metadataList) {
    QJsonObject meta = v.toObject();
    QString mName = meta.value("name").toString().trimmed().toLower();
    if (!cfgName.isEmpty() && cfgName == mName) return meta;
  }

  // 3. 匹配 basename
  for (const auto &v : metadataList) {
    QJsonObject meta = v.toObject();
    QString mPath = meta.value("file_path").toString().trimmed().toLower();
    QString mBase = basename(mPath).toLower();
    if (!cfgBase.isEmpty() && cfgBase == mBase) return meta;
  }

  // 4. 忽略 .yaml 后缀匹配名称
  auto stripYaml = [](const QString &s) {
    return s.endsWith(".yaml") ? s.left(s.length() - 5) : (s.endsWith(".yml") ? s.left(s.length() - 4) : s);
  };
  QString cleanCfg = stripYaml(cfgName.isEmpty() ? cfgBase : cfgName);
  for (const auto &v : metadataList) {
    QJsonObject meta = v.toObject();
    QString mName = meta.value("name").toString().trimmed().toLower();
    QString mBase = basename(meta.value("file_path").toString().trimmed().toLower());
    QString cleanMeta = stripYaml(mName.isEmpty() ? mBase : mName);
    if (!cleanCfg.isEmpty() && cleanCfg == cleanMeta) return meta;
  }

  return QJsonObject();
}

QString ConfigMetadataHelper::getDefaultValueString(const QJsonObject &metaObj,
                                                   const QString &fullPath) {
  if (metaObj.isEmpty() || fullPath.isEmpty())
    return QStringLiteral("--");

  QJsonObject curObj = metaObj.value("params").toObject();
  const QStringList parts = fullPath.split('.');

  for (int i = 0; i < parts.size() - 1; ++i) {
    QJsonValue nextVal = curObj.value(parts[i]);
    if (!nextVal.isObject()) {
      return QStringLiteral("--");
    }
    curObj = nextVal.toObject();
  }

  QString leafKey = parts.last();
  QJsonValue leafVal = curObj.value(leafKey);
  if (!leafVal.isObject()) {
    return QStringLiteral("--");
  }

  QJsonObject paramMeta = leafVal.toObject();
  if (!paramMeta.contains("default")) {
    return QStringLiteral("--");
  }

  QJsonValue def = paramMeta.value("default");
  if (def.isNull()) {
    return QStringLiteral("null");
  }
  if (def.isString()) {
    return def.toString();
  }
  if (def.isDouble()) {
    double d = def.toDouble();
    return (d == (long long)d) ? QString::number((long long)d)
                               : QString::number(d, 'g', 10);
  }
  if (def.isBool()) {
    return def.toBool() ? QStringLiteral("true") : QStringLiteral("false");
  }
  if (def.isArray()) {
    return QString::fromUtf8(
        QJsonDocument(def.toArray()).toJson(QJsonDocument::Compact));
  }
  return QStringLiteral("--");
}

bool ConfigMetadataHelper::getParamMinMax(const QJsonObject &metaObj,
                                          const QString &fullPath,
                                          double &outMin, double &outMax,
                                          bool &hasMin, bool &hasMax) {
  hasMin = false;
  hasMax = false;
  if (fullPath.isEmpty())
    return false;

  auto parseLimit = [](const QJsonValue &val, double &outVal) -> bool {
    if (val.isNull() || val.isUndefined())
      return false;
    if (val.isDouble()) {
      outVal = val.toDouble();
      return true;
    }
    if (val.isString()) {
      QString s = val.toString().trimmed();
      if (s.isEmpty() || s.compare("null", Qt::CaseInsensitive) == 0 ||
          s == "--" || s == "none")
        return false;
      bool ok = false;
      double d = s.toDouble(&ok);
      if (ok) {
        outVal = d;
        return true;
      }
    }
    return false;
  };

  if (!metaObj.isEmpty()) {
    QJsonObject curObj = metaObj.value("params").toObject();
    const QStringList parts = fullPath.split('.');
    for (int i = 0; i < parts.size() - 1; ++i) {
      curObj = curObj.value(parts[i]).toObject();
    }
    QJsonObject leafObj = curObj.value(parts.last()).toObject();
    if (!leafObj.isEmpty()) {
      hasMin = parseLimit(leafObj.value("min"), outMin);
      hasMax = parseLimit(leafObj.value("max"), outMax);
    }
  }

  // 兜底：若该项为 motion_control 相关参数且 metaObj 中未找到，则尝试从 MotionConfigManager 获取
  if (!hasMin && !hasMax) {
    const QString leafKey = fullPath.contains('.') ? fullPath.split('.').last() : fullPath;
    if (MotionConfigManager::instance().hasParamMeta(leafKey)) {
      auto m = MotionConfigManager::instance().paramMeta(leafKey);
      if (m.hasMin) {
        hasMin = true;
        outMin = m.minVal;
      }
      if (m.hasMax) {
        hasMax = true;
        outMax = m.maxVal;
      }
    }
  }

  return hasMin || hasMax;
}

void ConfigMetadataHelper::ensureMotionControlConfig(QJsonArray &configs) {
  bool found = false;
  auto &mc = MotionConfigManager::instance();
  for (int i = 0; i < configs.size(); ++i) {
    QJsonObject entry = configs[i].toObject();
    QString name = entry.value("name").toString().toLower();
    QString path = entry.value("file_path").toString().toLower();
    if (name == "motion_control" || name.contains("motion_control") ||
        path.contains("motion_control") || name == "运动控制") {
      found = true;
      QJsonObject params = entry.value("params").toObject();
      if (!params.contains("max_speed")) params["max_speed"] = mc.maxSpeed();
      if (!params.contains("max_acceleration")) params["max_acceleration"] = mc.maxAcceleration();
      if (!params.contains("max_deceleration")) params["max_deceleration"] = mc.maxDeceleration();
      if (!params.contains("max_angular_speed")) params["max_angular_speed"] = mc.maxAngularSpeedDeg();
      if (!params.contains("max_angular_acceleration")) params["max_angular_acceleration"] = mc.maxAngularAccelerationDeg();
      if (!params.contains("arrival_distance_tolerance")) params["arrival_distance_tolerance"] = mc.arrivalDistanceTolerance();
      if (!params.contains("arrival_angle_tolerance")) params["arrival_angle_tolerance"] = mc.arrivalAngleToleranceDeg();
      if (!params.contains("route_width")) params["route_width"] = mc.routeWidth();
      entry["params"] = params;
      configs[i] = entry;
      mc.updateFromJsonObject(params);
      break;
    }
  }

  if (!found) {
    QJsonObject motionEntry;
    motionEntry["name"] = "motion_control";
    motionEntry["file_path"] = "/home/ubuntu/xinje_agv/src/robot_bringup/config/motion_control.yaml";
    motionEntry["status"] = true;
    motionEntry["params"] = mc.toJsonObject();
    configs.prepend(motionEntry);
  }
}

void ConfigMetadataHelper::ensureAlignConfig(QJsonArray &configs) {
  auto &ac = AlignConfigManager::instance();
  for (int i = 0; i < configs.size(); ++i) {
    QJsonObject entry = configs[i].toObject();
    QString path = entry.value("file_path").toString().toLower();
    QJsonObject params = entry.value("params").toObject();
    if (params.contains("cylinder_length") || path.contains("secondary_localization_reflector")) {
      if (params.contains("cylinder_length")) {
        ac.setCylinderLength(params.value("cylinder_length").toDouble(ac.cylinderLength()));
      }
      if (params.contains("anti_align_depth")) {
        ac.setAntiAlignDepth(params.value("anti_align_depth").toDouble(ac.antiAlignDepth()));
      } else {
        params["anti_align_depth"] = ac.antiAlignDepth();
        entry["params"] = params;
        configs[i] = entry;
      }
      break;
    }
  }
}

void ConfigMetadataHelper::ensureObstacleConfig(QJsonArray &configs) {
  auto &mc = MotionConfigManager::instance();
  for (int i = 0; i < configs.size(); ++i) {
    QJsonObject entry = configs[i].toObject();
    QString name = entry.value("name").toString().toLower();
    QString path = entry.value("file_path").toString().toLower();
    QJsonObject params = entry.value("params").toObject();
    if (params.contains("id_stop_range_threshold") || params.contains("id_deceleration_range_threshold") ||
        name.contains("obstacle") || path.contains("obstacle") || name.contains("config") || path.contains("config.yaml")) {
      bool modified = false;
      if (!params.contains("id_stop_range_threshold")) {
        QJsonArray stopArr;
        stopArr.append(mc.defaultObsStopLength());
        stopArr.append(mc.defaultObsStopWidth());
        params["id_stop_range_threshold"] = stopArr;
        modified = true;
      }
      if (!params.contains("id_deceleration_range_threshold")) {
        QJsonArray decelArr;
        decelArr.append(mc.defaultObsDecelLength());
        decelArr.append(mc.defaultObsDecelWidth());
        params["id_deceleration_range_threshold"] = decelArr;
        modified = true;
      }
      if (modified) {
        entry["params"] = params;
        configs[i] = entry;
      }
      mc.updateObstacleConfig(params);
      break;
    }
  }
}
