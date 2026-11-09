# AutoFlotationSheet 架构说明

项目采用 C++20、Qt 6 Widgets 和 xmake。本轮职责调整与验证范围见 [架构检查记录](ARCHITECTURE_REVIEW.md)。设计目标是把画布交互、业务数据、计算拓扑和视觉标注分离，避免在 `QGraphicsItem::paint()` 或主窗口中堆积领域逻辑。

新增代码的职责选择、允许依赖和关键入口详见 [`MODULE_BOUNDARIES.md`](MODULE_BOUNDARIES.md)。

## 模块目录

### `src/contracts`

`TerminalTableSession` 是 UI 与应用层之间的会话契约，提供只读 `TerminalTableState`、类型化 `TableCommand` 和带范围的变更通知。它不暴露文档或场景实例；应用层实现该契约，UI 依赖契约而不依赖应用实现。

### `src/app`

应用装配层。`MainWindow` 创建场景、视图、停靠面板和工具栏，并连接各模块的信号。顶部动作按项目、流程、方案、显示和导出职责组织为下拉菜单。主窗口可以协调用户用例，但不实现计算公式、连接算法、结果表格或标注绘制。

主窗口实现按职责拆分：`MainWindow.cpp` 只保留窗口装配、信号连接与受控析构；`MainWindowActions.cpp` 只编排工具栏及项目/方案/导出入口，流程菜单和快捷键注册在 `MainWindowFlowActions.cpp`，设备添加菜单在 `MainWindowEquipmentActions.cpp`，显示设置在 `MainWindowDisplayActions.cpp`；流程菜单返回撤销/重做动作供窗口连接状态。`MainWindowCanvas.cpp` 负责创建单元、流程组复制/粘贴、编号、尺寸调整和断开连接；`MainWindowClipboard.cpp` 分派画布复制粘贴并管理当前粘贴类型，`MainWindowReagents.cpp` 协调单条药剂的复制、目标校验及独立撤销边界。`MainWindowResults.cpp` 负责计算命令及表格/画布结果同步，`MainWindowPresentation.cpp` 负责主题、状态/日志和样式编辑命令；`MainWindowProject.cpp` 负责项目生命周期，`MainWindowScenario.cpp` 负责试验方案用例，`MainWindowExport.cpp` 负责流程图、Excel 导出和导出顺序。

`AnnotationTextStyleDialog` 和 `StreamOrderDialog` 位于 `ui`，只编辑传入值并返回可选结果；主窗口负责确认后的文档提交。排序窗口通过值回调取得恢复顺序，不持有文档或画布指针。

工作区由 `MainWindowTools.cpp` 装配：画布保持中央视图，唯一的右侧停靠容器 `WorkspaceSidebar` 通过互斥动作和 `QStackedWidget` 在上方的表格、对象属性、计算问题、模板库间切换，默认显示表格。`MainWindowActions.cpp` 在“显示设置”右侧装配 `WorkspacePanelSelector`；下拉框从 `WorkspaceSidebar::panelKeys()` 读取页面及标签，与显示菜单共用页面注册表，并通过 `showPanel()` 和 `panelChanged` 双向同步。鼠标悬停滚轮无需先聚焦，按上下方向切换并在首尾停止；高分辨率滚动累积不足一格的角度，方向反转时重置余量。切换复用已有页面，不重建控件或修改项目；容器在内容区尺寸变化时，将底部固定日志区高度设为总高度的八分之一，上方页面堆栈占其余空间，避免子控件的最小尺寸提示挤占比例，右侧容器顶部不设置切换按钮栏；日志不参与页面切换。`MainWindowDiagnostics.cpp` 协调问题列表与画布/表格定位，`MainWindowTemplates.cpp` 协调模板保存、预览和插入；属性、问题、模板面板只持有显示值；主窗口显式协调跳转：计算后仅遇到错误级 `InconsistentBalance` 才自动显示问题面板，欠定和协调残差警告保留当前页面；手动“检查流程 / 计算问题”始终显示问题面板。问题单击保留列表，双击才切换到表格编辑。关闭主窗口时先断开并销毁面板和右侧容器，再销毁文档和场景。

最近项目列表由 `RecentProjectService` 通过 `QSettings` 持久化；主窗口只负责菜单呈现和项目切换，避免把历史记录规则混入项目序列化格式。

