#include "EditConfigWidget.h"
#include "network/ApiService.h"
#include "ui/common/LoadingDialog.h"
#include "ui/common/Toast.h"
#include <QFile>
#include <QFileDialog>
#include <QDir>
#include <QCoreApplication>
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

#include <QStyledItemDelegate>
#include <QTimer>
#include <QSettings>
#include "core/MotionConfigManager.h"
#include "core/AlignConfigManager.h"

// ── Toolbar icon helpers：图标在上、文字在下 (Qt::ToolButtonTextUnderIcon)，
// 与 MapViewWidget/TaskWidget 工具栏按钮风格保持一致 ─────────────────────────
namespace {

class ParamItemDelegate : public QStyledItemDelegate
{
public:
    explicit ParamItemDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QWidget *editor = QStyledItemDelegate::createEditor(parent, option, index);
        if (QLineEdit *lineEdit = qobject_cast<QLineEdit*>(editor)) {
            lineEdit->setStyleSheet("QLineEdit { background: #1a2330; border: 1.5px solid #4a9eff; border-radius: 4px; color: #ffffff; padding: 4px 8px; font-size: 13px; selection-background-color: #1e4080; }");
            // 进入编辑模式时自动全选原有文本，方便直接键盘录入新值
            QTimer::singleShot(0, lineEdit, &QLineEdit::selectAll);
        }
        return editor;
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        QStyledItemDelegate::setEditorData(editor, index);
        if (QLineEdit *lineEdit = qobject_cast<QLineEdit*>(editor)) {
            lineEdit->selectAll();
        }
    }

    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override
    {
        if (QLineEdit *lineEdit = qobject_cast<QLineEdit*>(editor)) {
            QString text = lineEdit->text().trimmed();
            model->setData(index, text, Qt::EditRole);
        } else {
            QStyledItemDelegate::setModelData(editor, model, index);
        }
    }
};

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
  m_paramTable->setColumnCount(4);
  m_paramTable->setHorizontalHeaderLabels({"参数名", "值", "默认值", "说明"});
  m_paramTable->horizontalHeader()->setStretchLastSection(true);
  m_paramTable->setColumnWidth(0, 220);
  m_paramTable->setColumnWidth(1, 260);
  m_paramTable->setColumnWidth(2, 200);
  m_paramTable->setItemDelegateForColumn(1, new ParamItemDelegate(m_paramTable));
  m_paramTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);
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

static void ensureMotionControlConfig(QJsonArray &configs) {
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
      // 同步更新全局运动配置
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
    configs.prepend(motionEntry); // 优先放置在最前面，作为首要控制分组
  }
}

