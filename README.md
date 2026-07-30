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

6. **全功能键盘遥控与顶升控制 (`TeleopController`)**
   - 支持 WASD / 方向键线速度与角速度连续微调。
   - 支持空格紧急刹车，P / L 快捷键下发顶升机构升降控制指令。
   - 内置安全保护机制：遥控停用或异常断开时自动强制下发零速指令并复位顶升状态。

---

## 软件架构与目录结构

```
QT-AGV/
├── CMakeLists.txt              # CMake 构建配置文件
├── CMakePresets.json           # CMake 预设配置
├── main.cpp                    # 应用入口、日志初始化、代理设置与全局事件处理
├── src/
│   ├── core/                   # 核心服务、网络通信与业务控制器
│   │   ├── ApiService.h/.cpp           # REST API 通信服务 (QNetworkAccessManager)
│   │   ├── AppConfig.h/.cpp            # 应用配置单例 (设置持久化、IP/卡号管理)
│   │   ├── ButtonClickEffect.h/.cpp    # 全局 UI 按钮点击动画事件过滤器
│   │   ├── DeviceManagerDialog.h/.cpp  # 启动设备扫描与连接管理对话框
│   │   ├── LoadingDialog.h/.cpp        # 全局加载指示对话框
│   │   ├── MainWindow.h/.cpp           # 主窗口框架及多页面切换
│   │   ├── MapPgmUtil.h/.cpp           # PGM 地图文件解析与渲染转换工具
│   │   ├── MqttService.h/.cpp          # Qt MQTT 客户端封装 (实时遥测与控制指令)
│   │   ├── TeleopController.h/.cpp     # 键盘遥控与安全刹车控制器
│   │   ├── Toast.h/.cpp                # 全局消息浮层提示框
│   │   ├── ToggleSwitch.h/.cpp         # 自定义开关控件
│   │   └── WebSocketService.h/.cpp     # WebSocket 实时推流通信服务
│   ├── widgets/                # 业务UI视图与交互组件
│   │   ├── AgvInfoWidget.h/.cpp        # AGV 硬件与系统信息展示
│   │   ├── CarWidget.h/.cpp            # 车辆仪表盘与主控制面板
│   │   ├── CreateMapWidget.h/.cpp      # 建图控制与地图创建组件
│   │   ├── EditConfigWidget.h/.cpp     # 系统参数树形编辑面板
│   │   ├── MapCanvas.h/.cpp            # 地图底层画布控件
│   │   ├── MapEditWidget.h/.cpp        # 地图站点与路径在线编辑器
│   │   ├── MapImageEditWidget.h/.cpp   # 地图图像处理与裁切编辑
│   │   ├── MapPaintCanvas.h/.cpp       # 地图矢量标绘画布
│   │   ├── MapShowWidget.h/.cpp        # 地图展示与点云位姿渲染
│   │   ├── MapViewWidget.h/.cpp        # 综合地图视图控制主组件
│   │   ├── MapWidget.h/.cpp            # 地图模块外层容器组件
│   │   ├── TaskEditWidget.h/.cpp       # 任务动作与路线编排组件
│   │   ├── TaskLogWidget.h/.cpp        # 任务执行日志面板
│   │   └── TaskWidget.h/.cpp           # 任务管理主页面容器
│   └── resources/              # 矢量图标与资源文件 (.rc / .ico / .svg)
└── installer/                  # 打包工具目录
    ├── build_installer.bat     # Inno Setup 自动化打包脚本
    ├── installer.iss           # Inno Setup 安装包配置文件
    └── Languages/              # 界面语言扩展配置文件
```

---

## 环境要求与编译指南

### 1. 开发环境要求

- **操作系统**: Windows 10 / 11 (64-bit) 或 Linux
- **编译器**: MSVC 2019/2022 (64-bit) 或 MinGW 64-bit (推荐 C++17 支持)
- **Qt 版本**: Qt 6.2+ (需包含 `Core`, `Gui`, `Widgets`, `Network`, `WebSockets`, `Mqtt`, `Svg` 模块)
- **构建工具**: CMake 3.16+ & Ninja / NMake

### 2. CMake 编译步骤

#### 命令行构建 (Windows MSVC / MinGW / Ninja)

```bash
# 1. 克隆代码仓库
git clone http://180.201.6.137:3000/zanghongjie/AGV-QT.git
cd AGV-QT/QT-AGV

# 2. 创建并配置构建目录 (例如使用 Release 模式)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 3. 执行编译
cmake --build build --config Release
```

#### Qt Creator 构建

1. 打开 Qt Creator，选择 `Open File or Project...`。
2. 选择 `QT-AGV/CMakeLists.txt`。
3. 选择配置好的 Qt 6 Kit (如 Qt 6.5.3 MSVC2019 64bit)。
4. 点击 `Build` (Ctrl+B) 与 `Run` (Ctrl+R)。

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