结果来源边框的区分开关与线宽同样是本机偏好，由 `AnnotationManager` 通过 `QSettings` 保存。项目中的指标可见性、质量单位和文字样式仍由文档管理；本机偏好不进入项目格式或撤销快照。

状态栏的选择提示使用永久 `QLabel`，由 `QGraphicsScene::selectionChanged` 驱动并根据 `UnitKind` 显示类型和稳定 ID；它与 `QStatusBar::showMessage()` 的临时操作消息相互独立。

### `src/core`

最基础的领域对象。目前包含 `FlotationUnit`。该目录不得依赖 `app`、`ui`、`editor` 或 `graphics`。

### `src/document`

项目文档状态：

- 项目共享的产品名称、结果标注记录、标注文字样式；
- 项目共享的 Excel 物流导出顺序（稳定物流 ID 列表）；
- 项目级 `ComponentDefinition`：稳定组分 ID 和显示名称；
- `ExperimentScenario`：单个试验方案的质量、按组分保存的品位/合流占比、药剂制度、自定义文字记录和最近计算结果；
- `ExperimentScenario::calculationMode`：方案级求解策略，严格模式与数据协调模式互不改变对方的数值语义；
- 当前方案及方案增删、复制、重命名、切换状态。

修改质量或品位时只使当前方案结果失效；修改画布拓扑时使全部方案结果失效。图元不得自行保存另一份实验数据。

实现按状态职责拆分：`FlowsheetDocument.cpp` 管理项目共享元数据、项目替换与孤立物流清理；`FlowsheetDocumentAnnotations.cpp` 管理共享显示设置以及当前方案文字/药剂和共享结果标注的读写；`FlowsheetDocumentMeasurements.cpp` 管理当前方案的测量、组分定义、计算模式与结果失效；`FlowsheetDocumentScenarios.cpp` 管理方案访问、增删、切换和批量替换。共同声明仍位于 `FlowsheetDocument.h`。

### `src/graphics`

画布图元及局部鼠标交互：

- `FlotationUnitItem`：几何与局部交互放在 `FlotationUnitItem.cpp`，`FlotationUnitItemPainting.cpp` 只选择选中态颜色并委托 `graphics/symbols/UnitSymbolPainter` 绘制；符号分派、贮池/球磨机/浓密池与分选设备分别由 `UnitSymbolPainter.cpp`、`VesselSymbols.cpp` 和 `SeparationSymbols.cpp` 负责；领域对象通过 `UnitKind` 区分浮选、分选、贮池、球磨机等类型，三产品单元动态创建中间产品图元，二分流器保存左支路百分比；
- `InputLineItem`：入料线；
- `ProductLineItem`：左、中、右产品线及球磨机的底部出料线；
- `MergeJunctionItem`：两条或多条产品合并，合流输出可终止、连接下游或回流；
- `FeedJunctionItem`：任意数量附加产品/合流输出与正常入料的汇合及公共入料箭头；正常入料可以是外部新鲜入料、上游产品或合流输出。

`FeedJunctionItem.cpp` 保留来源集合、选择状态和绘制，`FeedJunctionItemRouting.cpp` 负责路径、锚点与手动路线参数，`FeedJunctionItemInteraction.cpp` 负责鼠标和键盘事件。`ProductLineItem.cpp` 保留产品线几何、显示和状态，拖拽连接、路线编辑及键盘事件放在 `ProductLineItemInteraction.cpp`。`MergeJunctionItem.cpp` 管理合流来源/目标状态及绘制，`MergeJunctionItemRouting.cpp` 管理路径与手动汇合高度，`MergeJunctionItemInteraction.cpp` 管理鼠标和键盘事件。

`FeedJunctionItemHitTest.cpp` 和 `MergeJunctionItemHitTest.cpp` 提供位置到唯一物流 ID 的查询，公共线段或交点命中多个物流时返回空。两类汇流图元另以 `selectedAnnotationStreamId()` 记录鼠标选中的标注目标，取消选择即清空；该状态不进入项目或拓扑。回流的 `selectedSourceStreamId()` 继续服务于路线高亮与断开来源，不能用它替代主入料、外部入料和合流输出的标注目标。

产品线和产品合流的命中范围由线条轮廓及实际箭头组成。开放折线不能整体加入填充区域，否则会把折线间的空白变成可选区域，或抵消部分可见线段的命中范围。

