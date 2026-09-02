# AutoFlotationSheet 架构说明

项目采用 C++20、Qt 6 Widgets 和 xmake。设计目标是把画布交互、业务数据、计算拓扑和视觉标注分离，避免在 `QGraphicsItem::paint()` 或主窗口中堆积领域逻辑。

## 模块目录

### `src/app`

应用装配层。`MainWindow` 创建场景、视图、停靠面板和工具栏，并连接各模块的信号。顶部动作按项目、流程、方案、显示和导出职责组织为下拉菜单。主窗口可以协调用户用例，但不实现计算公式、连接算法、结果表格或标注绘制。

### `src/core`

最基础的领域对象。目前包含 `FlotationUnit`。该目录不得依赖 `app`、`ui`、`editor` 或 `graphics`。

### `src/document`

项目文档状态：

- 项目共享的产品名称、结果标注记录、标注文字样式；
- 项目共享的 Excel 物流导出顺序（稳定物流 ID 列表）；
- 项目级 `ComponentDefinition`：稳定组分 ID 和显示名称；
- `ExperimentScenario`：单个试验方案的质量、按组分保存的品位/合流占比、药剂制度和最近计算结果；
- 当前方案及方案增删、复制、重命名、切换状态。

修改质量或品位时只使当前方案结果失效；修改画布拓扑时使全部方案结果失效。图元不得自行保存另一份实验数据。

### `src/graphics`

画布图元及局部鼠标交互：

- `FlotationUnitItem`：浮选槽体；
- `InputLineItem`：入料线；
- `ProductLineItem`：左右产品线；
- `MergeJunctionItem`：两条产品合并为终端产品；
- `FeedJunctionItem`：任意数量附加产品/合流输出与正常入料的汇合及公共入料箭头；正常入料可以是外部新鲜入料、上游产品或合流输出。

产品物流使用稳定 ID：`<unit-id>:left`、`<unit-id>:right`；外部入料使用 `<unit-id>:feed`；合流输出使用 `<merge-id>:output`。

### `src/editor`

画布关系和编辑命令：

- `FlowsheetScene` 管理连接、断开、合流、拆分、连通组件移动和几何刷新；
- `CanvasActions` 对当前选择执行宽度、连接长度和断开操作；
- `CanvasView` 管理视图级交互：以鼠标位置为中心进行 20%～400% 缩放、鼠标中键平移，以及方向键的优先分派。视图通过注入的处理器请求药剂微调，不识别具体标注类型。

关系变化发出 `topologyChanged`，位置或尺寸变化发出 `geometryChanged`。前者会使计算失效，后者只更新标注锚点。

普通产品或产品合并输出连接到已占用入料时，`FlowsheetScene` 创建或扩展 `FeedJunctionItem`，且不自动移动目标单元。`FeedJunctionItem` 以集合保存任意数量的 `ProductLineItem` 和 `MergeJunctionItem` 附加来源；来源属于同一连通流程时构成回流。目标入料原先已经连接正常上游产品或合流输出时，汇合点会单独保存该端点，整体断开后自动恢复。

一个场景可以包含多个 `FeedJunctionItem`，用于表达独立、串联或嵌套回流环。连通分量遍历必须同时沿附加回流来源和汇合点保存的正常上游来源反向遍历，不能因直接入料线被汇合点替换而切断拓扑连通性。

### `src/adapters`

边界适配层。`CanvasTopologyBuilder` 把画布图元关系转换为纯计算 `TopologyGraph`，并生成报表物流（外部总入料及全部产品）、终端产品和必填实测物流的描述。只有该层允许同时了解画布图元和计算拓扑。

### `src/topology`

与界面完全独立的计算内核：

- `TopologyGraph`：节点、端口和物流；
- `TopologyValidator`：端口、环路、连通性和终端检查；
- `TopologyAlgorithms`：拓扑排序；
- `OpenCircuitCalculator`：单个组分的质量与组分守恒反算；`FlowsheetCalculationService` 对项目定义的每个组分分别调用计算器并聚合结果。

该模块不得依赖 `QGraphicsItem` 或窗口控件。

### `src/services`

应用用例和无状态服务：

- `FlowsheetCalculationService`：读取文档实测值、构建画布拓扑并调用计算内核；
- `ThemeService`：应用及保存明暗主题；
- `SvgExporter`：将当前场景导出为 SVG 矢量图；
- `RasterExporter`：将当前场景导出为 PNG 或 JPEG，并通过最长边及总像素限制控制内存占用。
- `ExcelDataExporter`：将各试验方案的最终及中间产品计算结果写入标准 XLSX 工作簿。

