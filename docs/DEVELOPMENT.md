# 开发指南

## 开发原则

1. 先确定数据归属，再添加界面。
2. 计算公式只放在 `src/topology`。
3. 画布到计算模型的转换只放在 `src/adapters`。
4. `MainWindow` 只装配模块，不新增大段领域逻辑。
5. 新交互必须补充相应测试。

## 新功能检查清单

- 是否需要稳定 ID？
- 数据是否应保存在 `FlowsheetDocument`？
- 拓扑变化和几何变化是否被正确区分？
- 修改输入后旧结果是否失效？
- 明暗主题是否都可读？
- SVG、PNG、JPEG 导出是否都应包含该对象？
- 是否覆盖连接、断开、合流和重算场景？

## Qt 约定

- 含 `Q_OBJECT` 的头文件必须加入对应 xmake target 的 `add_files`，否则不会生成 MOC。
- 图元颜色读取当前 `QApplication::palette()`，主题切换后显式刷新。
- 不在 `paint()` 中修改业务状态。
- 结果标注拖动和药剂标注键盘微调都保存相对锚点的偏移，不保存锚定标注的绝对位置。
- 测试目标只链接所需源码，避免把 `main.cpp` 链接进去。

## 添加计算指标

1. 在 `TopologyTypes.h` 中确定结果结构。
2. 在 `OpenCircuitCalculator` 中计算并测试。
3. 在 `ResultDetailsView` 中决定详细显示。
4. 如需画布显示，扩展 `ResultMetric` 和 `AnnotationContentFormatter`。
5. 在 `ResultMetricMenu` 中加入入口。

## 多组分数据

- 组分定义属于项目共享状态，使用稳定 `ComponentDefinition::id`；修改显示名称不能改变 ID。
- `StreamMeasurement::gradePercents` 和 `componentSharePercents` 按组分 ID 保存，质量仍只保存一次。
- `FlowsheetCalculationService` 对每个组分调用一次守恒计算器，结果写入 `CalculationResult::components`；第一个组分同时保留在旧结果字段中作为兼容层。
- 右侧输入表、结果详情、方案对比和 XLSX 导出必须遍历组分定义，不得假设只有一个品位列。
- 项目格式 v2 保存组分定义和按组分索引的数据；读取器继续接受 v1，并将旧项目中的 `gradePercent` 和 `componentSharePercent` 自动迁移到 `component-1`。

## 添加药剂标注

1. 药剂记录属于 `ExperimentScenario::reagentAnnotations`，不得放入项目共享状态。
2. 使用 `AnnotationKind::Reagent` 和 `AnnotationOwnerKind::Stream`，所有者使用稳定物流 ID。
3. 名称、用量、单位和备注分别保存；显示拼接及化学式排版放在 `ReagentAnnotationItem`。
4. 锚点解析和增删生命周期由 `AnnotationManager` 负责。
5. 药剂位置调整统一调用 `AnnotationManager::nudgeSelectedReagents()`；不要在图元或场景过滤器中再次处理方向键。
6. 补充方案切换、项目往返、主题和导出测试。

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

- 普通连接、产品合流横杆和回流走廊分别保存最小人工段参数，不保存由端点推导出的完整 `QPainterPath`；
- 路线拖动按 10 个画布单位吸附，完成后发出 `geometryChanged`，不得使计算结果失效；
- 右键菜单由 `AnnotationManager` 的场景事件过滤器统一构建，新增线路命令必须合并到该菜单，避免图元菜单被过滤器截获；
- 跨线桥在 `FlowsheetScene::drawForeground` 中动态绘制，只处理不同物流图元的水平—垂直内部交点，不得修改拓扑；
- 项目格式 v4 保存 `routeY`、`mergeY` 和各入料来源的 `routeX`，载入器继续接受不含这些字段的 v1–v3 项目；
- 修改布线路径后运行 `connection_test`、`merge_test`、`project_io_test` 和导出测试。

## 闭路连接