产品物流使用稳定 ID：`<unit-id>:left`、`<unit-id>:middle`（仅三产品单元）、`<unit-id>:right`；外部入料使用 `<unit-id>:feed`；合流输出使用 `<merge-id>:output`。

`ProductLineItem::terminalLengthOverride` 保存单条产品线的可选终端长度；没有覆盖值时继续使用单元 `bodyHeight`。该参数同时决定未连接箭头、产品合流和回流来源的端点位置，但不改变浮选单元或其他产品线。

产品侧别的稳定后缀、中文标签和横向方向由 `ProductLineItem.h` 中的侧别辅助函数统一提供。画布适配器、连接几何和标注不得各自拼接 `:left/:middle/:right` 或复制侧别判断。

球磨机绘制为圆形，入料锚点位于圆顶，出料锚点位于圆底正中。单一出料保留 `<unit-id>:right` 稳定 ID，但几何位置不再由右侧边沿推导；所有连接、汇流、回流及标注均读取实际锚点。设备图标下方不绘制名称或说明小字，贮池内部标签与用户填写的产品名称分别保留。

### `src/editor`

画布关系和编辑命令：

- `FlowsheetScene.cpp` 负责场景事件、命中与刷新入口；`FlowsheetSceneConnections.cpp` 管理连接、断开、合流、拆分和删除，`FlowsheetSceneMovement.cpp` 负责连通组件移动、宽度与长度调整；`FlowsheetRoutingCoordinator` 负责几何路径与主题刷新，`CrossingBridgeRenderer` 仅负责绘制不改变拓扑的跨线桥；
- `CanvasActions` 对当前选择执行宽度、连接长度和断开操作；
- `CanvasView` 管理视图级交互：以鼠标位置为中心进行 20%～400% 缩放、中键或 `Space` 拖动平移、`Space` 配合方向键平移、`Home` 适应内容，以及方向键的优先分派。视图通过注入的处理器请求药剂微调，不识别具体标注类型。

关系变化发出 `topologyChanged`，位置或尺寸变化发出 `geometryChanged`。前者会使计算失效，后者只更新标注锚点。

删除单元由 `FlowsheetScene::removeUnit()` 统一解除直接连接、产品汇流和入料汇流引用。若被删除物流是入料汇流的正常主来源，汇流节点转为外部主入料并保留其他附加来源。场景删除完成后，`FlowsheetDocument::removeUnknownStreams()` 依据重建后的稳定物流 ID 集合清理所有方案中的孤立测量、产品名和物流标注。

### 正交布线与跨线桥

人工路线只保存可稳定重建路径的段参数：`ProductLineItem::manualRouteY`、`MergeJunctionItem::manualMergeY`，以及 `FeedJunctionItem` 按来源物流保存的 `manualRouteXs/manualRouteYs`。端点和其余折线由图元根据当前拓扑生成，避免移动流程后出现脱离端口的绝对坐标路径。项目保存回流支路汇入高度和横向走廊；未设置的手动路线仍为可选字段。

`FeedJunctionItem` 为每个来源保留独立路径、横向与纵向控制柄及当前选中来源。选中某一来源时仅强调该路径，弱化同一汇合点的其他路径及其他回流节点；命中检测基于实际路径轮廓，而不是只比较横坐标。公共入料段仍只绘制一次。

每条来源路径在最后一段水平走廊上绘制自己的方向箭头，与所属支路使用相同的高亮和淡化状态；命中查询可通过箭头识别所属物流，但箭头不生成拓扑端口或物料约束。

`FlowsheetScene::drawForeground` 从可见物流图元的正交线段计算水平—垂直交点，在水平线一侧绘制跨线桥。桥形属于纯视图层，不生成端口、节点或物料流，也不参与拓扑计算。

同一个入料汇流图元内的不同支路也参与交点检测；不能按图元相同而跳过。只有线段内部交叉绘制拱桥，线段端点处的转角和实际汇合保持连接样式。

拱桥使用两段三次曲线，以水平切线与原水平段连续衔接；遮罩只清除拱桥替换的水平部分，保留中心竖线，避免被跨越的线路出现断口。

