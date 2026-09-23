# AutoFlotationSheet 架构说明

项目采用 C++20、Qt 6 Widgets 和 xmake。设计目标是把画布交互、业务数据、计算拓扑和视觉标注分离，避免在 `QGraphicsItem::paint()` 或主窗口中堆积领域逻辑。

新增代码的职责选择、允许依赖和关键入口详见 [`MODULE_BOUNDARIES.md`](MODULE_BOUNDARIES.md)。

## 模块目录

### `src/app`

应用装配层。`MainWindow` 创建场景、视图、停靠面板和工具栏，并连接各模块的信号。顶部动作按项目、流程、方案、显示和导出职责组织为下拉菜单。主窗口可以协调用户用例，但不实现计算公式、连接算法、结果表格或标注绘制。

主窗口实现按职责拆分：`MainWindow.cpp` 只保留窗口装配、信号连接与受控析构；`MainWindowCanvas.cpp` 负责创建单元、复制/粘贴、编号、尺寸调整和断开连接；`MainWindowPresentation.cpp` 负责结果计算命令、右侧结果同步、标注样式、主题和状态/日志提示；`MainWindowProject.cpp` 负责项目生命周期，`MainWindowScenario.cpp` 负责试验方案用例，`MainWindowExport.cpp` 负责流程图、Excel 导出和导出顺序。

最近项目列表由 `RecentProjectService` 通过 `QSettings` 持久化；主窗口只负责菜单呈现和项目切换，避免把历史记录规则混入项目序列化格式。

状态栏的选择提示使用永久 `QLabel`，由 `QGraphicsScene::selectionChanged` 驱动并根据 `UnitKind` 显示类型和稳定 ID；它与 `QStatusBar::showMessage()` 的临时操作消息相互独立。

### `src/core`

最基础的领域对象。目前包含 `FlotationUnit`。该目录不得依赖 `app`、`ui`、`editor` 或 `graphics`。

### `src/document`

项目文档状态：

- 项目共享的产品名称、结果标注记录、标注文字样式；
- 项目共享的 Excel 物流导出顺序（稳定物流 ID 列表）；
- 项目级 `ComponentDefinition`：稳定组分 ID 和显示名称；
- `ExperimentScenario`：单个试验方案的质量、按组分保存的品位/合流占比、药剂制度和最近计算结果；
- `ExperimentScenario::calculationMode`：方案级求解策略，严格模式与数据协调模式互不改变对方的数值语义；
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

- `FlowsheetScene` 管理连接、断开、合流、拆分和连通组件移动；`FlowsheetRoutingCoordinator` 负责几何路径与主题刷新，`CrossingBridgeRenderer` 仅负责绘制不改变拓扑的跨线桥；
- `CanvasActions` 对当前选择执行宽度、连接长度和断开操作；
- `ProjectUndoManager` 监听拓扑、几何和文档编辑信号，以 200 ms 合并窗口生成完整项目内存快照，并通过 `QUndoStack` 提供撤销/重做；恢复期间屏蔽新快照记录；
- `CanvasView` 管理视图级交互：以鼠标位置为中心进行 20%～400% 缩放、鼠标中键平移，以及方向键的优先分派。视图通过注入的处理器请求药剂微调，不识别具体标注类型。

关系变化发出 `topologyChanged`，位置或尺寸变化发出 `geometryChanged`。前者会使计算失效，后者只更新标注锚点。

删除单元由 `FlowsheetScene::removeUnit()` 统一解除直接连接、产品汇流和入料汇流引用。若被删除物流是入料汇流的正常主来源，汇流节点转为外部主入料并保留其他附加来源。场景删除完成后，`FlowsheetDocument::removeUnknownStreams()` 依据重建后的稳定物流 ID 集合清理所有方案中的孤立测量、产品名和物流标注。

### 正交布线与跨线桥

人工路线只保存可稳定重建路径的段参数：`ProductLineItem::manualRouteY`、`MergeJunctionItem::manualMergeY`，以及 `FeedJunctionItem` 按来源物流保存的 `manualRouteXs/manualRouteYs`。端点和其余折线由图元根据当前拓扑生成，避免移动流程后出现脱离端口的绝对坐标路径。项目格式 v7 保存回流支路汇入高度，并兼容读取仅含横向走廊的旧项目。

`FeedJunctionItem` 为每个来源保留独立路径、横向与纵向控制柄及当前选中来源。选中某一来源时仅强调该路径，弱化同一汇合点的其他路径及其他回流节点；命中检测基于实际路径轮廓，而不是只比较横坐标。公共入料段仍只绘制一次。

每条来源路径在最后一段水平走廊上绘制自己的方向箭头；箭头属于视图层，不参与命中、拓扑或物料计算，并与所属支路使用相同的高亮和淡化状态。