- 同一连通分量内的产品到入料连接视为回流，不能按开路连接自动移动目标单元；
- 入料与回流的画布表现放在 `FeedJunctionItem`：正常入料可以是外部新鲜入料、`ProductLineItem` 或 `MergeJunctionItem`，附加来源使用产品与合流输出集合，拓扑中统一转换为支持两条及以上输入的 `MergeNode`；
- 项目格式 v3 使用 `feedJunctions[].sources` 数组保存多输入端点；载入器必须继续接受 v1/v2 的单一 `sourceType/source` 字段；
- 将回流接入已占用入料时，汇合点必须保存原正常上游端点；断开回流及项目重新载入后都必须恢复该连接；
- 已占用入料图元虽然隐藏，其几何线段必须继续作为拖放热区；命中后临时显示高亮，并由 `closed_loop_test` 覆盖；
- 同一主流程允许存在多个独立或嵌套的 `FeedJunctionItem`；连通分量遍历必须覆盖每个汇合点的正常来源及全部附加来源；
- 多回流项目保存/载入由 `project_io_test` 覆盖，载入后汇合点 ID、外部入料数量和闭路检测结果必须保持一致；
- 产品合并输出接入回流后必须从终端产品列表移除，断开后恢复；
- `Cycle` 表示已经识别到闭路，不再作为拓扑结构错误；
- 普通回流将回流产品加入 `requiredMeasurements`；产品合并输出回流将两条合并前支路加入，确保方程可唯一求解；
- 闭路计算必须覆盖回流、内部入料、外部新鲜入料以及全流程指标；缺少撕裂流实测值时应返回 `Underdetermined`；
- 求解器必须从完整方程组判断秩，不能依赖节点顺序进行局部传播；新增约束类型时分别为干质量和组分质量生成方程，并保留矛盾、欠定和非物理解诊断；
- `topology_test` 必须包含至少一个没有局部传播起点、只能通过联立消元求解的闭路；
- 修改回流连接、断开或路由规则后运行 `closed_loop_test`。

## 导出格式

- 矢量格式由 `SvgExporter` 负责，栅格格式由 `RasterExporter` 负责；
- 各格式使用一致的场景范围、留白、背景和可见性规则；
- PNG/JPEG 导出倍率必须作用于实际渲染尺寸，不能在渲染后拉伸；同时限制最长边和总像素数，避免大画布造成过高内存占用；
- XLSX 数据导出由 `ExcelDataExporter` 负责，产品范围来自 `productStreams`，不得只导出终端产品；没有计算结果的产品物料行应直接忽略；
- XLSX 排序读取 `FlowsheetDocument::exportStreamOrder()`；先输出仍存在的已配置物流，再按流程顺序追加新物流。总入料不做固定位置处理；
- 新增或修改格式后扩展 `export_test`，校验文件可生成且格式可读取。
- 修改 Excel 导出时运行 `excel_export_test`，校验 OOXML 文件结构、中文方案名和产品名称回退。

## 发布前验证

Linux：

```bash
xmake f -p linux -m release
xmake
xmake build topology_test
xmake build connection_test
xmake build merge_test
xmake build terminal_input_test
xmake build canvas_view_test
xmake build export_test
xmake build closed_loop_test
xmake build project_io_test
xmake deploy
```

Windows：

```powershell
C:\Users\13427\xmake\xmake.exe
C:\Users\13427\xmake\xmake.exe build topology_test
C:\Users\13427\xmake\xmake.exe build connection_test
C:\Users\13427\xmake\xmake.exe build merge_test
C:\Users\13427\xmake\xmake.exe build terminal_input_test
C:\Users\13427\xmake\xmake.exe build canvas_view_test
C:\Users\13427\xmake\xmake.exe build export_test
C:\Users\13427\xmake\xmake.exe build closed_loop_test
C:\Users\13427\xmake\xmake.exe build project_io_test
C:\Users\13427\xmake\xmake.exe deploy
```
