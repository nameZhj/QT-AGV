# AGENT.md - QT-AGV AI 编程助手与开发者架构规范

> **核心定位**：本文档为 `QT-AGV` 智能移动机器人上位机系统的架构上下文规范。任何 AI Agent 或人类工程师在执行代码改动、重构、Bug 修复或新增功能时，**必须严格遵守**本文档定义的开发铁律与分层准则。

---

## 1. 项目简介与技术栈

- **项目名称**：QT-AGV
- **开发语言**：C++17
- **GUI 框架**：Qt 6 (Widgets 模块, Qt 6.11.0 / MinGW 64)
- **构建系统**：CMake (3.20+) + MinGW Makefiles
- **网络通信**：
  - RESTful API (基于 `QNetworkAccessManager` 的 `ApiService`)
  - MQTT 5.0/3.1.1 (基于 `QtMqtt` 的 `MqttService`)
- **坐标系统**：
  - 地图世界坐标系：以米 (m) 为单位，X 向右为正，Y 向上为正。
  - 视图场景坐标系：以像素 (px) 为单位，X 向右为正，Y 向下为正（需通过 `MapDrawHelper` 转换）。

---

## 2. 编译与构建标准指令

本工程构建强依赖 MinGW 64 与 Qt 6 环境，必须在执行 CMake 前注入环境变量：

```powershell
# 1. 注入 Qt 与 MinGW 工具链环境变量 (Windows PowerShell)
$env:PATH = "F:\Qt\6.11.0\mingw_64\bin;F:\Qt\Tools\mingw1310_64\bin;" + $env:PATH

# 2. 终止后台运行中占用的进程 (避免 ld 链接报 Permission denied)
Stop-Process -Name 'QT-AGV' -Force -ErrorAction SilentlyContinue

# 3. CMake 构建 (Release 模式，多线程并发)
F:\cmake\bin\cmake.exe --build build --config Release -j4
```

> **注意**：新增 `.h` 或 `.cpp` 文件时，若 CMake 未自动检测到，需重新配置：
> `F:\cmake\bin\cmake.exe -B build -G "MinGW Makefiles"`

---

## 3. 真实工作区模块映射表 (Workspace Module Map)

> **真实性审计声明**：本工程所有文件均严格源自物理磁盘实际扫描，共计 **106 个真实目标文件**（102 个 C++ 源码文件 + 1 个 QRC 资源 + 3 个构建配置）。本工程采用纯 C++ 现代代码驱动布局，无任何旧式 `.ui` (XML) 表单文件，无任何虚构类。