`FlowsheetScene::drawForeground` 从可见物流图元的正交线段计算水平—垂直交点，在水平线一侧绘制跨线桥。桥形属于纯视图层，不生成端口、节点或物料流，也不参与拓扑计算。

普通产品或产品合并输出连接到已占用入料时，`FlowsheetScene` 创建或扩展 `FeedJunctionItem`，且不自动移动目标单元。`FeedJunctionItem` 以集合保存任意数量的 `ProductLineItem` 和 `MergeJunctionItem` 附加来源；来源属于同一连通流程时构成回流。目标入料原先已经连接正常上游产品或合流输出时，汇合点会单独保存该端点。`CanvasActions` 优先读取 `selectedSourceStreamId()`：具体来源已选中时调用 `disconnectFeedSource()` 只移除该来源，否则才整体拆除汇流点并恢复原正常上游端点。

一个场景可以包含多个 `FeedJunctionItem`，用于表达独立、串联或嵌套回流环。连通分量遍历必须同时沿附加回流来源和汇合点保存的正常上游来源反向遍历，不能因直接入料线被汇合点替换而切断拓扑连通性。

### `src/adapters`

边界适配层。`CanvasTopologyBuilder` 把画布图元关系转换为纯计算 `TopologyGraph`，并生成报表物流（外部总入料及全部产品）、终端产品和必填实测物流的描述。只有该层允许同时了解画布图元和计算拓扑。

### `src/topology`

与界面完全独立的计算内核：

- `TopologyGraph`：节点、端口和物流；
- `TopologyValidator`：端口、环路、连通性和终端检查；
- `TopologyAlgorithms`：拓扑排序；
- `OpenCircuitCalculator`：将单个组分的质量与组分守恒、实测值和支路占比组装为广义线性方程组；
- `LinearSystemSolver`：只负责规范化增广矩阵、Gauss-Jordan 消元、秩/矛盾判断及部分唯一变量识别。它不知道浮选节点、物流或测量含义。
- `WeightedLeastSquaresSolver`：只负责由观测值、标准差和线性约束构造 KKT 系统，不了解物流含义。

二分流器在画布连接层沿用一入两出的稳定端口 ID，但计算拓扑在 `FlotationNode::leftSplitPercent` 中携带分流约束。求解器分别为干质量和每个组分质量加入左右支路比例方程，因此支路品位保持一致；二分流器不写入浮选单元性能结果。项目格式 v5 保存节点类型和比例，读取器继续兼容 v1–v4。

三产品单元通过 `PortKind::MiddleProduct` 和 `FlotationNode::hasMiddleProduct` 显式表达第三输出。`ThreeProductFlotation`、`ThreeProductScreening` 与 `ThreeProductDemediumScreen` 由 `hasMiddleProduct(UnitKind)` 统一映射为该结构；守恒方程、完整性校验、节点性能及结果详情均按三个产品处理。项目格式 v6 增加 `three-product-flotation`，v16 增加 `three-product-screening`，v19 增加 `three-product-demedium-screen`，v21 增加普通两产品的 `sedimentation-tank`，v23 增加 `shaking-table`，并兼容读取旧格式。

计算层使用 `flotationProductPorts()` 和 `TopologyGraph::flotationProductStreams()` 获得按左、中、右排列的实际输出集合。`FlotationPerformance::forPort()`/`setForPort()` 负责端口与性能指标映射，使求解器、校验器和结果界面共享同一产品集合定义。

该模块不得依赖 `QGraphicsItem` 或窗口控件。

### `src/services`

应用用例和无状态服务：

- `FlowsheetCalculationService`：读取文档实测值、构建画布拓扑并调用计算内核；
- `CalculationInputBuilder`：把方案测量、标准差、支路比例和边界关系转换为单组分纯拓扑输入；
- `ProjectSerializationData`：项目 JSON 与场景之间的中间数据结构；
- `ProjectSceneRestorer`：校验并从中间数据安全重建画布连接；
- `ProjectJsonReader`：只负责 JSON 版本兼容、字段范围与重复 ID 校验，并输出中间数据；
- `ProjectJsonWriter`：只负责把当前场景和文档写为稳定的项目 JSON；
- `ProjectSerializer`：薄编排层，负责文件大小限制、原子保存、隔离场景验证以及文档替换；
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

- `TerminalProductTableModel`：全部产品物流表模型；根据项目组分动态生成品位列，可编辑物流的输入项仅包含产品名称、绝对干质量和各组分品位；
- `TerminalProductDock`：面板装配、填写进度、自由度诊断和计算按钮状态；
- `TerminalProductViewSupport`：物流筛选规则以及数值、状态单元格委托；
- `ResultDetailsView`：物流和浮选单元结果表；
- `TerminalProductStyle`：面板主题样式生成；
- `ResultMetricMenu`：结果标注指标选择菜单；
- `OperationLogDock`：带时间戳和容量上限的只读操作记录面板，不依赖计算或画布业务。
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
   OpenCircuitCalculator（方程组装）
               │
               ▼
       LinearSystemSolver（消元）
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

