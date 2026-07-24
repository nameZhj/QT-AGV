#include "EditConfigWidget.h"
#include "core/ApiService.h"
#include "core/LoadingDialog.h"
#include "core/Toast.h"
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QProgressDialog>
#include <QSplitter>
#include <QVBoxLayout>

// ── Toolbar icon helpers：图标在上、文字在下 (Qt::ToolButtonTextUnderIcon)，
// 与 MapViewWidget/TaskWidget 工具栏按钮风格保持一致 ─────────────────────────
namespace {

QPixmap drawCfgToolIcon(const QString &name, const QColor &col, int sz = 20) {
  QPixmap pm(sz, sz);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);
  double c = sz * 0.5;

  if (name == "save") {
    p.setPen(QPen(col, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    QPainterPath cp;
    cp.moveTo(sz * 0.14, sz * 0.50);
    cp.lineTo(sz * 0.42, sz * 0.80);
    cp.lineTo(sz * 0.86, sz * 0.20);
    p.drawPath(cp);
  } else if (name == "download") {
    p.setPen(QPen(col, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawLine(QPointF(c, sz * 0.10), QPointF(c, sz * 0.62));
    p.setPen(Qt::NoPen);
    p.setBrush(col);
    QPolygonF arrow;
    arrow << QPointF(sz * 0.32, sz * 0.50) << QPointF(sz * 0.68, sz * 0.50)
          << QPointF(c, sz * 0.78);
    p.drawPolygon(arrow);
    p.setPen(QPen(col, 1.6));
    p.drawLine(QPointF(sz * 0.16, sz * 0.88), QPointF(sz * 0.84, sz * 0.88));
  } else if (name == "upload") {
    p.setPen(QPen(col, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawLine(QPointF(c, sz * 0.34), QPointF(c, sz * 0.88));
    p.setPen(Qt::NoPen);
    p.setBrush(col);
    QPolygonF arrow;
    arrow << QPointF(sz * 0.32, sz * 0.44) << QPointF(sz * 0.68, sz * 0.44)
          << QPointF(c, sz * 0.16);
    p.drawPolygon(arrow);
    p.setPen(QPen(col, 1.6));
    p.drawLine(QPointF(sz * 0.16, sz * 0.10), QPointF(sz * 0.84, sz * 0.10));
  }

  p.end();
  return pm;
}

QIcon makeCfgToolIcon(const QString &name) {
  QIcon icon;
  icon.addPixmap(drawCfgToolIcon(name, QColor("#9ab0c8")), QIcon::Normal,
                 QIcon::Off);
  icon.addPixmap(drawCfgToolIcon(name, QColor("white")), QIcon::Active,
                 QIcon::Off);
  return icon;
}

const char *kToolBtnStyle =
    "QToolButton{"
    "  background:#252a35; color:#c8d0d8; border:1px solid #3a3f4a;"
    "  border-radius:4px; padding:4px 12px; font-size:11px; font-weight:600;"
    "}"
    "QToolButton:hover{background:#2d3340; border-color:#4a9eff;}"
    "QToolButton:pressed{background:#1e4080;}"
    "QToolButton:disabled{color:#445566; border-color:#2d3340;}";

const char *kToolBtnPrimary =
    "QToolButton{"
    "  background:#1e6fd4; color:#fff; border:none;"
    "  border-radius:4px; padding:4px 12px; font-size:11px; font-weight:600;"
    "}"
    "QToolButton:hover{background:#2980e8;}"
    "QToolButton:pressed{background:#155db8;}";

} // namespace

EditConfigWidget::EditConfigWidget(QWidget *parent) : QWidget(parent) {
  setupUI();
  loadAll();
}

void EditConfigWidget::setupUI() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);

  // 顶部工具栏：图标在上、文字在下，与地图/任务页面的工具栏按钮风格一致
  QHBoxLayout *toolbar = new QHBoxLayout;

  auto addToolBtn = [&](const QString &iconName, const QString &text) {
    auto *b = new QToolButton;
    b->setText(text);
    b->setIcon(makeCfgToolIcon(iconName));
    b->setIconSize(QSize(20, 20));
    b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    b->setAutoRaise(true);
    b->setCursor(Qt::PointingHandCursor);
    b->setStyleSheet(kToolBtnStyle);
    return b;
  };

  m_saveBtn = addToolBtn("save", "保存");
  m_saveBtn->setStyleSheet(kToolBtnPrimary);
  m_downloadBtn = addToolBtn("download", "下载参数文件");
  m_uploadBtn = addToolBtn("upload", "上传参数文件");

  m_filterEdit = new QLineEdit;
  m_filterEdit->setPlaceholderText("搜索参数...");

  connect(m_saveBtn, &QToolButton::clicked, this, &EditConfigWidget::onSave);
  connect(m_downloadBtn, &QToolButton::clicked, this,
          &EditConfigWidget::onDownload);
  connect(m_uploadBtn, &QToolButton::clicked, this,
          &EditConfigWidget::onUpload);
  connect(m_filterEdit, &QLineEdit::textChanged, this,
          &EditConfigWidget::onFilterChanged);

  toolbar->addWidget(m_saveBtn);
  toolbar->addWidget(m_downloadBtn);
  toolbar->addWidget(m_uploadBtn);
  toolbar->addStretch();
  toolbar->addWidget(new QLabel("搜索:"));
  toolbar->addWidget(m_filterEdit);
  mainLayout->addLayout(toolbar);

  // 分割器: 左侧分组树 + 右侧参数表
  QSplitter *splitter = new QSplitter(Qt::Horizontal);

  m_groupTree = new QTreeWidget;
  m_groupTree->setHeaderLabel("配置分组");
  m_groupTree->setMaximumWidth(250);
  connect(m_groupTree, &QTreeWidget::itemClicked, this,
          &EditConfigWidget::onGroupItemClicked);
  connect(m_groupTree, &QTreeWidget::currentItemChanged, this,
          [this](QTreeWidgetItem *current, QTreeWidgetItem *previous) {
            Q_UNUSED(previous)
            if (current)
              onGroupItemClicked(current, 0);
          });
  splitter->addWidget(m_groupTree);

  m_paramTable = new QTableWidget;
  m_paramTable->setColumnCount(3);
  m_paramTable->setHorizontalHeaderLabels({"参数名", "值", "说明"});
  m_paramTable->horizontalHeader()->setStretchLastSection(true);
  m_paramTable->setColumnWidth(0, 220);
  m_paramTable->setColumnWidth(1, 300);
  // 默认行高偏矮，叠加 stylesheet 的 item padding 后，Qt 不会重新计算行高，
  // 导致编辑值时内嵌的输入框被裁切、显示不全。这里显式加大行高留出余量。
  m_paramTable->verticalHeader()->setDefaultSectionSize(40);
  m_paramTable->verticalHeader()->setMinimumSectionSize(40);
  m_paramTable->setStyleSheet(
      "QTableWidget::item { padding: 8px 10px; }"
      "QLineEdit {"
      "  background: #1a2330; border: 1px solid #4a9eff; border-radius: 4px;"
      "  color: #e0e8f0; padding: 4px 8px; font-size: 13px;"
      "  selection-background-color: #1e4080;"
      "}");
  connect(m_paramTable, &QTableWidget::cellChanged, this,
          &EditConfigWidget::onParamCellChanged);
  splitter->addWidget(m_paramTable);

  splitter->setSizes({250, 800});
  mainLayout->addWidget(splitter);
}

void EditConfigWidget::refreshData() { loadAll(); }

void EditConfigWidget::loadAll() {
  ApiService::instance().getAllConfigsPreserve(
      [this](int code, const QByteArray &rawData) {
        if (code != 200) {
          QMessageBox::warning(this, "错误", "加载配置失败");
          return;
        }

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(rawData, &err);
        if (err.error != QJsonParseError::NoError) {
          QMessageBox::warning(this, "错误", "解析配置失败");
          return;
        }

        QJsonObject root = doc.object();
        QJsonArray list;
        if (root.contains("data")) {
          if (root.value("data").isArray())
            list = root.value("data").toArray();
          else if (root.value("data").isObject()) {
            QJsonObject dataObj = root.value("data").toObject();
            if (dataObj.contains("data") && dataObj.value("data").isArray())
              list = dataObj.value("data").toArray();
            else {
              for (const auto &k : dataObj.keys()) {
                if (dataObj.value(k).isArray()) {
                  list = dataObj.value(k).toArray();
                  break;
                }
              }
            }
          }
        }

        m_configs = list;
        refreshGroups();
      });
}

void EditConfigWidget::refreshGroups() {
  m_groupTree->clear();

  // 按 profileKey 对配置文件索引列表进行分组，保持原始出现的顺序
  QMap<QString, QList<int>> groupIndices;
  QStringList profileOrder;

  for (int i = 0; i < m_configs.size(); ++i) {
    QJsonObject entry = m_configs[i].toObject();
    QString profileKey = determineProfile(entry);
    if (!groupIndices.contains(profileKey)) {
      profileOrder.append(profileKey);
    }
    groupIndices[profileKey].append(i);
  }

  for (const QString &profileKey : profileOrder) {
    QString profileName = getProfileName(profileKey);
    const QList<int> &indices = groupIndices[profileKey];

    QTreeWidgetItem *topItem = new QTreeWidgetItem;
    topItem->setText(0, profileName);
    topItem->setData(0, Qt::UserRole, profileKey);

    if (indices.size() == 1) {
      // 仅包含1个配置文件（如
      // IMU安装位姿、定位参数、建图参数）：删除无用的子节点，直接作为一级菜单项显示
      int configIdx = indices.first();
      topItem->setData(0, Qt::UserRole + 1, configIdx);
      m_groupTree->addTopLevelItem(topItem);
    } else {
      // 包含多个配置文件（如 雷达安装位姿包含 前雷达 /
      // 后雷达）：保留一级节点，并创建中文二级子节点
      m_groupTree->addTopLevelItem(topItem);
      for (int idx : indices) {
        QJsonObject entry = m_configs[idx].toObject();
        QTreeWidgetItem *child = new QTreeWidgetItem;
        child->setText(0, getChildDisplayName(entry));
        child->setData(0, Qt::UserRole, profileKey);
        child->setData(0, Qt::UserRole + 1, idx);
        topItem->addChild(child);
      }
    }
  }

  m_groupTree->expandAll();

  // 默认自动选中第一项
  if (m_groupTree->topLevelItemCount() > 0) {
    QTreeWidgetItem *firstItem = m_groupTree->topLevelItem(0);
    m_groupTree->setCurrentItem(firstItem);
    onGroupItemClicked(firstItem, 0);
  }
}

// Returns {cn_name, remark} from the Vue fieldMeta.js translations
QPair<QString, QString> EditConfigWidget::fieldMetaFor(const QString &key) {
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
      {"cylinder_length", {"前后货架腿中心点距离", "单位 m"}},
      {"cylinder_recognition_range", {"货架腿识别范围", ""}},
      {"lift_topic_name", {"顶升话题名称", ""}},
      {"lift_max", {"顶升最大高度", "单位 m"}},
      {"lift_min", {"顶升最小高度", "单位 m"}},
      {"cylinder_width", {"左右货架腿中心点距离", "单位 m"}},
      {"dis_goal_error", {"接近点后直线调整距离", "单位 m"}},
      {"length_undock", {"二次出库调整距离", "单位 m"}},
      {"rambler_bezier_step", {"贝塞尔步长", ""}},
      {"straight_line_cruise_governor_polling_interval",
       {"直线巡航调速器轮询间隔", "单位秒"}},
      {"ignore_area_a", {" 屏蔽区域直径", "单位 m"}},
      {"ignore_area_x", {"屏蔽区域距离车体中心的纵向距离", "单位 m"}},
      {"ignore_area_y", {"屏蔽区域距离车体中心的横向距离", "单位 m"}},

  };
  return meta.value(key, {"", ""});
}

void EditConfigWidget::addParamRows(const QJsonObject &params, int configIdx,
                                    const QString &prefix,
                                    const QString &filter) {
  for (auto it = params.begin(); it != params.end(); ++it) {
    const QString &key = it.key();
    const QJsonValue &jv = it.value();
    const QString fullPath = prefix.isEmpty() ? key : prefix + "." + key;

    // Recurse into nested objects (expand them as dotted-path rows)
    if (jv.isObject() && !jv.toObject().isEmpty()) {
      addParamRows(jv.toObject(), configIdx, fullPath, filter);
      continue;
    }

    // Leaf value → format as string
    QString value;
    if (jv.isString())
      value = jv.toString();
    else if (jv.isDouble()) {
      double d = jv.toDouble();
      value = (d == (long long)d) ? QString::number((long long)d)
                                  : QString::number(d, 'g', 10);
    } else if (jv.isBool())
      value = jv.toBool() ? "true" : "false";
    else if (jv.isArray())
      value = QString::fromUtf8(
          QJsonDocument(jv.toArray()).toJson(QJsonDocument::Compact));
    else
      value = "null";

    // Filter
    auto fm = fieldMetaFor(key);
    if (!filter.isEmpty()) {
      bool hit = fullPath.toLower().contains(filter) ||
                 value.toLower().contains(filter) ||
                 fm.first.contains(filter) || fm.second.contains(filter);
      if (!hit)
        continue;
    }

    int row = m_paramTable->rowCount();
    m_paramTable->insertRow(row);

    // Column 0: dotted key path (not editable)
    auto *keyItem = new QTableWidgetItem(fullPath);
    keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
    keyItem->setData(Qt::UserRole, configIdx);
    keyItem->setData(Qt::UserRole + 1, fullPath);
    m_paramTable->setItem(row, 0, keyItem);

    // Column 1: value (editable)
    m_paramTable->setItem(row, 1, new QTableWidgetItem(value));

    // Column 2: Chinese name + remark (not editable)
    QString desc = fm.first;
    if (!fm.second.isEmpty())
      desc += "  (" + fm.second + ")";
    auto *descItem = new QTableWidgetItem(desc);
    descItem->setFlags(descItem->flags() & ~Qt::ItemIsEditable);
    descItem->setForeground(QColor(0x88, 0x99, 0xaa));
    m_paramTable->setItem(row, 2, descItem);
  }
}

void EditConfigWidget::refreshParams(const QString &groupKey) {
  m_paramTable->blockSignals(true);
  m_paramTable->setRowCount(0);

  QString filter = m_filterEdit->text().trimmed().toLower();

  for (int i = 0; i < m_configs.size(); ++i) {
    QJsonObject entry = m_configs[i].toObject();
    if (determineProfile(entry) != groupKey)
      continue;
    addParamRows(entry.value("params").toObject(), i, "", filter);
  }

  m_paramTable->blockSignals(false);
}

void EditConfigWidget::refreshParamsForConfig(int configIdx) {
  m_paramTable->blockSignals(true);
  m_paramTable->setRowCount(0);

  QString filter = m_filterEdit->text().trimmed().toLower();

  if (configIdx >= 0 && configIdx < m_configs.size()) {
    QJsonObject entry = m_configs[configIdx].toObject();
    addParamRows(entry.value("params").toObject(), configIdx, "", filter);
  }

  m_paramTable->blockSignals(false);
}

QString EditConfigWidget::determineProfile(const QJsonObject &entry) const {
  QString name = entry.value("name").toString().toLower();
  QString path = entry.value("file_path").toString().toLower();

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

QString EditConfigWidget::getProfileName(const QString &key) const {
  static QMap<QString, QString> names = {{"DISPATCH", "任务调度"},
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

QString EditConfigWidget::getChildDisplayName(const QJsonObject &entry) const {
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

QString EditConfigWidget::basename(const QString &path) const {
  int idx = path.lastIndexOf('/');
  return idx >= 0 ? path.mid(idx + 1) : path;
}

void EditConfigWidget::onSave() {
  QJsonObject payload;
  for (const auto &item : m_configs) {
    QJsonObject entry = item.toObject();
    QString key =
        entry.value("file_path").toString(entry.value("name").toString());
    QJsonObject val;
    val["name"] = entry.value("name");
    val["status"] = entry.value("status");
    val["params"] = entry.value("params");
    val["file_path"] = entry.value("file_path");
    payload[key] = val;
  }

  m_saveBtn->setEnabled(false);
  auto *loading = new LoadingDialog("正在保存配置参数...", this);
  loading->show();
  ApiService::instance().modifyParam(
      payload, [this, loading](int code, const QJsonObject &) {
        loading->close();
        loading->deleteLater();
        m_saveBtn->setEnabled(true);
        if (code == 200) {
          Toast::success(this, "保存成功");
        } else {
          QMessageBox::warning(this, "错误", "保存失败");
        }
      });
}

void EditConfigWidget::onDownload() {
  m_downloadBtn->setEnabled(false);
  auto *loading = new LoadingDialog("正在下载配置文件...", this);
  loading->show();
  ApiService::instance().getConfigSummary(
      [this, loading](bool ok, const QByteArray &data) {
        loading->close();
        loading->deleteLater();
        m_downloadBtn->setEnabled(true);
        if (!ok) {
          QMessageBox::warning(this, "错误", "下载失败");
          return;
        }
        QString path = QFileDialog::getSaveFileName(
            this, "保存配置文件", "config_summary.json", "JSON Files (*.json)");
        if (path.isEmpty())
          return;
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
          file.write(data);
          file.close();
          Toast::success(this, "下载成功");
        }
      });
}

void EditConfigWidget::onUpload() {
  QString path = QFileDialog::getOpenFileName(this, "选择配置文件", "",
                                              "JSON Files (*.json)");
  if (path.isEmpty())
    return;

  m_uploadBtn->setEnabled(false);
  auto *loading = new LoadingDialog("正在上传配置文件...", this);
  loading->show();
  ApiService::instance().updateConfigSummary(
      path, [this, loading](int code, const QJsonObject &) {
        loading->close();
        loading->deleteLater();
        m_uploadBtn->setEnabled(true);
        if (code == 200) {
          Toast::success(this, "上传成功");
          loadAll();
        } else {
          QMessageBox::warning(this, "错误", "上传失败");
        }
      });
}

void EditConfigWidget::onFilterChanged(const QString &text) {
  Q_UNUSED(text)
  if (m_currentConfigIdx >= 0)
    refreshParamsForConfig(m_currentConfigIdx);
  else if (!m_currentGroupKey.isEmpty())
    refreshParams(m_currentGroupKey);
}

void EditConfigWidget::onGroupItemClicked(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column)
  if (!item)
    return;
  QString key = item->data(0, Qt::UserRole).toString();
  if (key.isEmpty())
    return;

  bool ok = false;
  int configIdx = item->data(0, Qt::UserRole + 1).toInt(&ok);
  if (!ok)
    configIdx = -1;

  if (configIdx >= 0 && configIdx < m_configs.size()) {
    // 直接关联具体配置文件的节点（包括提为一级菜单的单文件节点和二级节点）
    m_currentGroupKey = key;
    m_currentConfigIdx = configIdx;
    refreshParamsForConfig(configIdx);
  } else {
    // 包含多个配置文件的组节点（显示该分组下所有配置文件的参数）
    m_currentGroupKey = key;
    m_currentConfigIdx = -1;
    refreshParams(key);
  }
}

// Navigate a QJsonValue along a dotted path, returning the leaf value
static QJsonValue jsonGet(const QJsonValue &root, const QStringList &path) {
  if (path.isEmpty())
    return root;
  if (!root.isObject())
    return {};
  return jsonGet(root.toObject().value(path.first()), path.mid(1));
}

// Return a copy of root with the value at path replaced by newVal
static QJsonValue jsonSet(const QJsonValue &root, const QStringList &path,
                          const QJsonValue &newVal) {
  if (path.isEmpty())
    return newVal;
  QJsonObject obj = root.isObject() ? root.toObject() : QJsonObject{};
  obj[path.first()] = jsonSet(obj.value(path.first()), path.mid(1), newVal);
  return obj;
}

void EditConfigWidget::onParamCellChanged(int row, int col) {
  if (col != 1)
    return;
  auto *keyItem = m_paramTable->item(row, 0);
  auto *valItem = m_paramTable->item(row, 1);
  if (!keyItem || !valItem)
    return;

  QVariant idxVar = keyItem->data(Qt::UserRole);
  if (!idxVar.isValid())
    return;
  int configIdx = idxVar.toInt();
  if (configIdx < 0 || configIdx >= m_configs.size())
    return;

  // Full dotted path stored in UserRole+1 (e.g. "run_pid.sp_P")
  QString fullPath = keyItem->data(Qt::UserRole + 1).toString();
  if (fullPath.isEmpty())
    fullPath = keyItem->text(); // fallback
  QStringList segments = fullPath.split(".");

  const QString newStr = valItem->text().trimmed();

  QJsonObject entry = m_configs[configIdx].toObject();
  QJsonValue params = entry.value("params");

  // Determine original leaf type for type-preserving write-back
  QJsonValue origLeaf = jsonGet(params, segments);
  QJsonValue newLeaf;

  if (origLeaf.isDouble()) {
    bool ok;
    double d = newStr.toDouble(&ok);
    newLeaf = ok ? QJsonValue(d) : QJsonValue(newStr);
  } else if (origLeaf.isBool()) {
    QString lo = newStr.toLower();
    if (lo == "true" || lo == "1")
      newLeaf = QJsonValue(true);
    else if (lo == "false" || lo == "0")
      newLeaf = QJsonValue(false);
    else
      newLeaf = QJsonValue(newStr);
  } else if (origLeaf.isArray()) {
    QJsonParseError pe;
    QJsonDocument doc = QJsonDocument::fromJson(newStr.toUtf8(), &pe);
    newLeaf = (pe.error == QJsonParseError::NoError && doc.isArray())
                  ? QJsonValue(doc.array())
                  : QJsonValue(newStr);
  } else {
    newLeaf = QJsonValue(newStr);
  }

  entry["params"] = jsonSet(params, segments, newLeaf).toObject();
  m_configs[configIdx] = entry;
}