```
QT-AGV/
├── CMakeLists.txt                      # CMake 跨平台构建定义
├── IntelligentAGVSchedulingSystem.pro  # Qt qmake 模块化工程配置
├── main.cpp                            # 程序主入口 (QApplication, StyleLoader, 异常捕获)
├── docs/
│   └── device_flow_apis.md             # 机器人硬件/服务端接口规范文档
├── src/
│   ├── app/                            # [DataComm] 全局持久化与系统常量
│   │   ├── AppConfig.h/.cpp            # 全局配置单例 (IP/端口/车辆参数)
│   │   └── AppConstants.h              # 窗口尺寸、轮询间隔、资源 URI 常量
│   ├── network/                        # [DataComm] 协议通信层 (严禁依赖任何 ui/ 模块)
│   │   ├── ApiService.h/.cpp           # RESTful API 客户端 (鉴权、超时与请求派发)
│   │   ├── MqttService.h/.cpp          # Qt MQTT 客户端服务
│   │   ├── RouteNetworkApi.h/.cpp      # 路网 GeoJSON 上传/下载/序列化业务接口
│   │   └── NetworkConstants.h          # 集中收拢所有 HTTP Endpoints、MQTT Topics 与 JSON 键名
│   ├── utils/                          # [DataComm] 独立基础通用工具库 (零业务耦合)
│   │   ├── JsonUtil.h/.cpp             # JSON 安全读写、键值安全提取与点分路径查询
│   │   ├── MapPgmUtil.h/.cpp           # PGM / YAML 地图文件二进制解析与渲染工具
│   │   └── ZipUtil.h/.cpp              # 地图/日志压缩包解压工具
│   ├── core/                           # [AGVModel & CoreTopology] 核心业务模型与调度
│   │   ├── RobotInfoManager.h/.cpp     # [AGVModel] 机器人运行状态数据池与遥测解析
│   │   ├── MotionConfigManager.h/.cpp  # [AGVModel] 运动控制参数管理与双键持久化单例
│   │   ├── AlignConfigManager.h/.cpp   # [AGVModel] 二次对位与货架参数管理单例
│   │   ├── TeleopController.h/.cpp     # [AGVModel] 键盘/摇杆遥控速度计算与安全保护
│   │   ├── TaskChainManager.h/.cpp     # [CoreTopology] 任务链调度引擎与执行流管理
│   │   ├── RouteFeatureSelect.h/.cpp   # [CoreTopology] 站点/线路要素选择枚举
│   │   └── RouteNetworkValidator.h/.cpp# [CoreTopology] 路网拓扑合规性与碰撞校验器
│   ├── ui/                             # [GUILayer] 用户交互与界面展示层
│   │   ├── mainwindow/                 # 顶层应用主框架
│   │   │   └── MainWindow.h/.cpp       # 顶部导航、侧边栏、状态栏与视图多页调度
│   │   ├── common/                     # 通用 UI 组件与控件工厂
│   │   │   ├── UiHelper.h/.cpp         # 控件工厂 (带过滤器 SpinBox、带动效按钮、标准表格)
│   │   │   ├── StyleLoader.h/.cpp      # 模块化 QSS 自动合并与动态加载器
│   │   │   ├── ThemeColors.h           # 自绘控件统一颜色与主题调色板常量
│   │   │   ├── Toast.h/.cpp            # 全局浮动气泡通知
│   │   │   ├── LoadingDialog.h/.cpp    # 模态加载指示框
│   │   │   ├── ButtonClickEffect.h/.cpp# 按钮点击回弹动效全局过滤器
│   │   │   ├── SpinBoxWheelFilter.h/.cpp# 步进器非聚焦状态滚轮屏蔽事件过滤器
│   │   │   └── ToggleSwitch.h/.cpp     # 仿 iOS/现代风自绘滑动开关
│   │   ├── control/                    # 底盘控制与系统配置面板
│   │   │   ├── CarWidget.h/.cpp        # 仪表盘、底盘状态与快捷操作
│   │   │   ├── DirectionPadWidget.h/.cpp# 软方向盘十字控制板
│   │   │   ├── EditConfigWidget.h/.cpp # 系统参数树形配置主面板
│   │   │   └── ConfigMetadataHelper.h/.cpp # 参数元数据解析与校验辅助类
│   │   ├── dialogs/                    # 业务弹窗组件 (继承自 BaseDialog)
│   │   │   ├── BaseDialog.h/.cpp       # 统一弹窗基类 (标准边距、标题栏、操作按钮盒)
│   │   │   ├── DeviceManagerDialog.h/.cpp  # 设备扫描与连接管理对话框
│   │   │   ├── MotionDialog.h/.cpp     # 运动控制参数快捷调节弹窗
│   │   │   ├── NetworkConfigDialog.h/.cpp  # 网络与 IP 配置弹窗
│   │   │   ├── RobotStatusDialog.h/.cpp# 机器人详细硬件运行状态弹窗
│   │   │   ├── MapCheckResultDialog.h/.cpp # 地图诊断与拓扑报错结果展示对话框
│   │   │   ├── MapUploadDialog.h/.cpp  # 地图资源包导入对话框
│   │   │   ├── SmapUploadDialog.h/.cpp # SMAP 地图上传对话框
│   │   │   ├── AudioManagerDialog.h/.cpp   # 音频提示音管理对话框
│   │   │   └── ManualControlConfigDialog.h/.cpp # 手动控制按键配置对话框
│   │   ├── map/                        # 地图显示与编辑视图
│   │   │   ├── MapWidget.h/.cpp        # 地图模块顶层路由切换容器
│   │   │   ├── MapViewWidget.h/.cpp    # 实时地图监控、点云图层与位姿渲染
│   │   │   ├── MapEditWidget.h/.cpp    # 站点拓扑与路径编辑主视图
│   │   │   ├── MapCanvas.h/.cpp        # 地图底层画布交互
│   │   │   ├── MapDrawHelper.h/.cpp    # 坐标转换 (world<->scene) 与自绘算法
│   │   │   ├── MapShowWidget.h/.cpp    # 地图选点与展示组件
│   │   │   ├── CreateMapWidget.h/.cpp  # 在线建图 (SLAM) 控制
│   │   │   ├── MapImageEditWidget.h/.cpp# 地图底图旋转/擦除/裁剪二次编辑
│   │   │   └── MapPaintCanvas.h/.cpp   # 地图底图笔刷与橡皮擦画布
│   │   └── task/                       # 任务规划与运行视图
│   │       ├── TaskWidget.h/.cpp       # 任务管理主页面
│   │       ├── TaskChainWidget.h/.cpp  # 任务链编排与节点拖拽主界面
│   │       ├── TaskEditWidget.h/.cpp   # 任务动作明细参数编辑面板
│   │       └── TaskLogWidget.h/.cpp    # 任务执行流日志追踪与状态监视
│   └── resources/                      # 资源与模块化样式表
│       ├── resources.qrc               # Qt 资源描述文件
│       ├── app.rc.in                   # Windows 应用图标与版本资源模板
│       ├── qss/                        # 模块化 QSS 样式表 (base, control, dialogs, mainwindow, map, task)
│       └── icons/                      # 统一矢量图标库 (.svg / .ico)
```

