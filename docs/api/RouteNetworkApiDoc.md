# 工业级 AGV/AMR 路网数据规范与 API 接口说明文档
> **版本**：v1.2.0 (工业增强版)  
> **适用模块**：地图编辑 (MapEditWidget)、主监控看板 (MapShowWidget)、全局视图 (MapViewWidget)、任务执行与编辑 (TaskWidget / TaskEditWidget)、拓扑调度引擎

---

## 一、双向路网绘制与拓扑展开机制

在工业 AGV/AMR 地图编辑系统中（参考业界主流工业调度标准），路网中的**直线**与**贝塞尔曲线**均支持单向（前进/后退）与双向通行配置。

### 1. 直线双向绘制机制
- **单向前进 (`direct == 0`)**：在线段中心点绘制一个指向终点的流线型箭头（→）。
- **单向后退 (`direct == 1`)**：在线段中心点绘制一个指向起点的流线型反向箭头（←）。
- **双向通行 (`direct == 10` 或 `direct == 2`)**：
  - 在直线中点两侧，沿线段矢量方向对称偏移 $L_{\text{gap}} = \text{clamp}(3.5, \text{len} \times 0.12, 7.5)\text{px}$；
  - 正向箭头朝向终点（起点至终点方向）；
  - 反向箭头朝向起点（终点至起点方向）；
  - 形成标准双向箭头指示：`←  →`。

### 2. 贝塞尔曲线双向绘制机制
- 曲线不能使用简单的起终点连线，必须**严格沿曲线参数曲线切线方向**进行评估：
  - 取曲线弧长参变量 $t_1 = 0.50 + \Delta t$（通常 $\Delta t = 0.03 \sim 0.05$）：
    - 坐标计算：$P_1 = \text{EvaluateBezier}(ctrl, t_1)$；
    - 切线矢量：$\vec{d_1} = \text{EvaluateBezier}(ctrl, t_1 + 0.02) - \text{EvaluateBezier}(ctrl, t_1 - 0.02)$；
    - 绘制前进方向箭头（贴合曲线弯曲，指向终点）。
  - 取曲线弧长参变量 $t_2 = 0.50 - \Delta t$：
    - 坐标计算：$P_2 = \text{EvaluateBezier}(ctrl, t_2)$；
    - 切线矢量：$\vec{d_2} = \text{EvaluateBezier}(ctrl, t_2 + 0.02) - \text{EvaluateBezier}(ctrl, t_2 - 0.02)$；
    - 绘制反向箭头（方向取 $-\vec{d_2}$，贴合曲线弯曲，指向起点）。
- 双向箭头自然紧贴曲线弯曲弧度，呈现极佳的工业美学与明确的通行导向。

---

## 二、数据格式兼容性与工业扩展字段

本系统采用与既有后端完全兼容的标准 **GeoJSON FeatureCollection** 格式存储路网数据。所有原有字段保持不变，新增的工业级调度参数收敛放置在 `properties` 字典中，实现 **100% 向上扩展与向下兼容**。

### 1. 站点数据格式 (`Point`)
```json
{
  "type": "Feature",
  "geometry": {
    "type": "Point",
    "coordinates": [2.005, 3.055, 0.0]
  },
  "properties": {
    "id": "p_1",
    "name": "LM1",
    "type": "point",
    "yaw": 0.0,
    "rdsDockable": true,
    "allowRotate": false,
    "useDownPGV": false,
    "epc": "E28011700000020",
    "isyawfix": false,
    "taskid": 0,

    "ignoreDir": false,
    "label": "停靠点",
    "waitTimeout": 0.0
  }
}
```

| 字段名称 | 类型 | 默认值 | 字段说明 | 兼容性 |
| :--- | :--- | :--- | :--- | :--- |
| `id` | string | 必填 | 站点唯一标识（如 `p_1`, `c_1`, `hp_1`） | 原有字段 |
| `name` | string | 必填 | 站点显示名称（如 `LM1`, `CP1`, `TL1`） | 原有字段 |
| `type` | string | "point" | 站点类型：`point` (普通站点), `charge` (充电桩), `highPrecision` (精确定位点) | 原有字段 |
| `coordinates` | array | [x, y, 0] | 世界物理坐标 $(x, y)$，单位：米 (m) | 原有字段 |
| `yaw` | double | 0.0 | 站点推荐车头朝向角，单位：弧度 (rad) | 原有字段 |
| `rdsDockable` | bool | false | 调度停靠使能（true=允许作为任务停靠点） | 原有字段 |
| `allowRotate` | bool | false | 原地旋转使能（true=允许车体在站点原地打转） | 原有字段 |
| `useDownPGV` | bool | false | 下视传感器使能（true=到达时启用下视二维码/PGV标定） | 原有字段 |
| `epc` | string | "" | 站点 RFID / EPC 二维码校验码 | 原有字段 |
| `ignoreDir` | bool | false | 调度导航时是否忽略车头朝向 | **新增扩展** |
| `label` | string | "" | 业务标签（如“库位1号”、“上料口”） | **新增扩展** |
| `waitTimeout` | double | 0.0 | 站点允许最大停留超时时间（秒） | **新增扩展** |

