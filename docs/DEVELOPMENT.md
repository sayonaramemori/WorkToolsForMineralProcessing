# 开发指南

## 维护入口

先阅读 [模块职责与依赖边界](MODULE_BOUNDARIES.md) 决定代码归属，再查 [架构说明](ARCHITECTURE.md) 的实现入口。业务状态归文档、求解归 topology、画布到计算图归 adapters、跨模块用例归 app。

配置好 Qt/xmake 后运行：

```bash
python3 scripts/validate.py --clean
```

该命令执行全目标干净构建、从 `xmake.lua` 发现的全部 17 个检查，以及 v23、v24 项目启动/退出验证。日志和退出码位于 `build/validation/latest/`，任一检查失败时脚本以非零状态退出。已经构建且源码未变化时可使用 `--skip-build`；xmake 未加入 PATH 时传入 `--xmake /path/to/xmake`。默认使用 Qt offscreen 平台，可用 `--platform` 指定其他已安装插件。验证脚本只依赖 Python 3 标准库。

修改 UI 布局时先保持交互和数据流不变；按需复用现有工作区预览检查。拆分同一 Qt 类时保留统一头文件及唯一 MOC 输入；`terminal_input_test`、`project_io_test` 等按需链接目标必须显式包含新实现依赖，完整应用目标则通过共用源码集收集。

## 开发原则

1. 先确定数据归属，再添加界面。
2. 计算公式只放在 `src/topology`。
3. 画布到计算模型的转换只放在 `src/adapters`。
4. `MainWindow` 只装配模块，不新增大段领域逻辑。
5. 新交互必须补充相应测试。
6. 删除画布单元必须通过 `FlowsheetScene::removeUnit()`；禁止直接 `delete` 图元，否则产品合流或回流节点会保留悬空指针。
7. 新增类前先按 `MODULE_BOUNDARIES.md` 判断归属；跨层转换必须集中在既有适配入口。

## 新功能检查清单

- 是否需要稳定 ID？
- 数据是否应保存在 `FlowsheetDocument`？
- 拓扑变化和几何变化是否被正确区分？
- 修改输入后旧结果是否失效？
- 明暗主题是否都可读？
- SVG、PNG、JPEG 导出是否都应包含该对象？
- 是否覆盖连接、断开、合流和重算场景？

## Qt 约定

- 含 `Q_OBJECT` 的头文件必须登记为 xmake 的 MOC 输入。主程序及完整应用测试统一使用 `add_application_sources()` 中的头文件清单；仅按需链接的目标仍显式登记其依赖，不能依赖其他目标生成的 MOC。
- 图元颜色读取当前 `QApplication::palette()`，主题切换后显式刷新。
- 不在 `paint()` 中修改业务状态。
- 结果标注拖动和药剂标注键盘微调都保存相对锚点的偏移，不保存锚定标注的绝对位置。
- 测试目标只链接所需源码，避免把 `main.cpp` 链接进去。

## 撤销与重做

- 撤销由 `ProjectUndoManager` 管理，快照必须调用 `ProjectSerializer::serialize/deserialize`，不得另外维护一套项目复制格式。
- 连续信号在 200 ms 内合并为一个编辑事务；执行撤销或重做前先提交尚未到期的事务。
- 单条药剂粘贴在写入前后调用 `commitPending()`，每次粘贴形成独立事务，不能与前一次连续输入或下一次粘贴合并。
- 快照恢复前必须清除 `AnnotationManager` 的非拥有型图元索引，恢复期间使用 `m_loadingProject` 和管理器内部标志阻止结果失效及递归记录。
- 导入项目后清空撤销历史；保存项目后将当前撤销栈位置标记为干净状态。

## 添加计算指标

1. 在 `TopologyTypes.h` 中确定结果结构。
2. 在 `OpenCircuitCalculator` 中计算并测试。
3. 如需对比或导出，扩展方案对比与 Excel 展示。
4. 如需画布显示，扩展 `ResultMetric` 和 `AnnotationContentFormatter`。
5. 在 `ResultMetricMenu` 中加入入口。

## 扩展方程求解

- 流程守恒、实测值、分流比例及边界约束由 `BalanceEquationSolver` 组装为标量方程，`OpenCircuitCalculator` 负责校验、求解调度及结果编排；不要把物流或浮选语义加入 `LinearSystemSolver`。
- 通用加权约束最小二乘只放在 `WeightedLeastSquaresSolver`，不得依赖画布或文档类型。
- 工具栏编排放在 `MainWindowActions.cpp`，流程菜单及画布快捷键注册放在 `MainWindowFlowActions.cpp`，设备添加菜单放在 `MainWindowEquipmentActions.cpp`，显示设置菜单放在 `MainWindowDisplayActions.cpp`；项目生命周期命令放在 `MainWindowProject.cpp`，画布编辑及流程组粘贴放在 `MainWindowCanvas.cpp`，复制粘贴类型分派放在 `MainWindowClipboard.cpp`，单条药剂命令放在 `MainWindowReagents.cpp`；计算和结果联动放在 `MainWindowResults.cpp`，主题、状态与样式命令放在 `MainWindowPresentation.cpp`，导出命令放在 `MainWindowExport.cpp`。自定义编辑窗口放在 `ui`，只返回编辑值；表格筛选及绘制委托放在 `TerminalProductViewSupport`。
- 项目解析先生成 `ProjectSerializationData`，画布重建统一调用 `ProjectSceneRestorer`；不要在 JSON 解析器中直接修改当前场景。
- `ProjectJsonReader` 仅负责 JSON、当前版本与根结构检查，然后按顺序调用显示设置、几何连接、共享标注、方案测量的分段读取；共用读取工具放在内部 `ProjectJsonReadSupport.h/.cpp`。格式写出归 `ProjectJsonWriter`；`ProjectSerializer` 不应重新积累具体 JSON 字段或连接重建逻辑。
- 新的跨节点线性关系使用 `LinearBalanceConstraint` 表达，系数键必须是当前计算子图中的稳定物流 ID。
- “显示”下拉框及选中流程关联只能用于 `StreamFilterProxyModel` 的表格显示筛选，不得传入计算服务或改变拓扑方程范围；求解始终使用完整流程和全部已录入测量值。
- `CalculationInputBuilder::build()` 是文档数据到单组分计算输入的唯一组装入口；项目导入、界面重算和后续批处理必须复用 `FlowsheetCalculationService`。
- 物理解校验失败时只保留实测值。不得继续显示或累计来自同一个非法联立解的其他推导值。
- 数值算法调整至少补充 `topology_test`，覆盖矛盾、欠定、部分唯一、不同质量尺度和物理非法解。

