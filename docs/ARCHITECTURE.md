# AutoFlotationSheet 架构说明

项目采用 C++20、Qt 6 Widgets 和 xmake。设计目标是把画布交互、业务数据、计算拓扑和视觉标注分离，避免在 `QGraphicsItem::paint()` 或主窗口中堆积领域逻辑。

## 模块目录

### `src/app`

应用装配层。`MainWindow` 创建场景、视图、停靠面板和工具栏，并连接各模块的信号。顶部动作按项目、流程、方案、显示和导出职责组织为下拉菜单。主窗口可以协调用户用例，但不实现计算公式、连接算法、结果表格或标注绘制。

状态栏的选择提示使用永久 `QLabel`，由 `QGraphicsScene::selectionChanged` 驱动并根据 `UnitKind` 显示类型和稳定 ID；它与 `QStatusBar::showMessage()` 的临时操作消息相互独立。

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

- `FlotationUnitItem`：二产品浮选槽、三产品浮选槽或二分流器；领域对象通过 `UnitKind` 区分类型，三产品单元动态创建中间产品图元，二分流器保存左支路百分比；
- `InputLineItem`：入料线；
- `ProductLineItem`：左右产品线；
- `MergeJunctionItem`：两条产品合并为终端产品；
- `FeedJunctionItem`：任意数量附加产品/合流输出与正常入料的汇合及公共入料箭头；正常入料可以是外部新鲜入料、上游产品或合流输出。

产品物流使用稳定 ID：`<unit-id>:left`、`<unit-id>:middle`（仅三产品单元）、`<unit-id>:right`；外部入料使用 `<unit-id>:feed`；合流输出使用 `<merge-id>:output`。

`ProductLineItem::terminalLengthOverride` 保存单条产品线的可选终端长度；没有覆盖值时继续使用单元 `bodyHeight`。该参数同时决定未连接箭头、产品合流和回流来源的端点位置，但不改变浮选单元或其他产品线。

产品侧别的稳定后缀、中文标签和横向方向由 `ProductLineItem.h` 中的侧别辅助函数统一提供。画布适配器、连接几何和标注不得各自拼接 `:left/:middle/:right` 或复制侧别判断。

### `src/editor`

画布关系和编辑命令：

- `FlowsheetScene` 管理连接、断开、合流、拆分、连通组件移动和几何刷新；
- `CanvasActions` 对当前选择执行宽度、连接长度和断开操作；
- `CanvasView` 管理视图级交互：以鼠标位置为中心进行 20%～400% 缩放、鼠标中键平移，以及方向键的优先分派。视图通过注入的处理器请求药剂微调，不识别具体标注类型。

关系变化发出 `topologyChanged`，位置或尺寸变化发出 `geometryChanged`。前者会使计算失效，后者只更新标注锚点。

### 正交布线与跨线桥

人工路线只保存可稳定重建路径的段参数：`ProductLineItem::manualRouteY`、`MergeJunctionItem::manualMergeY`，以及 `FeedJunctionItem` 按来源物流保存的 `manualRouteXs/manualRouteYs`。端点和其余折线由图元根据当前拓扑生成，避免移动流程后出现脱离端口的绝对坐标路径。项目格式 v7 保存回流支路汇入高度，并兼容读取仅含横向走廊的旧项目。

`FeedJunctionItem` 为每个来源保留独立路径、横向与纵向控制柄及当前选中来源。选中某一来源时仅强调该路径，弱化同一汇合点的其他路径及其他回流节点；命中检测基于实际路径轮廓，而不是只比较横坐标。公共入料段仍只绘制一次。

每条来源路径在最后一段水平走廊上绘制自己的方向箭头；箭头属于视图层，不参与命中、拓扑或物料计算，并与所属支路使用相同的高亮和淡化状态。

`FlowsheetScene::drawForeground` 从可见物流图元的正交线段计算水平—垂直交点，在水平线一侧绘制跨线桥。桥形属于纯视图层，不生成端口、节点或物料流，也不参与拓扑计算。