普通产品或产品合并输出连接到已占用入料时，`FlowsheetScene` 创建或扩展 `FeedJunctionItem`，且不自动移动目标单元。`FeedJunctionItem` 以集合保存任意数量的 `ProductLineItem` 和 `MergeJunctionItem` 附加来源；来源属于同一连通流程时构成回流。目标入料原先已经连接正常上游产品或合流输出时，汇合点会单独保存该端点。`CanvasActions` 优先读取 `selectedSourceStreamId()`：具体来源已选中时调用 `disconnectFeedSource()` 只移除该来源，否则才整体拆除汇流点并恢复原正常上游端点。

一个场景可以包含多个 `FeedJunctionItem`，用于表达独立、串联或嵌套回流环。连通分量遍历必须同时沿附加回流来源和汇合点保存的正常上游来源反向遍历，不能因直接入料线被汇合点替换而切断拓扑连通性。

### `src/adapters`

边界适配层。`CanvasTopologyBuilder` 把画布图元关系转换为纯计算 `TopologyGraph`，并生成报表物流（外部总入料及全部产品）与终端产品描述。该层是画布关系转换为计算拓扑的统一入口，不再构建已删除 UI 使用的对象目录或固定取样清单。

### `src/topology`

与界面完全独立的计算内核：

- `TopologyGraph`：节点、端口和物流；
- `TopologyValidator`：端口、环路、连通性和终端检查；
- `TopologyAlgorithms`：拓扑排序；
- `OpenCircuitCalculator`：校验单组分计算输入，调用标量方程求解，检查物理解并汇总物流值、残差和性能指标；
- `BalanceEquationSolver`：位于内部 `calculation_detail` 命名空间，组装干质量或组分质量的守恒、分流、合流占比及测量方程，委托严格消元或加权最小二乘求解；
- `LinearSystemSolver`：只负责规范化增广矩阵、Gauss-Jordan 消元、秩/矛盾判断及部分唯一变量识别。它不知道浮选节点、物流或测量含义。
- `WeightedLeastSquaresSolver`：只负责由观测值、标准差和线性约束构造 KKT 系统，不了解物流含义。

二分流器在画布连接层沿用一入两出的稳定端口 ID，但计算拓扑在 `FlotationNode::leftSplitPercent` 中携带分流约束。求解器分别为干质量和每个组分质量加入左右支路比例方程，因此支路品位保持一致；二分流器不写入浮选单元性能结果。项目显式保存节点类型和分流比例。

三产品单元通过 `PortKind::MiddleProduct` 和 `FlotationNode::hasMiddleProduct` 显式表达第三输出。`ThreeProductFlotation`、`ThreeProductScreening` 与 `ThreeProductDemediumScreen` 由 `hasMiddleProduct(UnitKind)` 统一映射为该结构；守恒方程、完整性校验、节点性能及结果导出均按三个产品处理。项目通过 `three-product-flotation`、`three-product-screening`、`three-product-demedium-screen`、`sedimentation-tank` 和 `shaking-table` 显式保存这些单元类型。

计算层使用 `flotationProductPorts()` 和 `TopologyGraph::flotationProductStreams()` 获得按左、中、右排列的实际输出集合。`FlotationPerformance::forPort()`/`setForPort()` 负责端口与性能指标映射，使求解器、校验器和结果界面共享同一产品集合定义。

球磨机经 `isNonSeparatingUnit()` 复用非终端 `StoragePoolNode` 的一入一出守恒结构，使用 `PoolOutput`；该复用只表达干质量和各组分质量保持一致，不把球磨机当作二产品分选器，也不引入新的磨矿方程。

浓密池 `UnitKind::Thickener` 同样复用一入一出守恒节点，项目类型键为 `thickener`。池体以椭圆池口、刮耙和锥底表示，几何及选择形状统一在 `ThickenerGeometry.h` 中；进出料锚点分别位于顶部和底部中心，出料使用 `<unit-id>:right`。宽度调整不横移下游设备，所有连接仍读取实际锚点。

该模块不得依赖 `QGraphicsItem` 或窗口控件。

### `src/services`

应用用例、流程快照和文件服务：