数据协调通过 `CalculationMode::DataReconciliation` 显式启用，测量权重来自 `StreamMeasurement` 的质量及品位标准差；严格模式不得读取这些误差字段，也不得降低矛盾判定标准。协调算法必须复用拓扑守恒方程，并在结果中保留标准化残差用于异常值诊断。

协调模式目前使用一阶误差传播把质量和品位标准差转换为组分质量标准差。未填写时采用质量 1%、品位 0.1 个百分点的默认值；后续若增加相对误差、协方差或稳健估计，应扩展独立的测量权重模型，避免把相关性逻辑塞进表格模型。

## 操作日志

- `OperationLogDock` 只负责有时间戳的只读展示和容量限制，不直接了解具体业务操作。
- 用户可见的短操作结果仍通过 `QStatusBar::showMessage()` 发布，`MainWindow` 将其统一转发到日志。
- 需要保留具体诊断时，通过 `appendOperationLog()` 逐条写入；不要让计算内核依赖 UI 日志类。

结果指标框的 `Delete` 是会话级隐藏操作，状态只保存在 `AnnotationManager`，不能写回项目文档；`calculationChanged` 必须清除临时隐藏集合。药剂和自定义文字的 `Delete` 仍删除对应的 `AnnotationRecord`，新增键盘行为时要明确区分这两类生命周期。

质量单位和“显示产品名称”均是项目级显示元数据：预设单位保存在 `AnnotationTextSettings::massUnit`，自定义文本保存在 `customMassUnit` 并限制为 17 个字符，产品名称开关保存在 `showProductName`。它们只能改变指标卡文本，不能在格式化器、表格模型或求解器中换算或改变数值；编辑其他标注样式时必须保留这些显示设置。格式化器仅在产品名称非空时写入名称行，避免未命名的中间物流产生无意义标题。

`AnnotationManager::setDistinguishValueSources` 控制结果卡的边框区分，默认开启，通过 `QSettings` 的 `appearance/distinguishValueSources` 保存本机偏好；该开关不属于文档或项目格式，也不进入撤销历史。`AnnotationManager` 根据当前方案中求解器实际可用的完整质量/品位观测判断来源：当前可见质量或任一组分品位属于计算值时，调用 `AnnotationItem::setCalculatedResultEmphasis` 加粗边框。产率、回收率和产品名称不参与判断，协调后的观测仍按实测来源处理。格式化器只输出原有指标文本，不添加来源文字。切换显示开关不得改变测量、计算结果、标注位置或布局，不得触发项目变更。

修改来源展示或设置时，运行 `result_source_interaction_test` 和 `project_io_test`。交互测试覆盖多组分混合来源、未使用的部分输入、仅可见质量/品位影响边框、绘制前后对比、菜单开关与样式对话框、本机偏好恢复、项目数据和保存状态不变、项目往返及撤销重做保持偏好、方案切换、实际协调修正、符号/单位和 SVG 边框导出。

计算值边框线宽通过 `AnnotationManager::setCalculatedResultBorderWidth` 设置，并在 `QSettings` 的 `appearance/calculatedResultBorderWidth` 中保存。本机线宽的默认值、范围和有限值校验归 `ResultBorderStyle` 管理；线宽不进入项目、测量或撤销历史。`AnnotationItem` 的绘制和 `boundingRect` 必须同时考虑该线宽及选中时的附加线宽，避免导出或局部刷新时裁切边框。交互测试覆盖自定义数值、取消编辑、最大线宽与选中状态、普通实测卡不变、本机恢复和 SVG 实际线宽。

## 多组分数据

- 组分定义属于项目共享状态，使用稳定 `ComponentDefinition::id`；修改显示名称不能改变 ID。
- `StreamMeasurement::components` 按组分 ID 保存品位、标准差和支路占比，质量仍只保存一次。JSON 使用 `gradePercents`、`gradeStdDevs` 和 `componentSharePercents` 对象；支路占比不在右侧主表展示或编辑，但仍作为计算约束保存。
- `FlowsheetCalculationService` 对每个组分调用一次守恒计算器，结果写入 `CalculationResult::components`；第一个组分同时提供公共结果字段，供默认质量及结果视图使用。
- 右侧输入表、方案对比和 XLSX 导出必须遍历组分定义，不得假设只有一个品位列。
- 项目读取接受 `ProjectFormat.h` 定义的 v23 和 v24，保存仅使用 v24。`ProjectJsonReader` 在入口将 v23 共享文字复制到各方案并转换为 v24 结构，再交给同一套校验器，拒绝其他版本。测量仅保存于 `scenarios[].measurements`，药剂仅保存于 `scenarios[].reagents`，自定义文字仅保存于 `scenarios[].notes`；组分定义、方案、当前方案索引和显示样式均为必填字段；除 v23 文字归属转换外不做旧字段迁移。

## 球磨机

跨线桥按正交线段的内部交点检测，不能按图元相同就跳过：一个 `FeedJunctionItem` 含多条独立入料支路，支路之间也会相交。交点位于线段端点时保留转角或实际汇合，不绘制拱桥。`ball_mill_interaction_test` 的 `CrossingBridgeChecks.cpp` 覆盖同一汇流图元内部的交叉、移动和断开后的重算，以及 `tests/test.json` 中球磨机入料的九处交叉和 SVG 导出。

球磨机使用 `UnitKind::BallMill` 和项目类型键 `ball-mill`，图标为圆形。它通过 `isNonSeparatingUnit` 复用一入一出的守恒节点，不应创建左右两条分选产品或改变组分品位。设备入料的实际连接位置统一通过 `FlotationUnitItem::inputAnchorScenePosition()` 获取，球磨机锚在圆顶，其余设备保持原位置；直连、产品汇流和入料回流均须使用该锚点。球磨机出料锚在圆底正中，由 `ProductLineItem::sourceAnchorScenePosition()` 提供，终端线长从该锚点向下量取；连接、汇流及回流不能退回使用设备左右边沿作为球磨机出料位置。设备图标下方不再绘制名称、设备类型或二分流比例小字，贮池内部名称保留。修改相关代码时运行 `ball_mill_interaction_test` 及现有连接和项目读写测试。