---

### 2. 直线数据格式 (`LineString`)
```json
{
  "type": "Feature",
  "geometry": {
    "type": "LineString",
    "coordinates": [
      [0.779, 0.042, 0.0],
      [2.005, 3.055, 0.0]
    ]
  },
  "properties": {
    "id": "l_1",
    "name": "L1",
    "type": "line",
    "sId": "p_1",
    "eId": "p_2",
    "direct": 10,
    "direction": 10,
    "maxSpeed": 0.5,
    "minSpeed": 0.0,
    "maxAcc": 0.3,
    "maxDec": 0.3,
    "maxRotSpeed": 60.0,
    "maxRotAcc": 45.0,
    "reachDist": 0.05,
    "reachAngle": 2.0,
    "roadWidth": 1.0,
    "moveStyle": 0,
    "locked": false,
    "weight": 1.0,
    "priority": 0,
    "spinTurn": true,
    "avoidObstacle": true
  }
}
```

---

### 3. 贝塞尔曲线数据格式 (`MultiPoint`)
```json
{
  "type": "Feature",
  "geometry": {
    "type": "MultiPoint",
    "coordinates": [
      [1.126, 3.875, 0.0],
      [2.233, 4.500, 0.0],
      [3.340, 4.500, 0.0],
      [4.449, 3.875, 0.0]
    ]
  },
  "properties": {
    "id": "b_1",
    "name": "曲1",
    "type": "curve",
    "sId": "p_2",
    "eId": "p_3",
    "direct": 10,
    "direction": 10,
    "controlPoints": [
      {"x": 1.126, "y": 3.875},
      {"x": 2.233, "y": 4.500},
      {"x": 3.340, "y": 4.500},
      {"x": 4.449, "y": 3.875}
    ],
    "maxSpeed": 0.5,
    "minSpeed": 0.0,
    "maxAcc": 0.3,
    "maxDec": 0.3,
    "maxRotSpeed": 60.0,
    "maxRotAcc": 45.0,
    "reachDist": 0.05,
    "reachAngle": 2.0,
    "roadWidth": 1.0,
    "moveStyle": 0,
    "locked": false,
    "weight": 1.0,
    "priority": 0,
    "spinTurn": true,
    "avoidObstacle": true
  }
}
```

| 字段名称 | 类型 | 默认值 | 字段说明 | 兼容性 |
| :--- | :--- | :--- | :--- | :--- |
| `id` | string | 必填 | 线路唯一编号（直线 `l_x`, 曲线 `b_x`） | 原有字段 |
| `name` | string | 必填 | 线路名称 | 原有字段 |
| `type` | string | "line"/"curve" | 线路类型 | 原有字段 |
| `sId` / `startid` | string | 必填 | 起点站点 ID | 原有字段 |
| `eId` / `endid` | string | 必填 | 终点站点 ID | 原有字段 |
| `direct` | int | 10 | 通行方向：`10` (双向), `0` (正向前进), `1` (反向后退) | 原有字段 |
| `direction` | int | 10 | 工业标准别名（与 `direct` 等价，取值 10/2 为双向） | **新增扩展** |
| `controlPoints`| array | [...] | 贝塞尔控制点列表，按序包含起点、中间控制点与终点 | 原有字段 |
| `maxSpeed` | double | 0.5 | 最大运行线速度 (m/s) | 原有字段 |
| `minSpeed` | double | 0.0 | 最小巡航线速度 (m/s) | 原有字段 |
| `maxAcc` | double | 0.3 | 最大线加速度 (m/s²) | 原有字段 |
| `maxDec` | double | 0.3 | 最大线减速度 (m/s²) | 原有字段 |
| `maxRotSpeed` | double | 60.0 | 最大旋转角速度 (°/s) | 原有字段 |
| `maxRotAcc` | double | 45.0 | 最大旋转角加速度 (°/s²) | **新增扩展** |
| `reachDist` | double | 0.05 | 到点距离允许误差容差 (m) | 原有字段 |
| `reachAngle` | double | 2.0 | 到点车头角度允许误差容差 (°) | 原有字段 |
| `roadWidth` | double | 1.0 | 虚拟通道包络宽度 (m) | 原有字段 |
| `moveStyle` | int | 0 | 导航模式：`0`:路径导航, `1`:自由导航/激光, `4`:二维码 | **新增扩展** |
| `locked` | bool | false | 是否临时封闭/锁定此路线 | **新增扩展** |
| `weight` | double | 1.0 | 路径规划代价权重（默认 1.0） | **新增扩展** |
| `priority` | int | 0 | 交通管制与通行优先级 | **新增扩展** |
| `spinTurn` | bool | true | 是否允许车辆在该段两端原地自转掉头 | **新增扩展** |
| `avoidObstacle` | bool | true | 是否允许局部自主绕障 | **新增扩展** |