普通产品或产品合并输出连接到已占用入料时，`FlowsheetScene` 创建或扩展 `FeedJunctionItem`，且不自动移动目标单元。`FeedJunctionItem` 以集合保存任意数量的 `ProductLineItem` 和 `MergeJunctionItem` 附加来源；来源属于同一连通流程时构成回流。目标入料原先已经连接正常上游产品或合流输出时，汇合点会单独保存该端点，整体断开后自动恢复。

一个场景可以包含多个 `FeedJunctionItem`，用于表达独立、串联或嵌套回流环。连通分量遍历必须同时沿附加回流来源和汇合点保存的正常上游来源反向遍历，不能因直接入料线被汇合点替换而切断拓扑连通性。

### `src/adapters`

边界适配层。`CanvasTopologyBuilder` 把画布图元关系转换为纯计算 `TopologyGraph`，并生成报表物流（外部总入料及全部产品）、终端产品和必填实测物流的描述。只有该层允许同时了解画布图元和计算拓扑。

### `src/topology`

与界面完全独立的计算内核：

- `TopologyGraph`：节点、端口和物流；
- `TopologyValidator`：端口、环路、连通性和终端检查；
- `TopologyAlgorithms`：拓扑排序；
- `OpenCircuitCalculator`：将单个组分的质量与组分守恒、实测值和支路占比组装为广义线性方程组并求解；`FlowsheetCalculationService` 对项目定义的每个组分分别调用计算器并聚合结果。

二分流器在画布连接层沿用一入两出的稳定端口 ID，但计算拓扑在 `FlotationNode::leftSplitPercent` 中携带分流约束。求解器分别为干质量和每个组分质量加入左右支路比例方程，因此支路品位保持一致；二分流器不写入浮选单元性能结果。项目格式 v5 保存节点类型和比例，读取器继续兼容 v1–v4。

三产品单元通过 `PortKind::MiddleProduct` 和 `FlotationNode::hasMiddleProduct` 显式表达第三输出。守恒方程、完整性校验、节点性能及结果详情均按三个产品处理。项目格式 v6 增加 `three-product-flotation` 单元类型，并兼容读取 v1–v5。

计算层使用 `flotationProductPorts()` 和 `TopologyGraph::flotationProductStreams()` 获得按左、中、右排列的实际输出集合。`FlotationPerformance::forPort()`/`setForPort()` 负责端口与性能指标映射，使求解器、校验器和结果界面共享同一产品集合定义。

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
- `TerminalProductDock`：面板装配、物流类别筛选、填写进度、自由度诊断和计算按钮状态；
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

`CanvasTopologySnapshot::requiredMeasurements` 保留一组传统的建议取样组合，供拓扑测试和后续取样建议功能使用，但不再限制右侧表格输入。表格允许编辑 `reportStreams` 中的全部物流，计算服务也从全部报表物流收集完整的实测值；是否足够由线性方程组的秩决定。

`CanvasStreamDescriptor` 保存终端、入料、回流类别以及相关对象 ID，`StreamFilterProxyModel` 据此组合“物流类别”和“关注对象”两层筛选而不依赖显示名称。求解结果同时携带干质量和组分质量的自由度，右侧提示栏使用两者较大值呈现当前仍缺少的独立约束数量。

关注对象非空时，`FlowsheetCalculationService` 构造诱导子图：保留选中节点及所有相邻物流，移除物流在未选中一侧的端点，使其成为局部边界。子图允许存在多个边界入料，但不会改变完整项目“仅一个主流程”的拓扑校验规则。

`OpenCircuitCalculator` 将每条物流的干质量和组分质量分别作为未知量，把浮选节点守恒、汇流节点守恒、实测值及支路占比组装为两个线性方程组。求解采用带绝对值选主元的 Gauss-Jordan 消元；行最简形用于识别矛盾方程、自由变量，以及整体欠定时仍可唯一确定的局部物流。只有干质量和组分质量都唯一时，物流才进入结果集。该方法不依赖节点遍历顺序，可以直接处理多个相互耦合的闭路；外部新鲜入料也由全网方程联立反算。

## 生命周期规则

1. 当前方案任意物流实测数据变化：清除当前方案结果；拓扑变化：清除全部方案结果。
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