- `FlowsheetCalculationService`：读取文档实测值、构建画布拓扑并调用计算内核；
- `CalculationInputBuilder`：把方案测量、标准差、支路比例和边界关系转换为单组分纯拓扑输入；
- `FlowGroupClipboard`：管理独立 `FlowGroupSnapshot` 和连续粘贴次数；`FlowGroupSnapshotCapture` 从选中图元与当前方案采集值快照，`FlowGroupSnapshotPaste` 只读快照、分配新 ID 并重建副本；文字使用新的标注 ID，与单元共享粘贴偏移；药剂重映射到副本物流并保留相对偏移和堆叠顺序。单独粘贴文字只修改文档标注，不发出拓扑变更；
- `ReagentClipboard`：保存单条药剂的值快照，生成新的标注 ID 并绑定目标物流，清除源手动偏移；不持有图元或文档指针，不操作系统剪贴板；
- `ProjectUndoManager`：监听拓扑、几何和文档编辑信号，以 200 ms 合并窗口生成完整项目内存快照，通过 `QUndoStack` 提供撤销/重做；离散命令可用 `commitPending()` 提交前后边界，保证每次药剂粘贴独立撤销；恢复期间屏蔽新快照记录；
- `ProjectSerializationData`：项目 JSON 与场景之间的中间数据结构；
- `ProjectSceneRestorer`：校验并从中间数据安全重建画布连接；
- `ProjectJsonReader`：接受 v23 和 v24，读取入口将 v23 共享文字转换为各方案独立文字，再统一进行 v24 结构校验；负责 JSON 解析、根结构检查和分段读取编排；`ProjectGeometryReader` 校验单元与连接，`ProjectDisplaySettingsReader` 校验组分定义、显示设置、产品名称及共享标注，`ProjectScenarioReader` 校验试验方案、实测值、药剂与自定义文字；共用的数值/字符串读取在 `ProjectJsonReadSupport` 中，所有分段读取只输出中间数据；
- `ProjectJsonWriter`：只负责把当前场景和文档写为稳定的项目 JSON；版本号统一定义在 `ProjectFormat.h`，设备类型键由 `ProjectUnitKindCodec` 的单一映射表双向转换，实测数据仅写入 `scenarios[].measurements`，药剂仅写入 `scenarios[].reagents`，文字仅写入 `scenarios[].notes`，保存版本始终为 v24；
- `ProjectSerializer`：薄编排层，负责文件大小限制、原子保存、隔离场景验证以及文档替换；
- `ThemeService`：应用及保存明暗主题；
- `SvgExporter`：将当前场景导出为 SVG 矢量图；
- `RasterExporter`：将当前场景导出为 PNG 或 JPEG，并通过最长边及总像素限制控制内存占用。
- `ExcelDataExporter`：仅将用户已命名物流在各试验方案中的计算结果写入标准 XLSX 工作簿，命名筛选与导出排序列表复用 `namedStreams()`；`ExcelWorksheetBuilder` 将有序结果组织成工作表 XML，`XlsxWorkbookWriter` 只处理工作簿部件、样式和 ZIP 打包，不读取流程或文档。

流程图导出服务使用相同的图元包围范围和留白，并统一绘制白色背景；Excel 导出读取文档方案和拓扑快照。所有导出服务均不修改图元关系或文档数据。

产品名称属于项目共享数据，默认为空。Excel 仅输出已命名且在对应方案中具有计算值的物流；无命名物流或无可用结果时在写入文件前返回提示。右侧表格的定位描述和显示筛选均不决定 Excel 输出名称或导出范围。

### `src/annotations`

通用画布标注系统：

- `AnnotationTypes`：标注类型、样式、所有者和结果指标设置；
- `AnnotationItem`：选择、拖动、绘制和主题表现；
- `ReagentAnnotationItem`：药剂小叉、化学式上下标、文本表现和相对锚点偏移；
- `AnnotationContentFormatter`：业务数据到显示文字；
- `AnnotationManager.cpp`：结果卡同步、锚点查找与图元生命周期；
- `AnnotationManagerContent.cpp`：自定义文字与药剂图元的创建、更新、移除和堆叠；
- `AnnotationManagerDisplay.cpp`：项目显示设置、本机来源边框偏好、文字样式和外观刷新；
- `AnnotationManagerInteraction.cpp`：场景事件分派、删除及药剂方向键微调；`AnnotationManagerContextMenu.cpp` 构建右键菜单，`AnnotationContextTarget` 仅按鼠标位置识别标注和唯一物流；汇流图元的 `*HitTest.cpp` 识别实际支路，不修改布线路径。
- `AnnotationManagerReagentPlacement.cpp`：按目标线已有标签的实际边界追加摆放并选中新标注；
- `ReagentOrder.h`：统一普通药剂和单条粘贴副本的顺序，供图元堆叠、流程组复制和模板快照共用。