static void ensureAlignConfig(QJsonArray &configs) {
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

static void ensureObstacleConfig(QJsonArray &configs) {
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

void EditConfigWidget::refreshData() { loadAll(); }

void EditConfigWidget::loadAll() {
  // 1. 尝试从本地缓存恢复元数据
  QSettings settings;
  QByteArray cachedMeta = settings.value("editConfig/cachedMetadata").toByteArray();
  if (!cachedMeta.isEmpty() && m_metadataList.isEmpty()) {
    parseMetadata(cachedMeta);
  }

  // 2. 异步请求 /rbrainrobot/get_config_metadata (端口 8062) 获取默认值元数据
  ApiService::instance().getConfigMetadata(
      [this](int mCode, const QByteArray &mRaw) {
        if (mCode == 200) {
          parseMetadata(mRaw);
          updateDefaultValuesInTable();
        }
      });

  // 3. 异步请求 /rbrainrobot/get_all_configs (端口 8062) 获取当前配置数据
  ApiService::instance().getAllConfigsPreserve(
      [this](int code, const QByteArray &rawData) {
        QJsonArray list;
        bool loadedFromNetwork = false;

        if (code == 200) {
          QJsonParseError err;
          QJsonDocument doc = QJsonDocument::fromJson(rawData, &err);
          if (err.error == QJsonParseError::NoError) {
            QJsonObject root = doc.object();
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
            loadedFromNetwork = true;
          }
        }

        // 若网络未获取到数据，尝试从本地缓存恢复
        if (!loadedFromNetwork || list.isEmpty()) {
          QSettings settings;
          QByteArray cached = settings.value("editConfig/cachedConfigs").toByteArray();
          if (!cached.isEmpty()) {
            QJsonDocument cDoc = QJsonDocument::fromJson(cached);
            if (cDoc.isArray()) {
              list = cDoc.array();
            }
          }
        }

        // 确保“运动控制”分组及对应的7项参数完备存在
        ensureMotionControlConfig(list);
        ensureAlignConfig(list);
        ensureObstacleConfig(list);

        // 将从 /rbrainrobot/get_all_configs 接口获取的实际参数动态更新到全局 MotionConfigManager
        MotionConfigManager::instance().updateFromConfigsArray(list);

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

void EditConfigWidget::addParamRows(const QJsonObject &params, int configIdx,
                                    const QString &prefix,
                                    const QString &filter) {
  QStringList keys = params.keys();
  static const QStringList kPreferredOrder = {
      "max_speed",
      "max_acceleration",
      "max_deceleration",
      "max_angular_speed",
      "max_angular_acceleration",
      "arrival_distance_tolerance",
      "arrival_angle_tolerance",
      "route_width"
  };
  std::sort(keys.begin(), keys.end(), [](const QString &a, const QString &b) {
    int idxA = kPreferredOrder.indexOf(a);
    int idxB = kPreferredOrder.indexOf(b);
    if (idxA != -1 && idxB != -1) return idxA < idxB;
    if (idxA != -1) return true;
    if (idxB != -1) return false;
    return a < b;
  });

  for (const QString &key : keys) {
    const QJsonValue &jv = params.value(key);
    const QString fullPath = prefix.isEmpty() ? key : prefix + "." + key;

    // 隐藏 max_deceleration 相关的参数配置 UI
    if (key == "max_deceleration" || fullPath.endsWith(".max_deceleration")) {
      continue;
    }

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

    // Look up default value from metadata
    QJsonObject metaObj;
    if (configIdx >= 0 && configIdx < m_configs.size()) {
      metaObj = findMetadataForConfig(m_configs[configIdx].toObject());
    }
    QString defaultStr = getDefaultValueString(metaObj, fullPath);

    // Filter
    auto fm = fieldMetaFor(key);
    if (!filter.isEmpty()) {
      bool hit = fullPath.toLower().contains(filter) ||
                 value.toLower().contains(filter) ||
                 defaultStr.toLower().contains(filter) ||
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

    // Column 2: default value (not editable)
    auto *defaultItem = new QTableWidgetItem(defaultStr);
    defaultItem->setFlags(defaultItem->flags() & ~Qt::ItemIsEditable);
    defaultItem->setForeground(QColor(0x94, 0xa3, 0xb8));
    defaultItem->setData(Qt::UserRole, configIdx);
    defaultItem->setData(Qt::UserRole + 1, fullPath);
    m_paramTable->setItem(row, 2, defaultItem);

    // Column 3: Chinese name + remark + range (not editable)
    QString desc = fm.first;
    if (!fm.second.isEmpty())
      desc += "  (" + fm.second + ")";

    // 检查 metadata 是否包含 min/max 参数范围 (若为 null 或空则不显示范围)
    double outMin = 0.0, outMax = 0.0;
    bool hasMin = false, hasMax = false;
    if (getParamMinMax(metaObj, fullPath, outMin, outMax, hasMin, hasMax)) {
      if (hasMin && hasMax) {
        desc += QString(" [范围: %1 ~ %2]").arg(outMin).arg(outMax);
      } else if (hasMin) {
        desc += QString(" [最小: %1]").arg(outMin);
      } else if (hasMax) {
        desc += QString(" [最大: %1]").arg(outMax);
      }
    }

    auto *descItem = new QTableWidgetItem(desc);
    descItem->setFlags(descItem->flags() & ~Qt::ItemIsEditable);
    descItem->setForeground(QColor(0x88, 0x99, 0xaa));
    m_paramTable->setItem(row, 3, descItem);
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

QString EditConfigWidget::getProfileName(const QString &key) const {
  static QMap<QString, QString> names = {{"MOTION_CONTROL", "运动控制"},
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

void EditConfigWidget::syncObstacleParamsToRouteNetwork(double stopDx, double stopDy,
                                                       double decelDx, double decelDy,
                                                       std::function<void(bool ok)> onDone) {
  ApiService::instance().loadEditedMap([this, stopDx, stopDy, decelDx, decelDy, onDone](int code, const QJsonObject &resp) {
    if (code != 200) {
      qWarning() << "[EditConfigWidget] Failed to load route network for obstacle sync, code:" << code;
      if (onDone) onDone(false);
      return;
    }

    QString jsonStr = resp.value("json").toString();
    if (jsonStr.isEmpty()) {
      if (onDone) onDone(false);
      return;
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
      if (onDone) onDone(false);
      return;
    }

    QJsonObject geojson = doc.object();
    QString mapId = geojson.value("mapId").toString();
    QString mapName = geojson.value("mapName").toString();

    auto isSpecialPoint = [](const QJsonObject &props) -> bool {
      QString id = props.value("id").toString();
      QString type = props.value("type").toString();
      QString typeName = props.value("typeName").toString();
      QString name = props.value("name").toString();

      if (id.startsWith("c_", Qt::CaseInsensitive) || id.startsWith("hp_", Qt::CaseInsensitive) ||
          id.startsWith("cp_", Qt::CaseInsensitive) || id.startsWith("ap_", Qt::CaseInsensitive)) {
        return true;
      }
      if (type.compare("charge", Qt::CaseInsensitive) == 0 ||
          type.compare("highPrecision", Qt::CaseInsensitive) == 0 ||
          type.compare("chargeEpc", Qt::CaseInsensitive) == 0 ||
          type.compare("highPrecisionEpc", Qt::CaseInsensitive) == 0) {
        return true;
      }
      if (typeName.compare("ChargePoint", Qt::CaseInsensitive) == 0 ||
          typeName.compare("ActionPoint", Qt::CaseInsensitive) == 0) {
        return true;
      }
      if (name.startsWith("CP", Qt::CaseInsensitive) || name.startsWith("AP", Qt::CaseInsensitive) ||
          name.contains("充电") || name.contains("工作站")) {
        return true;
      }
      return false;
    };

    if (geojson.contains("features") && geojson.value("features").isArray()) {
      QJsonArray features = geojson.value("features").toArray();
      for (int i = 0; i < features.size(); ++i) {
        QJsonObject feat = features[i].toObject();
        QJsonObject props = feat.value("properties").toObject();
        QJsonObject geom = feat.value("geometry").toObject();
        QString gtype = geom.value("type").toString();

        if (gtype == "Point") {
          if (isSpecialPoint(props)) {
            // 高精度点（工作站）或充电桩点（特殊工作站）的四个避障参数永远为零
            props["intrusion_stop_length"]         = 0.0;
            props["intrusion_stop_width"]          = 0.0;
            props["intrusion_deceleration_length"] = 0.0;
            props["intrusion_deceleration_width"]  = 0.0;
            props["is_custom_obstacle"]            = false;
            if (props.contains("actions") && props.value("actions").isArray()) {
              QJsonArray acts = props.value("actions").toArray();
              for (int a = 0; a < acts.size(); ++a) {
                QJsonObject act = acts[a].toObject();
                if (act.value("action_type").toInt() == 21 || act.value("name").toString() == "实时避障范围") {
                  act["name"] = "实时避障范围";
                  act["action_type"] = 21;
                  act["intrusion_stop_length"]         = 0.0;
                  act["intrusion_stop_width"]          = 0.0;
                  act["intrusion_deceleration_length"] = 0.0;
                  act["intrusion_deceleration_width"]  = 0.0;
                  act["is_custom_obstacle"]            = false;
                  acts[a] = act;
                }
              }
              props["actions"] = acts;
            }
          } else {
            // 普通站点：如果用户修改过避障参数，则不使用全局避障参数
            bool isCustom = props.value("is_custom_obstacle").toBool(props.value("custom_obstacle").toBool(false));
            if (!isCustom) {
              props["intrusion_stop_length"]         = stopDx;
              props["intrusion_stop_width"]          = stopDy;
              props["intrusion_deceleration_length"] = decelDx;
              props["intrusion_deceleration_width"]  = decelDy;
              props["is_custom_obstacle"]            = false;

              QJsonArray acts = props.value("actions").toArray();
              bool foundAct = false;
              for (int a = 0; a < acts.size(); ++a) {
                QJsonObject act = acts[a].toObject();
                if (act.value("action_type").toInt() == 21 || act.value("name").toString() == "实时避障范围") {
                  act["name"] = "实时避障范围";
                  act["action_type"] = 21;
                  act["intrusion_stop_length"]         = stopDx;
                  act["intrusion_stop_width"]          = stopDy;
                  act["intrusion_deceleration_length"] = decelDx;
                  act["intrusion_deceleration_width"]  = decelDy;
                  act["is_custom_obstacle"]            = false;
                  acts[a] = act;
                  foundAct = true;
                  break;
                }
              }
              if (!foundAct && (stopDx > 0.0 || stopDy > 0.0 || decelDx > 0.0 || decelDy > 0.0)) {
                QJsonObject act;
                act["name"] = "实时避障范围";
                act["action_type"] = 21;
                act["intrusion_stop_length"]         = stopDx;
                act["intrusion_stop_width"]          = stopDy;
                act["intrusion_deceleration_length"] = decelDx;
                act["intrusion_deceleration_width"]  = decelDy;
                act["is_custom_obstacle"]            = false;
                acts.append(act);
              }
              props["actions"] = acts;
            }
          }
        } else if (gtype == "LineString" || gtype == "MultiPoint") {
          // 线和曲线：如果用户修改过，则以用户修改为准；未修改则同步全局默认值
          bool isCustom = props.value("is_custom_obstacle").toBool(props.value("custom_obstacle").toBool(false));
          if (!isCustom) {
            props["intrusion_stop_length"]         = stopDx;
            props["intrusion_stop_width"]          = stopDy;
            props["intrusion_deceleration_length"] = decelDx;
            props["intrusion_deceleration_width"]  = decelDy;
            props["is_custom_obstacle"]            = false;

            const auto &mc = MotionConfigManager::instance();
            props["maxspeed"]                 = mc.maxSpeed();
            props["max_acceleration"]         = mc.maxAcceleration();
            props["maxacc"]                   = mc.maxAcceleration();
            props["max_deceleration"]         = mc.maxDeceleration();
            props["maxdec"]                   = mc.maxDeceleration();
            props["max_angular_speed"]        = mc.maxAngularSpeedDeg();
            props["maxrot"]                   = mc.maxAngularSpeedDeg();
            props["max_angular_acceleration"] = mc.maxAngularAccelerationDeg();
            props["maxrotacc"]                = mc.maxAngularAccelerationDeg();
            props["reachdist"]                = mc.arrivalDistanceTolerance();
            props["reachangle"]               = mc.arrivalAngleToleranceDeg();
            props["width"]                    = mc.routeWidth();
          }
        }

        feat["properties"] = props;
        features[i] = feat;
      }
      geojson["features"] = features;
    }

    if (!geojson.contains("features") && (geojson.contains("points") || geojson.contains("lines") || geojson.contains("curves"))) {
      auto updatePoints = [&](QJsonArray &arr, bool isSpecialArr) {
        for (int i = 0; i < arr.size(); ++i) {
          QJsonObject pt = arr[i].toObject();
          if (isSpecialArr || isSpecialPoint(pt)) {
            pt["intrusion_stop_length"]         = 0.0;
            pt["intrusion_stop_width"]          = 0.0;
            pt["intrusion_deceleration_length"] = 0.0;
            pt["intrusion_deceleration_width"]  = 0.0;
            pt["is_custom_obstacle"]            = false;
          } else {
            bool isCustom = pt.value("is_custom_obstacle").toBool(pt.value("custom_obstacle").toBool(false));
            if (!isCustom) {
              pt["intrusion_stop_length"]         = stopDx;
              pt["intrusion_stop_width"]          = stopDy;
              pt["intrusion_deceleration_length"] = decelDx;
              pt["intrusion_deceleration_width"]  = decelDy;
              pt["is_custom_obstacle"]            = false;
            }
          }
          arr[i] = pt;
        }
      };
      auto updateLines = [&](QJsonArray &arr) {
        for (int i = 0; i < arr.size(); ++i) {
          QJsonObject ln = arr[i].toObject();
          bool isCustom = ln.value("is_custom_obstacle").toBool(ln.value("custom_obstacle").toBool(false));
          if (!isCustom) {
            ln["intrusion_stop_length"]         = stopDx;
            ln["intrusion_stop_width"]          = stopDy;
            ln["intrusion_deceleration_length"] = decelDx;
            ln["intrusion_deceleration_width"]  = decelDy;
            ln["is_custom_obstacle"]            = false;

            const auto &mc = MotionConfigManager::instance();
            ln["maxspeed"]                 = mc.maxSpeed();
            ln["max_acceleration"]         = mc.maxAcceleration();
            ln["maxacc"]                   = mc.maxAcceleration();
            ln["max_deceleration"]         = mc.maxDeceleration();
            ln["maxdec"]                   = mc.maxDeceleration();
            ln["max_angular_speed"]        = mc.maxAngularSpeedDeg();
            ln["maxrot"]                   = mc.maxAngularSpeedDeg();
            ln["max_angular_acceleration"] = mc.maxAngularAccelerationDeg();
            ln["maxrotacc"]                = mc.maxAngularAccelerationDeg();
            ln["reachdist"]                = mc.arrivalDistanceTolerance();
            ln["reachangle"]               = mc.arrivalAngleToleranceDeg();
            ln["width"]                    = mc.routeWidth();
          }
          arr[i] = ln;
        }
      };
      if (geojson.contains("points")) {
        QJsonArray pts = geojson.value("points").toArray();
        updatePoints(pts, false);
        geojson["points"] = pts;
      }
      if (geojson.contains("charges")) {
        QJsonArray chs = geojson.value("charges").toArray();
        updatePoints(chs, true);
        geojson["charges"] = chs;
      }
      if (geojson.contains("highPrecisionPoints")) {
        QJsonArray hps = geojson.value("highPrecisionPoints").toArray();
        updatePoints(hps, true);
        geojson["highPrecisionPoints"] = hps;
      }
      if (geojson.contains("lines")) {
        QJsonArray lns = geojson.value("lines").toArray();
        updateLines(lns);
        geojson["lines"] = lns;
      }
      if (geojson.contains("curves")) {
        QJsonArray cvs = geojson.value("curves").toArray();
        updateLines(cvs);
        geojson["curves"] = cvs;
      }
    }

    ApiService::instance().saveEditedMap(geojson, [this, geojson, mapName, mapId, onDone](int saveCode, const QJsonObject &saveResp) {
      if (saveCode == 200) {
        if (!mapId.isEmpty() && !mapName.isEmpty()) {
          QString localDir = QCoreApplication::applicationDirPath() + "/maps/" + mapName + "_" + mapId;
          QDir().mkpath(localDir);
          QFile rf(localDir + "/route.json");
          if (rf.open(QIODevice::WriteOnly)) {
            rf.write(QJsonDocument(geojson).toJson());
            rf.close();
          }
        }
        qDebug() << "[EditConfigWidget] Route network obstacle parameters synchronized and saved successfully!";
        emit routeNetworkUpdated();
        if (onDone) onDone(true);
      } else {
        QString err = saveResp.value("msg").toString(saveResp.value("message").toString());
        qWarning() << "[EditConfigWidget] Failed to save route network after obstacle sync, code:" << saveCode << "err:" << err;
        if (onDone) onDone(false);
      }
    });
  });
}

void EditConfigWidget::onSave() {
  // 强行使焦点离开输入框和表格，清除当前选中索引，促使 Qt 自动提交正在编辑的输入框数据
  if (QWidget *fw = focusWidget()) {
      fw->clearFocus();
  }
  m_paramTable->clearFocus();
  m_paramTable->setCurrentIndex(QModelIndex());

  // 兜底强行更新所有当前表格中的行，确保每一行都经过 min/max 范围校验并同步
  for (int r = 0; r < m_paramTable->rowCount(); ++r) {
    onParamCellChanged(r, 1);
  }

  // 1. 同步修改 MotionConfigManager 中的全局参数值
  MotionConfigManager::instance().updateFromConfigsArray(m_configs);

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

  // 同步写入本地持久化缓存，保障离线或重启后仍能保持用户配置
  QSettings settings;
  settings.setValue("editConfig/cachedConfigs", QJsonDocument(m_configs).toJson(QJsonDocument::Compact));

  // 同步执行修改路网接口，把路网中的普通点（站点）和线的避障参数修改并保存
  const auto &mc = MotionConfigManager::instance();
  syncObstacleParamsToRouteNetwork(mc.defaultObsStopLength(), mc.defaultObsStopWidth(),
                                   mc.defaultObsDecelLength(), mc.defaultObsDecelWidth());

  m_saveBtn->setEnabled(false);
  auto *loading = new LoadingDialog("正在保存配置参数并同步路网...", this);
  loading->show();
  ApiService::instance().modifyParam(
      payload, [this, loading](int code, const QJsonObject &) {
        loading->close();
        loading->deleteLater();
        m_saveBtn->setEnabled(true);
        if (code == 200) {
          Toast::success(this, "配置参数与路网避障参数已保存");
        } else {
          QMessageBox::warning(this, "错误", "保存配置参数失败 (已在本地缓存最新配置)");
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

  QString newStr = valItem->text().trimmed();

  QJsonObject entry = m_configs[configIdx].toObject();
  QJsonValue params = entry.value("params");

  // Determine original leaf type for type-preserving write-back
  QJsonValue origLeaf = jsonGet(params, segments);
  QJsonValue newLeaf;

  // 1. 获取该参数在接口元数据中的 min 和 max 限制 (若为 null 或空则不限制)
  double minVal = 0.0;
  double maxVal = 0.0;
  bool hasMin = false;
  bool hasMax = false;
  QJsonObject metaObj = findMetadataForConfig(entry);
  getParamMinMax(metaObj, fullPath, minVal, maxVal, hasMin, hasMax);

  // 2. 如果存在 min 或 max 限制，对输入数值施加限制；若为 null 或空则不限制
  if (hasMin || hasMax) {
    bool ok = false;
    double d = newStr.toDouble(&ok);
    if (ok) {
      bool clamped = false;
      if (hasMin && d < minVal) {
        d = minVal;
        clamped = true;
      }
      if (hasMax && d > maxVal) {
        d = maxVal;
        clamped = true;
      }

      if (clamped) {
        QString rangeStr;
        if (hasMin && hasMax) {
          rangeStr = QString("[%1 ~ %2]").arg(minVal).arg(maxVal);
        } else if (hasMin) {
          rangeStr = QString("[>= %1]").arg(minVal);
        } else {
          rangeStr = QString("[<= %1]").arg(maxVal);
        }
        Toast::warning(this, QString("参数「%1」超出允许范围 %2，已限制为 %3")
                                  .arg(fullPath)
                                  .arg(rangeStr)
                                  .arg(d));
      }

      newStr = (d == (long long)d) ? QString::number((long long)d)
                                   : QString::number(d, 'g', 10);
      m_paramTable->blockSignals(true);
      valItem->setText(newStr);
      m_paramTable->blockSignals(false);

      newLeaf = QJsonValue(d);
    } else {
      if (origLeaf.isDouble()) {
        double oldD = origLeaf.toDouble();
        newStr = (oldD == (long long)oldD) ? QString::number((long long)oldD)
                                           : QString::number(oldD, 'g', 10);
        m_paramTable->blockSignals(true);
        valItem->setText(newStr);
        m_paramTable->blockSignals(false);
        newLeaf = QJsonValue(oldD);
        Toast::warning(this, QString("参数「%1」必须为数值").arg(fullPath));
      } else {
        newLeaf = QJsonValue(newStr);
      }
    }
  } else {
    // min 和 max 为 null 或空，不限制
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
  }

  entry["params"] = jsonSet(params, segments, newLeaf).toObject();
  m_configs[configIdx] = entry;

  if (!segments.isEmpty()) {
    const QString lastKey = segments.last();
    if (lastKey == "anti_align_depth") {
      AlignConfigManager::instance().setAntiAlignDepth(newLeaf.toDouble(AlignConfigManager::instance().antiAlignDepth()));
    } else if (lastKey == "cylinder_length") {
      AlignConfigManager::instance().setCylinderLength(newLeaf.toDouble(AlignConfigManager::instance().cylinderLength()));
    } else if (lastKey == "id_stop_range_threshold" || lastKey == "id_deceleration_range_threshold") {
      MotionConfigManager::instance().updateObstacleConfig(entry.value("params").toObject());
      const auto &mc = MotionConfigManager::instance();
      syncObstacleParamsToRouteNetwork(mc.defaultObsStopLength(), mc.defaultObsStopWidth(),
                                       mc.defaultObsDecelLength(), mc.defaultObsDecelWidth());
    }

    QString cfgName = entry.value("name").toString().trimmed().toLower();
    QString cfgPath = entry.value("file_path").toString().trimmed().toLower();
    if (cfgName.contains("motion_control") || cfgPath.contains("motion_control")) {
      MotionConfigManager::instance().updateFromJsonObject(entry.value("params").toObject());
    }
  }
}

void EditConfigWidget::parseMetadata(const QByteArray &rawJson) {
  QJsonParseError err;
  QJsonDocument doc = QJsonDocument::fromJson(rawJson, &err);
  if (err.error != QJsonParseError::NoError || !doc.isObject()) {
    return;
  }
  QJsonObject root = doc.object();
  QJsonArray list;
  if (root.contains("data")) {
    QJsonValue dv = root.value("data");
    if (dv.isArray()) {
      list = dv.toArray();
    } else if (dv.isObject()) {
      QJsonObject dataObj = dv.toObject();
      if (dataObj.contains("data") && dataObj.value("data").isArray()) {
        list = dataObj.value("data").toArray();
      } else {
        for (const auto &k : dataObj.keys()) {
          if (dataObj.value(k).isArray()) {
            list = dataObj.value(k).toArray();
            break;
          }
        }
      }
    }
  }
  if (!list.isEmpty()) {
    m_metadataList = list;
    QSettings settings;
    settings.setValue("editConfig/cachedMetadata", rawJson);
    MotionConfigManager::instance().updateFromMetadataArray(list);
  }
}

QJsonObject EditConfigWidget::findMetadataForConfig(const QJsonObject &configEntry) const {
  if (m_metadataList.isEmpty() || configEntry.isEmpty()) return QJsonObject();

  QString cfgPath = configEntry.value("file_path").toString().trimmed().toLower();
  QString cfgName = configEntry.value("name").toString().trimmed().toLower();
  QString cfgBase = basename(cfgPath).toLower();

  // 1. 完全匹配 file_path
  for (const auto &v : m_metadataList) {
    QJsonObject meta = v.toObject();
    QString mPath = meta.value("file_path").toString().trimmed().toLower();
    if (!cfgPath.isEmpty() && cfgPath == mPath) return meta;
  }

  // 2. 完全匹配 name
  for (const auto &v : m_metadataList) {
    QJsonObject meta = v.toObject();
    QString mName = meta.value("name").toString().trimmed().toLower();
    if (!cfgName.isEmpty() && cfgName == mName) return meta;
  }

  // 3. 匹配 basename
  for (const auto &v : m_metadataList) {
    QJsonObject meta = v.toObject();
    QString mPath = meta.value("file_path").toString().trimmed().toLower();
    QString mBase = basename(mPath).toLower();
    if (!cfgBase.isEmpty() && cfgBase == mBase) return meta;
  }

  // 4. 忽略 .yaml 后缀匹配名称 (如 motion_control 对比 motion_control.yaml)
  auto stripYaml = [](const QString &s) {
    return s.endsWith(".yaml") ? s.left(s.length() - 5) : (s.endsWith(".yml") ? s.left(s.length() - 4) : s);
  };
  QString cleanCfg = stripYaml(cfgName.isEmpty() ? cfgBase : cfgName);
  for (const auto &v : m_metadataList) {
    QJsonObject meta = v.toObject();
    QString mName = meta.value("name").toString().trimmed().toLower();
    QString mBase = basename(meta.value("file_path").toString().trimmed().toLower());
    QString cleanMeta = stripYaml(mName.isEmpty() ? mBase : mName);
    if (!cleanCfg.isEmpty() && cleanCfg == cleanMeta) return meta;
  }

  return QJsonObject();
}

QString EditConfigWidget::getDefaultValueString(const QJsonObject &metaObj,
                                               const QString &fullPath) const {
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

bool EditConfigWidget::getParamMinMax(const QJsonObject &metaObj,
                                      const QString &fullPath,
                                      double &outMin, double &outMax,
                                      bool &hasMin, bool &hasMax) const {
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

void EditConfigWidget::updateDefaultValuesInTable() {
  if (!m_paramTable || m_paramTable->rowCount() == 0) return;
  m_paramTable->blockSignals(true);
  for (int r = 0; r < m_paramTable->rowCount(); ++r) {
    auto *keyItem = m_paramTable->item(r, 0);
    auto *defItem = m_paramTable->item(r, 2);
    if (!keyItem) continue;

    int configIdx = keyItem->data(Qt::UserRole).toInt();
    QString fullPath = keyItem->data(Qt::UserRole + 1).toString();
    if (fullPath.isEmpty()) fullPath = keyItem->text();

    if (configIdx >= 0 && configIdx < m_configs.size()) {
      QJsonObject metaObj = findMetadataForConfig(m_configs[configIdx].toObject());
      QString defaultStr = getDefaultValueString(metaObj, fullPath);
      if (!defItem) {
        defItem = new QTableWidgetItem(defaultStr);
        defItem->setFlags(defItem->flags() & ~Qt::ItemIsEditable);
        defItem->setForeground(QColor(0x94, 0xa3, 0xb8));
        defItem->setData(Qt::UserRole, configIdx);
        defItem->setData(Qt::UserRole + 1, fullPath);
        m_paramTable->setItem(r, 2, defItem);
      } else {
        defItem->setText(defaultStr);
      }
    }
  }
  m_paramTable->blockSignals(false);
}