流程图导出服务使用相同的图元包围范围和留白，并统一绘制白色背景；Excel 导出读取文档方案和拓扑快照。所有导出服务均不修改图元关系或文档数据。

### `src/annotations`

通用画布标注系统：

- `AnnotationTypes`：标注类型、样式、所有者和结果指标设置；
- `AnnotationItem`：选择、拖动、绘制和主题表现；
- `ReagentAnnotationItem`：药剂小叉、化学式上下标、文本表现和相对锚点偏移；
- `AnnotationContentFormatter`：业务数据到显示文字；
- `AnnotationManager`：按稳定 ID 创建、更新、隐藏并定位标注。

药剂标注按当前试验方案存放，包含名称、用量、单位、备注和垂直偏移；结果标注位置属于项目共享数据。两类标注由 `AnnotationManager` 统一负责生命周期和锚点更新。药剂方向键微调采用 `CanvasView → AnnotationManager → ReagentAnnotationItem` 单一路径，避免与画布滚动重复处理。

### `src/ui`

非画布界面组件：

- `TerminalProductTableModel`：全部产品物流表模型；根据项目组分动态生成品位与组分占比列，必填实测物流可编辑质量和各组分品位；
- `TerminalProductDock`：面板装配、填写进度和计算按钮状态；
- `ResultDetailsView`：物流和浮选单元结果表；
- `TerminalProductStyle`：面板主题样式生成；
- `ResultMetricMenu`：结果标注指标选择菜单。
- `ScenarioComparisonDialog`：按最终或中间产品比较各方案已计算结果，并显示用户填写的产品名称，不修改文档状态。

## 主要数据流

```text
QGraphicsItem 关系
      │
      ▼
CanvasTopologyBuilder ──► TopologyGraph
      │                       │
      │ terminal products     ▼
      └──────────────► FlowsheetCalculationService
                               │
FlowsheetDocument measurements ┘
               │
               ▼
        CalculationResult（全流程完成 / 全部物流完成）
          │            │
          ▼            ▼
ResultDetailsView  AnnotationManager
```

## 依赖方向

```text
app ─► ui / editor / services / annotations
ui ─► document / topology
services ─► adapters / document / topology
annotations ─► document / editor / graphics / topology
adapters ─► editor / graphics / topology
editor ─► graphics
graphics ─► core
topology ─► QtCore only
```

禁止的依赖包括：`topology -> graphics`、`document -> ui`、`graphics -> ui`、计算器直接读取画布。

## 闭路拓扑表示

入料汇合在拓扑层复用 `MergeNode` 的守恒语义，但与产品终端合流使用不同的画布图元。拓扑结构为：

```text
external feed ─┐
               ├─► MergeNode ─► flotation feed
recycle product┘
```

普通产品流或产品合并节点的 `MergeOutput` 连接到入料汇合节点的 `MergeInput`；新鲜入料是该节点的另一条外部输入，入料汇合节点的 `MergeOutput` 连接目标浮选单元的 `Feed` 端口。已接入回流的产品合并输出不再属于终端产品，断开后恢复。拓扑排序检测到有向环时将其标记为闭路警告，而不是结构错误。

`CanvasTopologySnapshot::requiredMeasurements` 描述使平衡方程唯一所需的实测物流。开路包含所有终端产品；普通回流额外包含回流产品；合并输出回流额外包含两条合并前支路。计算器反复应用浮选节点和汇流节点的质量、组分守恒关系，直到不再产生新值，因此在给定回流撕裂流数据后也能求解闭路。外部新鲜入料由入料汇合守恒反算。

## 生命周期规则

1. 当前方案终端数据变化：清除当前方案结果；拓扑变化：清除全部方案结果。
2. 几何变化：保留计算结果，只更新标注自动锚点。
3. 重新计算：更新标注内容，保留 `manualOffset`。
4. 指标设置变化：只重新格式化文字，不重新创建标注。
5. SVG、PNG、JPEG 导出完整流程图和当前可见标注；XLSX 按项目配置的稳定物流 ID 顺序导出全部方案的数据，没有配置的新物流按流程顺序追加。
6. 方案切换：保留共享拓扑与显示设置，替换药剂标注、实测数据和计算结果视图。

## 扩展入口

- 新画布对象：领域数据放 `core/document`，表现放 `graphics/items`。
- 新连接规则：修改 `FlowsheetScene` 并补充交互测试。
- 新计算指标：先扩展 `topology::CalculationResult`，再修改结果视图和格式化器。
- 新标注类型：扩展 `AnnotationKind`、结构化文档数据和对应 formatter。
- 新导出格式：放入 `services`，不得修改画布关系。