---

## 三、C++ 路网 API (`RouteNetworkApi`) 使用指南

头文件路径：`src/core/RouteNetworkApi.h`

### 1. 核心 API 方法一览

```cpp
#include "core/RouteNetworkApi.h"

// 1. 从 GeoJSON 解析内存对象
RouteNetwork network = RouteNetworkApi::fromGeoJson(rootJson);

// 2. 内存对象序列化为原生 GeoJSON
QJsonObject geojson = RouteNetworkApi::toGeoJson(network);

// 3. 将双向路网展开为有向拓扑图 (供 A* 调度寻路)
QList<DirectedEdge> graph = RouteNetworkApi::toDirectedGraph(network);

// 4. 导入与导出工业 .smap 文件
RouteNetwork smapNet;
RouteNetworkApi::parseSmap(smapJson, smapNet);
QJsonObject exportedSmap = RouteNetworkApi::exportSmap(network);

// 5. 校验路网数据完整性与拓扑连通性
QStringList warnings, errors;
bool ok = RouteNetworkApi::validate(network, warnings, errors);

// 6. 异步网络加载与保存
RouteNetworkApi::loadNetwork("196", [](bool success, const RouteNetwork &net, const QString &err) {
    if (success) {
        qDebug() << "Loaded points:" << net.points.size() << "lines:" << net.lines.size();
    }
});

RouteNetworkApi::saveNetwork("196", "5090721", network, [](bool success, const QString &msg) {
    qDebug() << "Save result:" << success << msg;
});
```

### 2. 双向路线展开原理 (`toDirectedGraph`)
调度算法需要单向有向图进行 Dijkstra/A* 计算。`RouteNetworkApi::toDirectedGraph()` 会自动完成以下处理：
1. 若 `seg.direct == 10 || seg.direct == 2`（双向）：
   - 生成正向有向边：`source=startId`, `target=endId`, 控制点正序；
   - 生成反向有向边：`source=endId`, `target=startId`, 控制点逆序，标识 `isReverse=true`；
   - 自动精确积分测算贝塞尔曲线的实际物理弧长 $\int \sqrt{\dot{x}^2 + \dot{y}^2} dt$；
2. 若 `seg.direct == 0`（单向前进）：仅生成起点至终点的有向边；
3. 若 `seg.direct == 1`（单向后退）：仅生成终点至起点的有向边。

---

## 四、REST 通讯接口规范

### 1. 加载地图路网
- **接口路径**：`POST /robot/map/route/load`
- **请求体 (Content-Type: application/json)**：
  ```json
  {
    "mapId": "196"
  }
  ```
- **响应体**：
  ```json
  {
    "code": 200,
    "msg": "已查找到路径文件",
    "json": "{\"type\":\"FeatureCollection\",\"features\":[...]}"
  }
  ```

### 2. 保存地图路网
- **接口路径**：`POST /robot/map/route/save`
- **请求体 (Content-Type: application/json)**：
  标准的 GeoJSON `FeatureCollection` 根对象（由 `RouteNetworkApi::toGeoJson` 生成）。
- **响应体**：
  ```json
  {
    "code": 200,
    "msg": "保存成功"
  }
  ```