球磨机圆形尺寸由 `FlotationUnit::ballMillDiameter` 决定，绘制、命中形状、包围框及进出料锚点必须使用同一数值。`CanvasActions::resizeSelection` 对圆圈调整直径，对 `InputLineItem` 调整其所属单元的可选 `inputLength`，均沿用画布尺寸快捷键。入料线默认长度为普通单元 72、球磨机 40，设置后的范围为 30～5000；圆圈直径范围为 40～720。入料顶端必须通过 `InputLineItem::startScenePosition()` 获取，连接命中、Ctrl 对齐和入料汇流不得继续使用固定的 `-InputHeight`。几何参数随单元保存并进入流程组复制快照；修改时运行 `ball_mill_interaction_test` 中的 `ResizeInteractionChecks.cpp`，覆盖快捷键、实际绘制、连接、回流、复制、项目读写与撤销重做。

## 浓密池

`UnitKind::Thickener` 和项目类型键 `thickener` 表示一入一出的浓密池，经 `isNonSeparatingUnit()` 复用非终端守恒节点。`ThickenerGeometry.h` 统一定义池口、锥底及选择形状；入口锚在池口顶部，出口锚在锥底正中，稳定出料 ID 为 `<unit-id>:right`。`hasCenteredVerticalPorts()` 统一标记球磨机和浓密池，调整宽度时刷新路径而不横移下游设备。不得给浓密池创建第二条产品线；设备下方不绘制说明小字。运行 `thickener_interaction_test` 验证菜单、选择与尺寸快捷键、物料守恒、连接/汇流/回流、复制、读写、撤销重做及 SVG 图形。

## 添加药剂标注

1. 药剂记录属于 `ExperimentScenario::reagentAnnotations`，不得放入项目共享状态。
2. 使用 `AnnotationKind::Reagent` 和 `AnnotationOwnerKind::Stream`，所有者使用稳定物流 ID。
3. 名称、用量、单位和备注分别保存；显示拼接及化学式排版放在 `ReagentAnnotationItem`。
4. 锚点解析和增删生命周期由 `AnnotationManager` 负责。
5. 药剂位置调整统一调用 `AnnotationManager::nudgeSelectedReagents()`；不要在图元或场景过滤器中再次处理方向键。
6. 补充方案切换、项目往返、主题和导出测试。

自定义文字的内容和字体属性必须通过 `AnnotationItem::setRecord()` 同步。该入口始终在 `prepareGeometryChange()` 后重新测量文本边界；不能只调用可能因文字内容未变化而提前返回的 `setText()`。

## 试验方案

- `ExperimentScenario` 不保存拓扑副本，只保存随试验变化的数据；
- 新增方案为空白，复制方案复制当前药剂、实测值及内存中的计算结果；
- 产品名称、画布位置、标注样式和结果标注偏移为项目共享状态；
- 项目文件保存全部方案的输入数据和最近计算结果；导入后可直接恢复指标标注，输入或拓扑变化时再使结果失效；
- 修改拓扑后调用 `invalidateAllCalculations()`，不能只清除当前方案。

## 画布视图交互

- 缩放和平移等视图级交互放在 `CanvasView`，不要写入场景或图元；
- 滚轮缩放必须保留合理的倍率上下限，并以鼠标位置为锚点；
- 中键事件由视图消费，不能触发图元选择、拖动或连接；
- 方向键先由 `CanvasView` 处理；视图通过注入的处理器请求药剂微调，业务上的选中判断和位移由 `AnnotationManager` 完成；
- 新增视图交互后扩展 `canvas_view_test`。

## 正交布线

- 普通连接、产品合流横杆和回流走廊分别保存最小人工段参数，不保存由端点推导出的完整 `QPainterPath`；回流支路分别保存 `routeX` 和 `routeY`；
- 普通产品连接的自动横向折点使用实际产品线长，并保持在来源与目标入料锚点之间，避免短间距时越过下游设备再折回；人工路线仍使用用户指定坐标。`connection_test` 覆盖缩短产品线后的对齐连接、横向错位、压缩间距、等高和向上连接。
- 路线拖动按 10 个画布单位吸附，完成后发出 `geometryChanged`，不得使计算结果失效；
- 右键菜单由 `AnnotationManager` 的场景事件过滤器统一构建，新增线路命令必须合并到该菜单，避免图元菜单被过滤器截获；
- 跨线桥由 `FlowsheetScene::drawForeground` 委托 `CrossingBridgeRenderer` 动态绘制，处理水平—垂直内部交点，包括同一个汇流图元内不同支路的交叉；排除端点转角和实际汇合，不得修改拓扑。桥形与水平线以水平切线衔接，遮罩不得擦断中心竖线；
- 项目保存普通线 `routeY`、合流 `mergeY` 和各入料来源的 `routeX/routeY`；这些字段只在设置了手动路线时写入，缺失表示自动布线；
- 回流支路选择必须基于各自的路径轮廓；横向和纵向控制柄只修改对应轴。`selectedSourceStreamId()` 用于回流高亮和断开来源；药剂快捷键读取独立的 `selectedAnnotationStreamId()`，涵盖回流、主入料、外部入料和公共输出，不得沿用上一次回流选择作为药剂目标；
- 产品线和合流线的 `shape()` 使用线条轮廓与实际箭头，不能填充开放折线形成的区域；否则可能把空白变为点击目标，或因填充规则抵消可见线段的命中范围。`connection_test` 和 `ReagentMenuTargetChecks.cpp` 覆盖相关回归；
- 修改布线路径后运行 `connection_test`、`merge_test`、`project_io_test` 和导出测试。
- 终端产品长度属于 `ProductLineItem`，只能对 `isAvailable()` 的产品通过快捷键调整，范围为 30～5000；项目在单元的 `terminalLengths` 对象中按产品侧别保存覆盖值。

## 二分流器

- 二分流器使用 `UnitKind::BinarySplitter`，连接端口及稳定物流 ID 与普通一入两出单元一致；不要根据图形或名称推断节点类型；
- `leftSplitPercent` 必须为有限数且严格位于 `(0, 100)`，右支路比例始终由 `100 - left` 推导，不保存第二份状态；
- 计算拓扑为干质量和各组分质量同时生成比例约束，且不得为二分流器生成浮选性能指标；
- 二分流器按同一比例约束反算支路，不维护固定必填物流清单；是否已有足够独立约束由方程秩判断；
- 项目必须保存 `kind`，二分流器还必须保存 `leftSplitPercent`，缺失字段直接拒绝；
- 修改二分流器后运行 `topology_test` 和 `project_io_test`，覆盖比例反算、项目往返及无效格式拒绝。

## 三产品单元

