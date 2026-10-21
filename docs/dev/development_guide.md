# QT-AGV 开发者开发与架构指南

## 1. 工程架构分层规范
本项目严格遵循四层单向依赖架构：
`UI/Widgets -> Core (AGVModel / Scheduling) -> ProtocolComm (Network) -> App / Utils`

1. **UI / Widgets 层 (`src/ui/`, `src/widgets/`)**:
   - 视图、主窗口、自定义控件组件、状态看板、拓扑绘制与交互。
   - 依赖 Core 与 ProtocolComm，严禁 Core 反向依赖 UI。
2. **Core 核心业务与调度层 (`src/core/`)**:
   - AGV 模型与状态池 (`RobotInfoManager`)
   - 运动学与遥控计算 (`TeleopController`)
   - 运动参数管理器 (`MotionConfigManager`)
   - 二次对位与货架参数 (`AlignConfigManager`)
   - 任务链状态机与调度 (`TaskChainManager`)
   - 拓扑路网校验器 (`RouteNetworkValidator`)
3. **ProtocolComm 通信与协议层 (`src/network/`)**:
   - RESTful API 客户端 (`ApiService`)
   - MQTT 5.0/3.1.1 客户端 (`MqttService`)
   - 路网 GeoJSON 协议交互 (`RouteNetworkApi`)
   - 统一协议常量与键名 (`NetworkConstants.h`)
4. **App / Utils 基础支撑层 (`src/app/`, `src/utils/`)**:
   - 全局配置单例 (`AppConfig`) 与应用常量 (`AppConstants.h`)
   - JSON 容错解析工具 (`JsonUtil`)
   - PGM / YAML 地图二进制解析工具 (`MapPgmUtil`)
   - Zip 解压工具 (`ZipUtil`)

---

## 2. 构建与编译环境
- **构建工具**: CMake 3.16+ / Ninja / MinGW 13.1.0 64-bit
- **Qt 版本**: Qt 6.11.0 (mingw_64)
- **C++ 标准**: C++17
- **常用构建命令**:
  ```powershell
  # 配置
  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
  # 编译
  cmake --build build -j4
  ```

---

## 3. 代码重构与贡献规范
- **AI 语义注释头**: 每个 `.h` 与 `.cpp` 开头统一保留标准 6 行元数据头（`@file`, `@layer`, `@brief`, `@deps`, `@note`）。
- **零回归准则**: 不随意修改现有 QSS 样式、UI 控件布局尺寸、协议字段名以及信号槽连接。
- **模块化构建**: 新增类库或源文件时，务必在同一事务中同步更新 `CMakeLists.txt` 与 `IntelligentAGVSchedulingSystem.pro`。