药剂标注和自定义文字按当前试验方案存放，复制方案时一并复制，之后独立编辑；自定义文字的内容、样式和自由场景坐标保存在 `ExperimentScenario::noteAnnotations`，对应 `scenarios[].notes`。药剂包含名称、用量、单位、备注和垂直偏移；结果标注位置属于项目共享数据。三类标注由 `AnnotationManager` 统一负责生命周期和锚点更新。药剂方向键微调采用 `CanvasView → AnnotationManager → ReagentAnnotationItem` 单一路径，避免与画布滚动重复处理。

结果来源区分只影响边框：当前可见质量或任一组分品位为计算值时加粗，不给指标追加来源文字。产率和回收率不参与判断；线宽及来源开关不修改计算或文档数据。具体规则和复制行为见 [标注系统](ANNOTATIONS.md)。

### `src/ui`

非画布界面组件：

- `WorkspacePanelSelector`：顶部的面板下拉框及悬停滚轮交互，只请求既有页面切换，不操作项目或新增快捷键；
- `TerminalProductTableModel`：报表物流表模型；根据项目组分动态生成品位列，编辑产品名称、绝对干质量和各组分品位，并在协调模式下提供标准差列。`NameColumn` 显示定位信息，`ProductNameColumn` 只显示用户填写的名称，未填写时为空。`TerminalProductTableModel.cpp` 管理列映射、会话快照呈现、编辑命令映射和模型通知，`TerminalProductTableModelData.cpp` 负责单元格角色、工具提示及表头呈现；
- `TerminalProductDock.cpp`：对象初始化、跨控件信号连接、表格键盘事件和主题刷新；`TerminalProductDockLayout.cpp` 只装配顶部摘要卡、筛选/操作行及主表，保留标题/统计、筛选/操作、计算状态三行布局；`TerminalProductDockComponents.cpp` 处理组分名称编辑；`TerminalProductDockColumns.cpp` 管理列与委托、初始列宽及手动调整保护；`TerminalProductDockSelection.cpp` 管理快照与选择联动；`TerminalProductDockResults.cpp` 只呈现进度与应用层给出的计算状态；
- `CalculationExplanationDialog`：只接收计算结果与计算模式，显示只读弹窗；不读取文档、不触发计算或修改项目。表格面板不保留物流结果小表格，主表占满剩余空间；
- `TerminalProductViewSupport`：物流筛选规则以及数值、状态单元格委托；
- `TerminalProductStyle`：面板主题样式生成，输入表使用随主题变化的网格线；
- `ResultMetricMenu`：直接继承 `QMenu` 的结果标注指标菜单，嵌入“显示设置”，不创建隐藏的工具按钮；
- `OperationLogDock`：带时间戳和容量上限的只读操作记录面板，不依赖计算或画布业务。
- `ScenarioComparisonDialog`：按最终或中间产品比较各方案已计算结果，并显示用户填写的产品名称，不修改文档状态。

## 主要数据流

```text
QGraphicsItem 关系
      │
      ▼
CanvasTopologyBuilder ──► TopologyGraph
      │                       │
      │ full snapshot         ▼
      └──────────────► FlowsheetCalculationService
                               │
FlowsheetDocument measurements ┘
               │
               ▼
   OpenCircuitCalculator（校验与编排）
               │
               ▼
   BalanceEquationSolver（方程组装）
               │
               ▼
   LinearSystemSolver（严格消元）
       或 WeightedLeastSquaresSolver（数据协调）
               │
               ▼
        CalculationResult（全部或部分可解结果、诊断）
          │            │
          ▼            ▼
状态与诊断面板    AnnotationManager
```

`AnnotationItem.cpp` 负责文字排版、绘制和图元事件；`AnnotationItemEditor.cpp` 实现自由文字编辑弹窗，保留同一图元类和 MOC 头，返回记录值，由调用方提交到当前方案。

## 依赖方向