- `UnitKind::ThreeProductFlotation`、`ThreeProductScreening` 与 `ThreeProductDemediumScreen` 都使用三产品端口；中间物流 ID 固定为 `<unit-id>:middle`；
- 计算拓扑必须使用独立的 `PortKind::MiddleProduct`，不得把第三产品表达为产品合流或额外节点；
- 守恒、节点性能、右侧输入、标注、连接、回流和导出必须遍历实际产品集合，避免重新引入固定两个产品的假设；
- 项目使用 `three-product-flotation` 保存类型；
- 修改第三输出后运行全量测试，至少覆盖三产品守恒、性能指标、中间产品连接及项目保存往返。

## 磁选机

- `UnitKind::MagneticSeparation`、`WeakMagneticSeparation` 和 `StrongMagneticSeparation` 都是两产品分选单元：沿用一入两出拓扑、稳定物流 ID（`<unit-id>:left/right`）和物料平衡方程，不能为此复制浮选求解器；
- 弱磁选机使用筒式滚筒与马蹄磁铁，强磁选机使用立式环与对置磁极；左、右产品的定位描述使用“磁性产品”和“非磁性产品”。这些描述不写入产品名称，产品名称仍默认为空；
- 项目分别使用 `magnetic-separation`、`weak-magnetic-separation` 和 `strong-magnetic-separation` 保存类型。新增流程单元类型时，同时更新两向序列化映射、对象定位名称与项目往返测试。

## 贮池

- `TailingsPool` 是终端汇：只允许一条入料、没有出料；其入料物流仍是终端产品，不能为贮池添加“入=0”的守恒方程；
- `ConcentratePool` 和 `WaterPool` 是一入一出的缓冲池：使用稳定的右侧物流 ID `<unit-id>:right`，在干质量和各组分质量方程中均施加入料=出料；
- 画布采用无上沿的开口池体与波浪液面，默认标注尾矿池、精矿池、回水池；双击贮池可设置波浪线上方的显示名称。该名称只影响图形显示，不改变产品物流、方程或默认端口。
- 项目分别使用 `tailings-pool`、`concentrate-pool`、`water-pool` 保存类型；可选 `poolLabel` 保存自定义贮池名称，未设置时使用该类型的默认显示名称。
- 所有单元形象图使用同一连接布局规则：普通产品或合流输出拖拽连接默认不得移动目标单元或其连通组件；仅在松开鼠标时持有 `Ctrl` 才把下游组件对齐到产品端点。该标志属于瞬时 UI 交互，不能保存到项目或影响拓扑；不得为贮池、磁选、筛分等任一单元类型添加位置例外。
- 新建单元 ID 必须由 `MainWindow::takeNextUnitId()` 分配；分配前从 1 开始扫描当前未使用的数字 ID，加载、删除、粘贴或撤销后也不能依赖陈旧计数器。读取器必须拒绝重复 ID，不能静默重命名，因为连接与测量数据无法无歧义地指向其中一个单元。

## 重选设备

- `SpiralChute`、`ShakingTable`、`DenseMediumCyclone` 与 `SedimentationTank` 是普通两产品分选单元，沿用一入两出守恒方程、稳定的左/右产品物流 ID、回流和合流能力；不要为设备外观复制求解逻辑；
- 螺旋溜槽以三级椭圆槽道绘制，定位描述区分螺旋精、尾产品；重介质旋流器以圆筒—锥体及内部旋流线绘制，定位描述区分重、轻产品；
- 摇床以倾斜台面和横向床条绘制，定位描述区分摇床精矿、尾矿；
- `DemediumScreen` 沿用筛分的一入两出守恒方程，定位描述区分脱介产品、稀介质；图形在倾斜筛面上补充喷水和水滴，避免与普通筛分机混淆。
- `SedimentationTank` 的定位描述区分溢流、底流；以矩形澄清区、液面和漏斗底部绘制。它是分选单元而非贮池，因此参与常规一入两出物料守恒。
- `ThreeProductDemediumScreen` 复用三产品守恒方程，定位描述区分脱介产品、浓介质、稀介质；图形使用双层筛面与喷水标识。项目使用 `three-product-demedium-screen` 保存类型。
- `MediaTank` 是一入一出的固定缓冲贮池，沿用 `PoolOutput` 和入料=出料约束；以圆筒桶体和液面绘制，桶体下方不再绘制名称。
- `MixingTank` 同样是一入一出的固定缓冲贮池，多个来料应先接入入料汇流节点；图形在圆筒桶体上增加电机、搅拌轴和叶轮。它不引入额外配料或反应方程。
- 项目分别使用 `spiral-chute`、`shaking-table`、`dense-medium-cyclone`、`demedium-screen`、`media-tank`、`mixing-tank` 和 `sedimentation-tank` 保存类型，并须覆盖项目保存往返测试。

上述定位描述由适配器生成，供表格和选择使用，不是用户产品名，也不作为 Excel 名称回退。各设备的产品名称均由用户在右侧表格单独填写，初始为空。

## 筛分与分级

- `Screening` 和 `Classification` 均为普通两产品分选单元：复用一入两出守恒方程、稳定的左/右产品物流 ID，以及现有回流和汇流能力；它们只增加设备形象与定位描述，不能新增求解器分支；
- 筛分机绘制倾斜筛面和筛孔，定位描述区分筛上、筛下产品；分级机绘制倾斜槽体和螺旋轴，定位描述区分溢流、沉砂；
- 项目分别使用 `screening` 和 `classification` 保存类型，新增类型时必须同步两向序列化、对象定位显示名、选中提示和项目往返测试。

## 自定义文字颜色

- `AnnotationRecord::noteColor` 仅适用于 `UserNote`；无效 `QColor` 表示跟随项目的全局标注文字颜色，有效颜色覆盖该单个文字图层；
- 项目必须保存 `noteColor` 字符串：空字符串表示跟随全局颜色，`#AARRGGBB` 表示独立颜色，不能把主题色写回记录。

## 三产品筛分器

- `ThreeProductScreening` 使用与三产品浮选单元相同的 `LeftProduct`、`MiddleProduct`、`RightProduct` 端口和 `<unit-id>:left/middle/right` 稳定物流 ID；三产品能力必须通过 `hasMiddleProduct(UnitKind)` 统一判断，禁止重新写死为 `ThreeProductFlotation`；
- 该单元绘制双层倾斜筛网，定位描述区分筛上、筛中、筛下产品，产品名称默认留空；求解器仍仅使用三产品守恒方程，不引入筛分专属计算逻辑；
- 项目使用 `three-product-screening` 保存类型，并须覆盖三产品端口及保存往返。