---

## 4. 公共组件与工具类速查表 (开发强制复用)

任何新代码编写或重构时，**严禁重写重复样板代码**，必须优先调用以下公共基建：

| 业务场景 | 严禁做法 | 规范推荐做法 |
| :--- | :--- | :--- |
| **创建表单输入框** | 手动 `new QSpinBox` 并挂载 `SpinBoxWheelFilter` | 调用 `UiHelper::createSpinBox(...)` 或 `createDoubleSpinBox(...)` |
| **创建按钮** | 手动 `new QPushButton` 并设置光标和动效 | 调用 `UiHelper::createPushButton(text, icon, objectName, parent)` |
| **新建弹窗** | 从 `QDialog` 开始反复拼装按钮栏和样式 | 继承 `BaseDialog`，调用 `createButtonBox()` 自动获得规范外观与布局 |
| **JSON 文件/网络解析** | 手写 `QFile`、`QJsonDocument::fromJson` | 调用 `JsonUtil::readJsonObjectFromFile`、`JsonUtil::parseJsonObject` |
| **读取 JSON 字段** | 手写多层嵌套 if 判断与默认值转换 | 调用 `JsonUtil::getString`、`JsonUtil::getDouble`、`JsonUtil::getValueByPath` |
| **地图坐标变换** | 在视图类中手写 `(wx - worldL) / mapW * imgW` | 调用 `MapDrawHelper::worldToScene(...)` 与 `sceneToWorld(...)` |
| **几何计算与箭头** | 手写点线距离或箭头三角函数 | 调用 `MapDrawHelper::distancePointToSegment`、`makeArrowHead` |
| **自绘颜色配置** | 在 `paintEvent` 出现裸十六进制或裸 `QColor(r,g,b)` | 统一引用 `ThemeColors::*` (如 `ThemeColors::Map::kGridLine`) |
| **HTTP/MQTT 常量** | 在各 Widget 中散落 URL 字符串字面量 | 统一引用 `NetworkConstants::Endpoints::*` 与 `NetworkConstants::MqttTopics::*` |
| **全局参数常量** | 在代码中散落定时器间隔、默认长宽 | 统一引用 `AppConstants::*` (如 `AppConstants::Timer::kRobotStatusMs`) |

---

## 5. 开发铁律与架构红线

1. **单向依赖红线 (Strict One-Way Dependency)**：
   - 依赖链路必须为：`ui -> core -> network -> app / utils`。
   - `core/`、`network/`、`utils/` 模块**绝对严禁 `#include` 任何 `ui/` 下的头文件**。通信必须通过 Qt 信号槽 (`emit signal(...)`)，由 `ui/` 层统一接收并调度弹窗或视觉更新。
2. **零裸样式与 QSS 集中化**：
   - 严禁在 C++ 代码中拼接大段 `setStyleSheet(...)` 字符串；
   - 样式一律定义在 `src/resources/qss/` 对应的模块样式表中；
   - 动态状态切换一律使用 Qt 动态属性：`widget->setProperty("status", "error"); widget->style()->repolish(widget);`。
3. **头文件治理规范**：
   - 所有头文件一律统一使用 `#pragma once`（严禁使用旧式 `#ifndef` 守卫）；
   - 头文件除基类和标准值类型外，其余类型指针成员**必须使用前置声明**（如 `class QLineEdit;`），将具体头文件包含移入 `.cpp`。
4. **内存安全规范**：
   - 所有通过 `new` 创建的 `QObject` / `QWidget` 派生类必须显式传入 `parent`，纳入 Qt 对象树自动内存管理；
   - 独立生命周期的堆内存分配必须使用 `std::unique_ptr` 或 `std::shared_ptr`，严禁悬空裸指针。
5. **防御性编程**：
   - 对所有来自网络（REST / MQTT）的 JSON 数据进行判空与有效性校验；
   - 涉及除法运算（如地图缩放比、距离计算）必须增加分母防零（`<= 1e-6`）保护。