报表层明确区分 `主入料`、`回流支路` 与 `总入料（含回流）`。正常上游产品即使与回流共用 `FeedJunctionItem`，仍依据 `processProduct/processMerge` 判为主入料，不能仅凭存在 `feedJunction` 判为回流。没有正常上游端点的节点还必须通过持久化的 `hasExternalFeed` 区分外部新鲜入料与纯多输入汇流，不能从来源数量或图形位置猜测。

`FeedJunctionItem` 分别提供主入料支路、回流支路、外部入料和总入料的指标锚点；`AnnotationManager` 必须先识别 `processProduct/processMerge`，避免主入料因共享汇流图元而丢失结果标注。

`CanvasStreamDescriptor` 保存终端、入料、回流类别以及相关对象 ID，`StreamFilterProxyModel` 据此组合“物流类别”和“关注对象”两层筛选而不依赖显示名称。求解结果同时携带干质量和组分质量的自由度，右侧提示栏使用两者较大值呈现当前仍缺少的独立约束数量。

计算拓扑中的汇流节点通过 `MergeRole` 明确区分产品合并（`ProductMerge`）和主入料/回流汇合（`FeedJunction`）。两者当前共享质量守恒方程，但语义标签保持独立，后续可分别扩展校验和交互规则。

“关注对象”只写入 `StreamFilterProxyModel` 的所有者筛选集合，不进入 `FlowsheetCalculationService`，也不使文档计算结果失效。求解服务始终接收 `CanvasTopologyBuilder` 生成的完整快照，组装全部节点守恒和全流程边界约束；欠定时由线性求解器保留所有局部唯一变量。

`OpenCircuitCalculator` 将每条物流的绝对干质量和组分质量分别作为未知量，把浮选节点守恒、汇流节点守恒、实测值、支路占比及显式边界约束组装为两个线性方程组，再交给 `LinearSystemSolver`。全流程“外部新鲜入料 = 最终产品之和”以 `LinearBalanceConstraint` 进入矩阵，因此即使新鲜入料已有实测值，求解器仍会检查它与终端产品是否一致。只有干质量和组分质量都唯一时，物流才进入结果集；若同一联立解出现负质量等物理非法值，将丢弃全部推导值并仅保留独立实测值，防止错误结果继续传播。

数据协调模式复用同一套拓扑守恒矩阵，但不把实测值加入精确等式。它以质量标准差和由质量/品位误差传播得到的组分质量标准差建立权重，通过 KKT 方程求解带线性守恒约束的加权最小二乘问题。结果同时记录协调值、残差、标准化残差及最大绝对标准化残差。严格模式仍走原来的精确方程路径，不能用协调容差掩盖矛盾。

`FlowsheetCalculationService` 的内部流程固定为：读取完整拓扑快照 → 为每个组分构造 `ComponentCalculationInput` → 调用计算内核 → 聚合 `CalculationResult::components`。项目导入后的重算也调用该服务，不允许在序列化器中复制计算输入组装逻辑。

## 生命周期规则

1. 当前方案任意物流实测数据变化：清除当前方案结果；拓扑变化：清除全部方案结果。
2. 几何变化：保留计算结果，只更新标注自动锚点。
3. 重新计算：更新标注内容，保留 `manualOffset`。
4. 欠定或部分组分冲突的结果仍可包含已唯一确定的物流；指标标注按物流结果存在性显示，不以全局 `complete` 作为开关。
5. 结果指标框按 `Delete` 只写入 `AnnotationManager` 的会话级隐藏集合，不修改 `AnnotationRecord::visible`；下一次 `calculationChanged` 清空集合并恢复结果框。
6. 指标设置变化：只重新格式化文字，不重新创建标注。
   质量单位保存在项目级 `AnnotationTextSettings::massUnit/customMassUnit`，支持预设和最多 16 字符的自定义标签，仅作为结果指标后缀，不缩放求解器中的绝对干质量。
7. SVG、PNG、JPEG 导出完整流程图和当前可见标注；XLSX 按项目配置的稳定物流 ID 顺序导出全部方案的数据，没有配置的新物流按流程顺序追加。
8. 方案切换：保留共享拓扑与显示设置，替换药剂标注、实测数据和计算结果视图。

## 扩展入口

- 新画布对象：领域数据放 `core/document`，表现放 `graphics/items`。
- 新连接规则：修改 `FlowsheetScene` 并补充交互测试。
- 新计算指标：先扩展 `topology::CalculationResult`，再修改结果视图和格式化器。
- 新标注类型：扩展 `AnnotationKind`、结构化文档数据和对应 formatter。
- 新导出格式：放入 `services`，不得修改画布关系。