### 产品端口扩展规则

- 画布代码使用 `productSideSuffix()`、`productSideLabel()` 和 `productSideDirection()`，不要新增侧别三元表达式或手工拼接物流后缀；
- 计算代码使用 `flotationProductPorts()` 和 `TopologyGraph::flotationProductStreams()` 遍历实际产品，保持端口与物流顺序一致；
- 节点性能通过 `FlotationPerformance::forPort()` 和 `setForPort()` 访问，界面不直接判断列号对应哪个具名字段；
- 项目单元类型字符串只在 `ProjectUnitKindCodec` 的 `unitKindKey()`/`unitKindFromKey()` 中维护；新增类型时必须同时扩展两向映射和往返测试。

## 闭路连接

- 同一连通分量内的产品到入料连接视为回流，不能按开路连接自动移动目标单元；
- 入料与回流的画布表现放在 `FeedJunctionItem`：正常入料可以是外部新鲜入料、`ProductLineItem` 或 `MergeJunctionItem`，附加来源使用产品与合流输出集合，拓扑中统一转换为支持两条及以上输入的 `MergeNode`；
- 项目格式使用 `feedJunctions[].sources` 数组保存多输入端点，并以 `hasExternalFeed` 明确区分“外部新鲜入料 + 回流”和“纯多输入汇流”；载入器要求 `sources` 数组与布尔值 `hasExternalFeed`，不从来源数量推断外部入料；
- 将回流接入已占用入料时，汇合点必须保存原正常上游端点；断开回流及项目重新载入后都必须恢复该连接；
- 多输入汇流点的来源支路被单独选中时，`Ctrl+B` 必须只解除该来源；选择公共汇流线时才允许整体拆除；
- 已占用入料图元虽然隐藏，其几何线段必须继续作为拖放热区；命中后临时显示高亮，并由 `closed_loop_test` 覆盖；
- 同一主流程允许存在多个独立或嵌套的 `FeedJunctionItem`；连通分量遍历必须覆盖每个汇合点的正常来源及全部附加来源；
- 多回流项目保存/载入由 `project_io_test` 覆盖，载入后汇合点 ID、外部入料数量和闭路检测结果必须保持一致；
- 产品合并输出接入回流后必须从终端产品列表移除，断开后恢复；
- `Cycle` 表示已经识别到闭路，不再作为拓扑结构错误；
- 所有 `reportStreams` 都允许录入，求解服务必须收集其中所有完整实测值；不恢复旧的固定必填/取样清单或输入白名单；
- 闭路计算必须覆盖回流、内部入料、外部新鲜入料以及全流程指标；缺少足够独立约束时应返回 `Underdetermined`，不能把未填写某条建议撕裂流直接当作欠定判据；
- 求解器必须从完整方程组判断秩，不能依赖节点顺序进行局部传播；新增约束类型时分别为干质量和组分质量生成方程，并保留矛盾、欠定和非物理解诊断；
- `topology_test` 必须包含至少一个没有局部传播起点、只能通过联立消元求解的闭路；
- 修改回流连接、断开或路由规则后运行 `closed_loop_test`。

## 导出格式

- 矢量格式由 `SvgExporter` 负责，栅格格式由 `RasterExporter` 负责；
- 各格式使用一致的场景范围、留白、背景和可见性规则；
- PNG/JPEG 导出倍率必须作用于实际渲染尺寸，不能在渲染后拉伸；同时限制最长边和总像素数，避免大画布造成过高内存占用；
- XLSX 数据导出由 `ExcelDataExporter` 负责，候选来自 `reportStreams`，由 `namedStreams()` 仅保留 `FlowsheetDocument::productName()` 非空白的物流；名称不得回退为定位信息。命名总入料、中间物流和最终产品均可导出，没有计算结果的行直接忽略；
- XLSX 排序读取 `FlowsheetDocument::exportStreamOrder()`；先输出仍存在且已命名的已配置物流，再按流程顺序追加其他已命名物流。导出顺序对话框复用同一筛选结果，恢复顺序也只能列出已命名物流。总入料不做固定位置处理；
- 新增或修改格式后扩展 `export_test`，校验文件可生成且格式可读取。
- 没有命名物流或所有命名物流均无计算结果时，应在写文件前返回说明，不能生成空工作簿或覆盖已有文件；产品名称默认留空，但 `TerminalProductTableModel::NameColumn` 的定位信息保留。
- 修改 Excel 导出时运行 `excel_export_test`，校验实际 OOXML 单元格、命名筛选、排序、方案合并、清空/空白名称和空导出保护；运行 `stream_filter_interaction_test` 验证真实表格命名、排序对话框和无命名产品时的导出提示，相关补充检查位于 `ExcelExportInteractionChecks.cpp`。

## 发布前验证

修改共享数据结构或主窗口生命周期后，先执行一次干净构建，再运行退出冒烟测试。该入口会触发退出确认并自动选择“不保存”，覆盖 `closeEvent` 和完整析构路径：

```bash
xmake clean AutoFlotationSheet
xmake build AutoFlotationSheet
QT_QPA_PLATFORM=offscreen xmake run AutoFlotationSheet --open-project tests/test.json --smoke-test
```

Linux：

```bash
xmake f -p linux -m release
xmake build -a
```

Windows：

```powershell
C:\Users\13427\xmake\xmake.exe build -a
```

