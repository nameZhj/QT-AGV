# QT-AGV 智能移动机器人上位机系统

`QT-AGV` 是基于 **Qt 6 (C++17)** 打造的工业级 AGV (Automated Guided Vehicle) 智能移动机器人控制与调度上位机系统。系统集成了设备自动发现与链接管理、实时地图绘制与 SLAM 编辑、可视化任务规划与调度监控、实时数据图表展示、硬件/系统参数树形配置以及全功能遥控顶升控制等核心功能。

---

## 目录

- [项目简介](#项目简介)
- [核心功能特性](#核心功能特性)
- [软件架构与目录结构](#软件架构与目录结构)
- [环境要求与编译指南](#环境要求与编译指南)
- [打包与部署指南](#打包与部署指南)
- [核心通信协议与机制](#核心通信协议与机制)
- [运维与调试说明](#运维与调试说明)

---

## 项目简介

本项目旨在为 AGV / AMR 移动机器人提供高响应、高性能、高扩展度的桌面端控制界面。系统通过 RESTful API、MQTT 及 WebSocket 与 AGV 侧服务端/机器人端控制器进行高频通信，实时展现机器人位姿、传感器状态、激光雷达点云及任务运行日志，并支持对机器人进行可视化路径与任务编排。

---

## 核心功能特性

1. **设备发现与链接管理 (`DeviceManagerDialog`)**
   - 支持局域网设备自动轮询扫描与列表呈现。
   - 支持标准凭证链接及无凭证快速链接 API (`linkCarWithoutKey`)。
   - 具备设备状态本地持久化与卡片式 UI 提示。

2. **控制仪表盘与车辆状态监控 (`CarWidget`, `AgvInfoWidget`)**
   - 实时显示线速度、角速度、电池电量、传感器联通状态及当前导航模式。
   - 支持传感器与硬件状态分项监控及故障诊断。

3. **地图可视化与 SLAM 编辑控制 (`MapViewWidget`, `MapEditWidget`, `CreateMapWidget`, `MapCanvas`)**
   - 针对 PGM / YAML 格式地图提供高效绘制与坐标转换工具 (`MapPgmUtil`)。
   - 支持雷达点云与位姿轨迹实时显示、缩放与视角复位。
   - 支持图层编辑、站点/站点连线（路径线）绘制、障碍物标注与建图流程控制。

4. **任务规划与调度监控 (`TaskWidget`, `TaskEditWidget`, `TaskLogWidget`)**
   - 交互式路径与动作点（如等待、顶升、下降、旋转）编排。
   - 任务下发、暂停、继续、终止控制与实时运行日志追踪。

5. **系统参数树形配置编辑器 (`EditConfigWidget`)**
   - 提供基于树形分组与表格视图的系统配置项统一编辑与保存能力。
   - 包含【运动控制】（最大线速度、最大线加/减速度、最大角速度、到达距离/角度容差、路线宽度）、【任务调度】、【激光雷达驱动】、【PID控制】等多模块参数可视化配置。

6. **全功能键盘遥控与顶升控制 (`TeleopController`)**
   - 支持 WASD / 方向键线速度与角速度连续微调。
   - 支持空格紧急刹车，P / L 快捷键下发顶升机构升降控制指令。
   - 内置安全保护机制：遥控停用或异常断开时自动强制下发零速指令并复位顶升状态。

---

## 软件架构与目录结构

系统严格遵循单向依赖架构规范：`ui -> core -> network -> app / utils`。详细架构指南与 AI 编程助手准则见 [AGENT.md](AGENT.md)。

```
QT-AGV/
├── CMakeLists.txt              # CMake 构建配置
├── main.cpp                    # 程序主入口与样式加载
├── AGENT.md                    # AI 编程助手与开发者架构规范
├── docs/                       # 业务与接口规范文档
│   └── device_flow_apis.md     # 机器人硬件/服务端接口规范
├── src/
│   ├── app/                    # 全局应用配置与常量
│   │   ├── AppConfig.h/.cpp            # 设置持久化与网络参数配置
│   │   └── AppConstants.h              # 窗口尺寸、定时器轮询间隔常量
│   ├── network/                # 网络通信层 (REST API 与 MQTT)
│   │   ├── ApiService.h/.cpp           # REST API 核心服务单例
│   │   ├── MqttService.h/.cpp          # Qt MQTT 客户端服务
│   │   ├── RouteNetworkApi.h/.cpp      # 路网 GeoJSON 通信接口
│   │   └── NetworkConstants.h          # 集中收拢所有 HTTP/MQTT 常量与 JSON 键名
│   ├── core/                   # 核心数据管理与控制算法 (零 UI 依赖)
│   │   ├── MotionConfigManager.h/.cpp  # 运动控制参数管理与持久化
│   │   ├── AlignConfigManager.h/.cpp   # 二次对位与货架参数管理
│   │   ├── RobotInfoManager.h/.cpp     # 机器人状态数据池
│   │   ├── TaskChainManager.h/.cpp     # 任务链与调度业务流
│   │   └── TeleopController.h/.cpp     # 键盘遥控与安全刹车保护
│   ├── utils/                  # 独立基础工具库
│   │   ├── JsonUtil.h/.cpp             # JSON 读写与安全字段提取
│   │   ├── MapPgmUtil.h/.cpp           # PGM / YAML 地图文件二进制解析
│   │   └── ZipUtil.h/.cpp              # 资源包解压工具
│   ├── ui/                     # 用户交互与界面展示层
│   │   ├── mainwindow/                 # 顶层主窗口框架
│   │   ├── common/                     # 通用组件与工厂 (UiHelper, StyleLoader, BaseDialog, ThemeColors)
│   │   ├── dialogs/                    # 业务对话框 (BaseDialog 派生)
│   │   ├── map/                        # 地图监控与拓扑编辑 (MapDrawHelper 坐标运算)
│   │   ├── task/                       # 任务链编排与执行日志
│   │   └── control/                    # 仪表盘、十字摇杆与参数配置树 (ConfigMetadataHelper)
│   └── resources/              # 资源与模块化 QSS 样式表
│       ├── resources.qrc               # 资源清单
│       ├── qss/                        # 模块化 QSS (base, mainwindow, dialogs, map, task, control)
│       └── icons/                      # 统一矢量图标库 (.svg / .ico)
└── installer/                  # Inno Setup 自动化打包目录
```

---

## 环境要求与编译指南

### 1. 开发环境要求

- **操作系统**: Windows 10 / 11 (64-bit)
- **编译器**: MinGW 64-bit (推荐 GCC 13.1.0+, 支持 C++17)
- **Qt 版本**: Qt 6.2+ (当前基于 Qt 6.11.0 MinGW 64 构建)
- **构建工具**: CMake 3.20+ & MinGW Makefiles

### 2. 命令行快速构建 (Windows PowerShell)

```powershell
# 1. 设置 Qt 与 MinGW 环境变量
$env:PATH = "F:\Qt\6.11.0\mingw_64\bin;F:\Qt\Tools\mingw1310_64\bin;" + $env:PATH

# 2. 进入项目目录并配置
cd QT-AGV
cmake -B build -G "MinGW Makefiles"

# 3. 编译可执行程序
cmake --build build --config Release -j4
```

---

## 打包与部署指南

工程内置了基于 **Inno Setup** 的 Windows 一键打包脚本：

1. 确认项目已成功完成 `Release` 模式编译。
2. 确保系统已安装 [Inno Setup 6](https://jrsoftware.org/isinfo.php) 且已添加到系统环境变量或配置在路径中。
3. 进入 `installer` 目录执行打包脚本：

```cmd
cd installer
build_installer.bat
```

打包完成后将在 `installer/output/` 目录下生成可独立分发的 `.exe` 安装程序。

---

## 核心通信协议与机制

### 1. HTTP / REST API 规范
- 通信层由 `ApiService` 封装。
- 负责获取与保存地图节点数据、任务路线配置、系统全局设置及手动设备链接 API (`/api/v1/car/linkWithoutKey` 等)。

### 2. MQTT 实时通信与控制规范
- 通信层由 `MqttService` 封装。
- **速度控制**: 
  - 线速度发布 Topic: `/rob/{agvId}/linear_speed` （Payload: `{"velocity": float}`）
  - 角速度发布 Topic: `/rob/{agvId}/angular_speed` （Payload: `{"velocity": float}`）
- **顶升机构控制**:
  - 顶升状态发布 Topic: `/rob/{agvId}/hoist_status` （Payload: `"1"` 升起, `"2"` 下降, `"0"` 停止）
- **遥控使能控制**:
  - 遥控使能 Topic: `/rob/{agvId}/teleop_enable` （Payload: `"true"` / `"false"`）

---

## 运维与调试说明

1. **应用运行日志 (`qt-agv.log`)**
   - 客户端启动后会自动在程序所在目录生成 `qt-agv.log` 日志文件。
   - 所有 `qDebug()`, `qInfo()`, `qWarning()`, `qCritical()` 信息均会自动带有格式化时间戳并同时写入日志文件与控制台。

2. **局域网网络代理自动禁用**
   - 为避免客户端由于开启 VPN 或代理软件导致 MQTT/TCP 底层套接字被路由至 HTTP/SOCKS 代理引发 `TransportInvalid` 连接失败，程序在启动时强制调用了 `QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy)` 禁用系统代理。

3. **键盘遥控安全下发机制**
   - 当用户在 `CarWidget` 中关闭遥控功能或窗口关闭时，`TeleopController` 会自动触发安全防护，强制向 MQTT 服务端发布一次线速度 `0.0` 和角速度 `0.0`，并恢复顶升状态，防止 AGV 出现失控惯性行驶。