```text
app ─► ui / editor / services / annotations
ui ─► document / topology / editor（视图复用与属性值类型）
services ─► adapters / document / topology
annotations ─► document / editor / graphics / topology
adapters ─► editor / graphics / topology
editor ─► graphics
graphics ─► core / editor（场景编辑通知）
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

表格允许编辑 `reportStreams` 中的全部物流，计算服务从同一报表物流集合收集完整实测值；是否足够由线性方程组的秩决定。快照不保存另一份固定必填或建议取样列表。

报表层明确区分 `主入料`、`回流支路` 与 `总入料（含回流）`。正常上游产品即使与回流共用 `FeedJunctionItem`，仍依据 `processProduct/processMerge` 判为主入料，不能仅凭存在 `feedJunction` 判为回流。没有正常上游端点的节点还必须通过持久化的 `hasExternalFeed` 区分外部新鲜入料与纯多输入汇流，不能从来源数量或图形位置猜测。

`FeedJunctionItem` 分别提供主入料支路、回流支路、外部入料和总入料的指标锚点；`AnnotationManager` 必须先识别 `processProduct/processMerge`，避免主入料因共享汇流图元而丢失结果标注。

`StreamDescriptor` 保存终端、入料、回流类别以及相关对象 ID，`StreamFilterProxyModel` 据此处理“物流类别”和临时的“选中流程关联”筛选而不依赖显示名称。后者按全部所选单元的直接 `ownerIds` 匹配并取并集，不递归展开闭路；它不保存到项目文件。表格导航期间，`MainWindow` 通过受控的选择来源标记保留当前关联范围，避免画布清空选择时重入并修改正在处理鼠标事件的代理模型；实际画布选择变化则重新建立关联范围。多个报表行共享同一个汇流图元时，面板通过稳定物流 ID 保留当前行和编辑列。求解结果同时携带干质量和组分质量的自由度，右侧提示栏使用两者较大值呈现当前仍缺少的独立约束数量。

计算拓扑中的汇流节点通过 `MergeRole` 明确区分产品合并（`ProductMerge`）和主入料/回流汇合（`FeedJunction`）。两者当前共享质量守恒方程，但语义标签保持独立，后续可分别扩展校验和交互规则。

显示筛选只影响 `StreamFilterProxyModel`，不进入 `FlowsheetCalculationService`，也不使文档计算结果失效。求解服务始终接收 `CanvasTopologyBuilder` 生成的完整快照，组装全部节点守恒和全流程边界约束；欠定时由线性求解器保留所有局部唯一变量。

`OpenCircuitCalculator` 把干质量与组分质量的标量求解委托给 `BalanceEquationSolver`，后者将每条物流的值作为未知量，把浮选节点守恒、汇流节点守恒、实测值、支路占比及显式边界约束组装为两个线性方程组，再交给严格消元或加权最小二乘求解器。全流程“外部新鲜入料 = 最终产品之和”以 `LinearBalanceConstraint` 进入矩阵，因此即使新鲜入料已有实测值，求解器仍会检查它与终端产品是否一致。只有干质量和组分质量都唯一时，物流才进入结果集；若同一联立解出现负质量等物理非法值，将丢弃全部推导值并仅保留独立实测值，防止错误结果继续传播。

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
7. SVG、PNG、JPEG 导出完整流程图和当前可见标注；XLSX 仅导出各方案中已命名且有计算结果的物流，按项目配置的稳定物流 ID 顺序排列，未配置顺序的命名物流按流程顺序追加。
8. 方案切换：保留共享拓扑与显示设置，替换自定义文字、药剂标注、实测数据和计算结果视图。
9. 复制内容、当前快捷键粘贴类型和汇流标注目标属于窗口会话状态，不写入项目或撤销快照；药剂粘贴只修改当前方案的标注，不使计算结果失效。

## 扩展入口

- 新画布对象：领域数据放 `core/document`，表现放 `graphics/items`。
- 新连接规则：修改 `FlowsheetScene` 并补充交互测试。
- 新计算指标：先扩展 `topology::CalculationResult`，再修改结果视图和格式化器。
- 新标注类型：扩展 `AnnotationKind`、结构化文档数据和对应 formatter。
- 新导出格式：放入 `services`，不得修改画布关系。

## 工作区属性、诊断与模板

- `CanvasProperties` 在编辑层描述当前单选对象的数值属性，校验对象标识与数值范围，委托既有图元和场景接口修改几何；`ObjectPropertiesDock` 只构建控件、发出编辑请求，以代次标识忽略旧控件的延迟信号。
- `CalculationDiagnostics` 把当前结果转换成问题显示值，保留物流及组分定位信息；节点标题由现有计算图的节点及汇流角色解析，不依赖旧 UI 对象目录；全局约束只提供检查入口，不修改求解关系。`CalculationIssuesDock` 显示列表，主窗口结合拓扑快照定位实际图元，`TerminalProductDock::focusMeasurement()` 揭示被筛选的目标并聚焦输入列。
- `FlowTemplateStore` 在本机应用数据目录的 `flow-templates` 中以 UUID 文件保存模板。模板容器版本为 1，内嵌项目由当前 `ProjectSerializer` 读写；不另行复制项目字段编码，不迁移旧项目格式。以首个设备为原点规范化坐标，保留路线相对结构。
- 模板通过 `flow_group::capture/paste` 复用选择快照与新 ID 分配。只保存设备、拓扑、几何、名称、文字和当前方案药剂，不包含测量和计算结果；原子写入使用 `QSaveFile`，加载失败只影响临时场景。

模板预览由 `app/FlowTemplatePreviewSession` 拥有独立文档、`FlowsheetScene` 和 `AnnotationManager`，通过只读快照重建，并复用设备、跨线桥和标注绘制。`ui/FlowTemplatePreview` 仅管理视图与只读交互，复用 `CanvasView` 的缩放平移；窗口级编辑快捷键在预览聚焦时被拦截。模板列表和预览用纵向分隔条布局，默认自动适应可见内容，手动缩放或中键平移后保留视图，点击适应窗口恢复自动适应。主窗口切换模板时先将视图与旧场景分离，再销毁旧会话；关闭时先销毁视图，再清理预览标注、场景和文档。预览不持有当前项目图元，不使用主画布的 ID、剪贴板或撤销栈，也不向模板文件写入缩略图。

## 计算与画布的值边界

`document/FlowsheetSnapshot.h` 包含计算图与物流目录，`StreamDescriptor` 只有稳定 ID、定位描述、类别和关联对象 ID，不能包含 `QGraphicsItem*`。求解、诊断和 Excel 读取这一纯值契约；即使原画布销毁，快照仍能独立计算。`FlowsheetCalculationService` 不再提供场景参数重载，也不依赖 adapters/editor/graphics。

`CanvasTopologyBuilder::buildItemIndex()` 独立构建非拥有型定位索引。应用层在表格请求定位、画布选择同步及问题导航时按需取得当前映射；索引不跨删除、导入或撤销恢复缓存。表格发送 `streamRequested(streamId)`，通过 `selectStreams(streamIds)` 保留共享汇流图元对应的具体行及编辑列，UI 模型不接收图元。

`app/CalculationController` 持有当前纯快照和会话预览缓存。测量、组分、计算模式、方案、已存结果或拓扑变化使缓存失效；零延时单次定时器合并同一轮变更。筛选、选择、命名和主题不触发预览。表格只接收 `CalculationPanelState`；点击计算由主窗口协调显式求解并提交文档，计算说明复用已存结果或缓存预览。预览不写项目、不进入撤销、不改变已存结果；控制器在文档之前销毁并取消待执行刷新。

## 表格会话与同步图元请求

`app/TerminalTableController` 查询文档并发布只读状态，在提交测量、名称、组分和模式命令前校验有效物流、编辑权限、组分、数值范围与模式。不在当前物流目录中的目标被拒绝，目录切换通过模型重置取消旧编辑。表格和表格模型不持有 `FlowsheetDocument`；组件 ID 的保留/生成及输入规范化在应用层完成，界面只采集用户输入。

模型原子更新状态快照：仅行集合、编辑范围或列结构变化时重置；普通测量、名称和结果变更保留行选择及列宽。名称通知只刷新产品名称列，避免影响其它正在编辑的字段。计算预览仍由 `CalculationController` 管理，表格会话不额外求解。

`graphics/CanvasInteraction.h` 定义 `CanvasGeometryHost` 和 `CanvasConnectionHost`；`FlowsheetScene` 实现两者。图元只根据当前所属的 Qt 场景查询契约，请求几何联动、连接或路线变更通知，不引用、转换到具体的 `FlowsheetScene`，不保存回调。请求保持同步，尤其在 `ItemPositionChange` 中的组移动不能合并为排队信号。图元转移到普通场景后不再调用原场景；场景析构期间查询不到契约时安全退化为本地图形更新。