`build -a` 同时构建主程序和全部 17 个测试目标。逐项执行命令及职责见 [README 测试说明](../README.md#测试)；Qt Test 用于主窗口交互测试。全部检查通过后再执行 `xmake deploy` 生成部署目录或刷新平台运行时。

## 筛选联动回归测试

修改物流筛选或画布与表格的选择联动时，运行：

```bash
xmake build stream_filter_interaction_test
QT_QPA_PLATFORM=offscreen xmake run stream_filter_interaction_test
```

该目标链接完整应用模块和 Qt Test，通过真实 `MainWindow`、鼠标/键盘事件与菜单动作验证完整信号链，不直接调用筛选器的焦点设置接口。临时项目和 QSettings 配置均写入独立临时目录。测试同时检查可见物流 ID、行选择、编辑列、当前物流、显示计数，以及筛选不修改项目数据或使计算失效。

同一目标还运行 `ExcelExportInteractionChecks.cpp`，验证表格产品名称默认为空但保留定位信息、真实名称编辑影响导出资格、排序列表只含命名项，以及无命名产品时在打开保存对话框前给出提示。

## 画布对象复制粘贴

画布的复制粘贴快捷键只注册一次，由 `MainWindowClipboard` 与流程菜单共用分派入口。选择中包含设备或 `UserNote` 时沿用流程组复制；仅选中一条药剂时调用单条药剂复制。`CanvasClipboardKind` 区分 `Empty`、`FlowGroup` 和 `Reagent`；不支持的选择执行复制后进入 `Empty`，避免再次粘贴上一个对象。单条药剂的右键菜单和快捷键共用 `MainWindowReagents` 命令及 `ReagentClipboard` 值快照；流程组和药剂各自保留内容，右键粘贴药剂不受当前快捷键类型限制。

`FlowGroupClipboard` 保存选中的单元与 `UserNote` 文字快照。复制时不得保存图元指针；粘贴时为文字生成未使用的标注 ID，完整保留文字样式，并与单元共享平移偏移。由 `AnnotationManager` 创建副本文字图元，再选中副本。单独粘贴文字不得发送 `topologyChanged` 或清空计算结果。复制粘贴快捷键仅在画布中生效，保留输入框的普通文本剪贴板操作。

流程组同时快照当前方案中属于实际复制物流的 `Reagent` 记录：产品线、未连接的主入料，以及完整复制的产品汇流、入料汇流总料和外部新鲜入料。粘贴时用原物流 ID 到副本物流 ID 的映射重写 `ownerId`，保留相对偏移，不再叠加组平移；新记录只写入粘贴时的当前方案。采集顺序与图元堆叠共用 `ReagentOrder`，再用未占用的批次前缀与等宽序号生成副本 ID，保留普通药剂和单条粘贴药剂的显示顺序。主窗口在药剂粘贴后显式同步标注，保证无需计算即可显示副本。

修改时运行 `clipboard_interaction_test` 和 `project_io_test`，覆盖单独/多选/混合复制、重复粘贴与 ID 冲突、样式和位置、撤销重做以及项目保存往返。药剂专项检查位于 `ReagentClipboardChecks.cpp`，由同一交互测试目标执行，覆盖无需选中药剂、多药剂堆叠、不可变快照、方案切换、完整/部分流程及汇流回流的归属映射。

产品线的 `terminalLengthOverride` 也属于流程组几何快照；已连接到回流或合流的产品仍使用这个端点长度，不能只复制未连接产品的长度。粘贴时先恢复长度，再重建连接。`CopyMoveInteractionChecks.cpp` 加载 `tests/copy-move.afs.json`，通过画布复制粘贴和鼠标拖动比较整条回流路径及产品路径的平移结果。多选拖动的路线平移按每个场景鼠标移动事件、每个连通组件各执行一次，不能用零延时定时器合并，否则连续鼠标事件会漏掉位移。程序调用单个单元的 `setPos` 则直接平移整个连通组件。

## 清理已移除的 UI 功能

删除入口时同步检查派生状态、数据目录、菜单回调及仅服务于旧入口的兼容接口。计算快照不再包含关注对象目录和固定取样列表；诊断标题直接使用计算图节点/汇流角色，实际观测仍来自全部报表物流。不要为尚无入口的未来功能保留逐次构建的数据。

`ResultMetricMenu` 自身就是 `QMenu`，不创建隐藏的旧工具按钮；线路的右键动作仅由 `AnnotationManagerContextMenu.cpp` 构建，图元保留鼠标与键盘处理，不再复制一套被场景过滤器覆盖的右键菜单。单来源/前两支路的旧便利接口统一改用来源集合，场景中多个来源的语义保持一致。

回归检查覆盖指标菜单、单位编辑及取消/同步、统一右键恢复三类线路、诊断节点标题与输入定位、多来源连接、保存读取和计算；测试不再验证删除的对象目录或固定必填清单，而验证实际报表物流、拓扑端口及求解结果。

## 文件职责拆分

- 主窗口中只保留编辑命令；标注文字窗口和 Excel 排序窗口分别放在 `ui/AnnotationTextStyleDialog.*`、`ui/StreamOrderDialog.*`。窗口返回可选值，取消时不提交文档，也不产生撤销记录；原菜单文字和翻译上下文保持一致。结果联动和计算编排位于 `MainWindowResults.cpp`。
- `FlowGroupClipboard` 仅持有 `flow_group::Snapshot` 与连续粘贴计数；采集位于 `FlowGroupSnapshotCapture.cpp`，重建位于 `FlowGroupSnapshotPaste.cpp`。快照不得保存图元指针，粘贴使用只读快照；路线坐标、产品线长度、文字、药剂和 ID 顺序必须随重构保留。
- 项目读写器统一调用 `ProjectUnitKindCodec`；类型键的双向对应关系只定义一次，中文显示名称继续由呈现层管理。

- 场景事件与命中留在 `FlowsheetScene.cpp`；连接/断开/删除放在 `FlowsheetSceneConnections.cpp`，连通组件和尺寸移动放在 `FlowsheetSceneMovement.cpp`。
- 设备几何与交互留在 `FlotationUnitItem.cpp`；`FlotationUnitItemPainting.cpp` 只处理绘制上下文和选中态，设备符号委托 `graphics/symbols`。新增设备形状按贮池/磨矿/浓密或分选职责放入对应符号文件，由 `UnitSymbolPainter.cpp` 分派；符号绘制只能读取设备值，不能访问场景或修改拓扑、文档。
- 结果卡同步、锚点与生命周期留在 `AnnotationManager.cpp`；药剂/自由文字同步放在 `AnnotationManagerContent.cpp`，显示设置及本机偏好放在 `AnnotationManagerDisplay.cpp`，事件分派、删除和微调放在 `AnnotationManagerInteraction.cpp`。右键菜单放在 `AnnotationManagerContextMenu.cpp`，位置目标解析放在 `AnnotationContextTarget.cpp`，单条药剂追加摆放放在 `AnnotationManagerReagentPlacement.cpp`。
- 产品合流的状态与绘制留在 `MergeJunctionItem.cpp`，路线与手动汇合高度放在 `MergeJunctionItemRouting.cpp`，交互事件放在 `MergeJunctionItemInteraction.cpp`，唯一物流命中查询放在 `MergeJunctionItemHitTest.cpp`。
- 入料汇流的来源集合与绘制留在 `FeedJunctionItem.cpp`，路径与锚点放在 `FeedJunctionItemRouting.cpp`，事件处理放在 `FeedJunctionItemInteraction.cpp`，唯一物流命中查询放在 `FeedJunctionItemHitTest.cpp`；产品线的几何与显示留在 `ProductLineItem.cpp`，拖拽、路线编辑及键盘事件放在 `ProductLineItemInteraction.cpp`。
- `TerminalProductDock.cpp` 保留对象初始化、信号连接、键盘事件和主题刷新；布局放在 `TerminalProductDockLayout.cpp`，组分编辑放在 `TerminalProductDockComponents.cpp` 只采集名称并提交会话命令，列与委托及宽度保护放在 `TerminalProductDockColumns.cpp`，选择联动放在 `TerminalProductDockSelection.cpp`，进度与状态呈现放在 `TerminalProductDockResults.cpp`，预览求解与缓存由 `app/CalculationController` 统一管理。计算说明弹窗位于 `CalculationExplanationDialog`，只接收结果值，不能访问文档或重新计算。
- `TerminalProductTableModel.cpp` 管理列、编辑与模型通知，`TerminalProductTableModelData.cpp` 管理显示角色和工具提示。
- `ExcelDataExporter` 管理命名筛选与导出条件，`ExcelWorksheetBuilder` 生成工作表 XML，`XlsxWorkbookWriter` 负责标准部件及 ZIP 写入；按需链接的 `excel_export_test` 必须同时包含三个实现文件。
- `FlowsheetDocument.cpp` 保留项目共享元数据及替换/清理；标注归属和显示设置放在 `FlowsheetDocumentAnnotations.cpp`；当前方案测量、组分及计算结果放在 `FlowsheetDocumentMeasurements.cpp`，方案增删与切换放在 `FlowsheetDocumentScenarios.cpp`。
- `OpenCircuitCalculator.cpp` 负责计算校验与结果编排，内部 `BalanceEquationSolver.h/.cpp` 只组装并求解干质量或组分质量的标量方程；拆分时不得调整守恒关系、权重或诊断条件。
- 拆分实现文件后，必须同步 xmake 中按需链接源码的测试目标。`add_canvas_sources()` 集中收集场景、线路和 `graphics/symbols/*.cpp`，`add_application_sources()` 集中收集全部应用源码及 MOC 清单；文档、面板、表格模型分别使用 `FlowsheetDocument*.cpp`、`TerminalProductDock*.cpp`、`TerminalProductTableModel*.cpp`。项目读取器和方程组装器的内部辅助文件不能代替原有服务成为应用入口。

`project_io_test` 的主函数仅初始化 Qt 应用、临时目录并调用检查函数。`ProjectIoChecks.h` 声明检查入口；`ProjectIoRoundTripChecks.cpp` 检查元数据、方案、标注和损坏文件保护，`ProjectIoFormatChecks.cpp` 检查版本与字段拒绝，`ProjectIoConnectionChecks.cpp` 检查合流及回流，`ProjectIoEquipmentChecks.cpp` 检查设备类型，`ProjectIoHistoryChecks.cpp` 检查撤销与复制路线，`ProjectIoScenarioNoteChecks.cpp` 检查文字的方案隔离、读写、撤销及字段拒绝，`ProjectIoVersion23Checks.cpp` 检查真实 v23 样例、共享文字迁移及另存重开，`ProjectIoExampleChecks.cpp` 检查仓库样例和可选外部项目。xmake 用 `tests/cpp/ProjectIo*.cpp` 收集全部检查，保持原有失败码和调用顺序。

样式窗口重构的确认/取消、颜色模式、撤销/重做检查位于 `AnnotationTextStyleDialogChecks.cpp`，随 `result_source_interaction_test` 执行；排序窗口继续由 `stream_filter_interaction_test` 中的 `ExcelExportInteractionChecks.cpp` 验证。`project_io_test` 必须显式链接 `ProjectUnitKindCodec.cpp` 和 `FlowGroup*.cpp`，不能依赖主程序的对象文件。

## 属性、计算问题与流程模板回归

运行 `workspace_tools_interaction_test`，其中 `WorkspaceSidebarChecks.cpp` 检查默认表格、显示设置菜单的鼠标切换、右侧无切换按钮和顶部占位、上方仅一个页面可见且日志始终显示、日志在窗口缩放和页面切换后保持八分之一高度、500 条日志容量、画布占满左侧、页面状态保留、后台更新不抢占页面及切换不修改项目。`ObjectPropertyChecks.cpp` 专门覆盖属性编辑与撤销，`CalculationDiagnosticsChecks.cpp` 专门覆盖问题条目、自动跳转策略和定位编辑；两者共用 `WorkspaceToolsTestSupport.h`，只初始化一次测试应用和临时环境。`WorkspacePanelSelectorChecks.cpp` 检查下拉框紧邻“显示设置”、实际列表鼠标选择、菜单及程序跳转同步、无焦点悬停滚轮、方向与首尾边界、半格累积及反向重置，并断言项目数据、结果和撤销历史不变。该目标还覆盖数值实时输入、小数和输入焦点保持、增减按钮与方向键步长、本机步长偏好、精确属性输入、未修改值不提交、对象切换与撤销重做、各回流支路独立编辑和恢复、圈形尺寸、错误定位及双击输入、组分列跳转、全局矛盾、模板保存和跨窗口重载、重复插入的新 ID 与偏移、文字/药剂/路线保留、旧格式拒绝、重命名和删除不修改当前项目。

`TerminalTableLayoutChecks.cpp` 验证结果小表格已移除、主表占满剩余高度、计算说明使用独立只读弹窗且不修改项目，以及初始列宽铺满、窗口缩放、真实鼠标拖动列宽及面板切换保留、多组分/协调模式列重建和窄视口滚动；布局操作不得修改项目或计算结果。列宽自动适配通过视口 Resize/表格 Show 后的合并延时任务执行，所有列保持 Interactive，内部配置与分配宽度时必须屏蔽手动调整标记。

UI 的属性编辑请求同时校验对象标识与控件代次，不能把旧面板值提交到新选择；同一数值不固化自动布线参数。同一对象的同类表单应原位更新并屏蔽回填信号，保留正在输入的小数点、光标和连续增减状态；切换对象或字段结构才重建。步长属于 UI 本机偏好，不应放入画布属性描述或项目格式。问题列表在结果失效时清除，全局矛盾不能伪装成某一测量的唯一错误。计算按钮仅在 `IssueCode::InconsistentBalance` 且 `IssueSeverity::Error` 时自动打开问题面板，不能用“存在问题”或“计算未完成”作为跳转条件；同码的协调大残差警告不触发跳转。显式检查流程菜单仍打开问题面板。模板测试将 `XDG_DATA_HOME` 指向临时目录，本机模板不写入项目 JSON；模板容器内嵌当前格式项目，不创建另一套设备或路线序列化代码。新增 `Q_OBJECT` 面板头（包括 `WorkspaceSidebar.h`）必须登记到 `add_application_sources()` 的公共 MOC 清单，主程序与全部完整应用测试共同复用。修改面板所有权或主窗口销毁顺序后，执行主程序干净构建和带项目打开的启动冒烟检查。

`TemplatePreviewChecks.cpp` 随 `workspace_tools_interaction_test` 执行，覆盖单击预览、回流及文字/药剂/名称保留、只读拖动和双击、窗口编辑快捷键隔离、撤销与剪贴板及设备 ID 不受影响、大范围模板的连续缩放、中键平移、分隔条调整、适应窗口、明暗主题、搜索无结果、损坏模板、删除清理及列表双击插入。只为当前选中模板创建预览场景；场景、文档和标注管理器的生命周期属于预览会话，视图必须在销毁会话前脱离旧场景。

`terminal_input_test` 通过 `TerminalInputTest.cpp` 初始化 Qt 并顺序调用检查组。`TerminalInputModelChecks.cpp` 验证输入、列与主题；`TerminalInputCalculationChecks.cpp` 验证平衡、传播与闭路；`TerminalInputFilterChecks.cpp` 验证物流筛选及清空输入；`TerminalInputAnnotationChecks.cpp` 验证药剂公式和标注。`TerminalInputContext` 只共享跨组使用的场景和值快照，视图与模型由各组局部持有；数值失败码沿用原检查。xmake 通过 `TerminalInput*.cpp` 收集全部检查。

## 药剂复制回归

`ReagentMenuInteractionChecks.cpp`、`ReagentMenuTargetChecks.cpp` 和 `ReagentShortcutChecks.cpp` 随 `clipboard_interaction_test` 执行：

| 检查文件 | 验证范围 |
|---|---|
| `ReagentMenuInteractionChecks.cpp` | 真实上下文菜单及鼠标点击、未复制时禁用、复制不修改项目或系统剪贴板、不可变值快照、已有标签避让、逐次撤销、保存读取、删除源标注后粘贴、跨方案和流程组排列顺序 |
| `ReagentMenuTargetChecks.cpp` | 普通、合流、回流、主入料、外部入料及公共输出目标，旧选中支路不干扰、共用交点禁用、折线空白不命中及菜单打开后目标删除 |
| `ReagentShortcutChecks.cpp` | 真实鼠标选择和画布快捷键、药剂与流程组类型切换、两种入口互通、各类支路、连续粘贴、独立撤销、无目标和交点拒绝、值快照及输入框剪贴板隔离 |

复用画布已有快捷键，不重复注册导致歧义。药剂快捷键粘贴要求单选有效线路，也允许选中药剂后向其所属物流追加；无目标、多选、无法唯一识别时只提示，不回退到旧选中线路。汇流图元的选中标注物流、窗口粘贴类型及复制快照仅为会话状态，不新增项目字段或版本。

`ProjectUndoManager::commitPending()` 用于离散命令的前后边界，保证每次粘贴独立撤销；数值输入和拖动仍由已有定时检查点合并连续编辑。

## 计算边界回归

`calculation_service_test` 的链接集合只包含 QtCore/QtGui、文档和求解服务，不包含 QtWidgets、adapters、editor 或 graphics。它从纯值构建流程，覆盖部分可解、严格平衡、多组分、矛盾、协调及支路占比输入，用构建依赖保证求解服务能脱离画布。

`CalculationControllerChecks.cpp` 随工作区测试运行，验证画布销毁后的快照计算、按 ID 定位、预览合并与只读性、选择/筛选不刷新预览、显式计算提交、修改测量后的缓存失效、方案切换、组分和计算模式变化。原有筛选交互测试继续覆盖共享汇流图元的外部入料/总入料行保留、画布双向定位、删除、导入和撤销。

新增求解入口必须使用 `FlowsheetSnapshot`；新增表格导航必须使用物流 ID。禁止将 `CanvasItemIndex` 放回表格或保存进计算缓存。预览不写入 `FlowsheetDocument`，也不作为新项目格式字段；保存仍使用 v24，v23 读取行为不变。

## 会话与图元契约回归

- `table_session_test` 只使用 QtCore/QtGui，检查应用层数值/组分/模式校验、不可编辑或失效物流拒绝、独立状态副本、结果失效和方案切换。
- `TerminalInputSessionChecks.cpp` 用契约替身运行真实表格模型，不创建文档或应用控制器；验证编辑请求、拒绝传播、状态发布后更新、名称列通知范围及列结构切换。
- `CanvasHostChecks.cpp` 使用只实现几何/连接契约的普通 Qt 场景，不使用 `FlowsheetScene`；检查同步移动和尺寸请求、连接释放、路线通知及图元转移后的回调隔离。原有复制组移动和真实连线交互继续回归，确保时序不变。

会话的 MOC 输入仅登记 `contracts/TerminalTableSession.h`，具体控制器复用会话信号。按需链接的表格测试需显式加入控制器实现与契约 MOC；完整应用复用公共清单。表格模型通过快照比较决定结构重置，不能为普通测量、名称或结果变化重建模型或覆盖用户列宽。

`FlowGroupSnapshotCapture` 仅从实际复制的物流集合采集非空用户名称；`FlowGroupSnapshotPaste` 在单元、合流和入料汇流均完成重建后，统一使用物流 ID 映射恢复名称，不能仅遍历普通产品线。`GroupProductNameChecks.cpp` 覆盖实际表格命名、快捷键复制、源名称后改、画布与表格显示、撤销重做、项目往返及模板归一化；拓扑映射检查同时覆盖部分选区和各类汇流名称。

`MergeExtensionInteractionChecks.cpp` 随 `merge_test` 运行，验证从可用合流出料端拖到普通产品线，通过既有 `addProductToMerge` 将产品加入原合流；不增加嵌套合流或项目字段。交互层只识别并高亮候选产品，场景继续统一校验可用性和来源。检查还覆盖反向拖拽、名称/物流 ID/手动高度保留、空白及无效投放、占用线路保护和合流接入下游。三支路合流的复制、撤销重做、项目读写与模板归一化由 `GroupProductNameChecks.cpp` 覆盖。
