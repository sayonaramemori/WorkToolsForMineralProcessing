# 模块职责与依赖边界

本文是新增代码的放置准则。目标是让业务状态、数值计算、画布表现和应用编排可以分别修改、分别测试。

## 依赖层级

```text
app
 ├─ ui
 ├─ annotations
 └─ services
      └─ adapters
           └─ editor / graphics

ui ─────────► document / topology / editor（视图复用与属性值类型）
annotations ─► document / editor / graphics
document ────► core / topology / annotation value types
editor ──────► graphics
graphics ────► core / editor scene notifications
topology ────► QtCore
```

`topology` 是最内层的计算模型，不能依赖画布、文档或 UI。`app` 是最外层，只组合用例和显示消息。

## 代码归属判断

| 问题 | 所属模块 |
|---|---|
| 是否随项目或方案保存？ | `document` |
| 纯质量守恒、秩、协调算法？ | `topology` |
| 从画布关系生成计算图？ | `adapters` |
| 创建、连接、断开画布图元？ | `editor` |
| 图元几何、命中、绘制？ | `graphics` |
| 表格、对话框、停靠面板？ | `ui` |
| 项目保存、导入、导出、计算用例？ | `services` |
| 菜单装配、复制类型分派、跨模块信号连接？ | `app` |
| 结果来源边框、线宽等本机显示偏好？ | `annotations`，通过 `QSettings` 保存 |

## 关键接口

- `FlowsheetDocument` 是持久业务状态的唯一来源；图元不能保存第二份测量数据。
- `CanvasTopologyBuilder` 是画布进入计算模型的唯一入口。
- `CalculationInputBuilder` 是文档测量转换成单组分求解输入的唯一入口。
- `OpenCircuitCalculator` 负责输入、物理解校验和结果编排；内部 `BalanceEquationSolver` 组装标量守恒方程，通用消元和加权最小二乘分别委托给数值求解器。
- `ProjectSerializer` 只编排文件 I/O、安全验证和恢复，JSON 读写与场景恢复分别由独立类负责；设备类型键只在 `ProjectUnitKindCodec` 中定义，不与显示名称或设备绘制混用。
- `MainWindowActions.cpp` 负责工具栏编排，`MainWindowFlowActions.cpp` 负责流程命令和快捷键，`MainWindowEquipmentActions.cpp` 负责设备菜单，`MainWindowDisplayActions.cpp` 负责显示设置；`MainWindow` 的其他实现按装配、画布命令、呈现命令、结果同步/计算命令、项目生命周期、方案和导出命令拆分，不承载数值公式。
- `ExcelWorksheetBuilder` 负责有序数据到工作表 XML 的转换；`XlsxWorkbookWriter` 只打包单工作表 XLSX，不依赖流程、文档、画布或表格控件。
- `ExcelDataExporter::namedStreams()` 统一筛选和排序用户已命名的物流，Excel 写出与排序对话框共用该入口；`NameColumn` 的定位信息不能作为 `ProductNameColumn` 或导出名称的回退。
- 画布复制粘贴由 `app/MainWindowClipboard` 按选中对象和最近复制类型分派；单条药剂的右键及快捷键复制由 `services/ReagentClipboard` 保存值快照，`app/MainWindowReagents` 校验目标并提交文档与撤销边界，`annotations/AnnotationContextTarget` 按光标识别目标；汇流图元提供命中查询并记录单击选中的标注物流，不改变拓扑。摆放与选中属于标注管理器，排序规则由 `annotations/ReagentOrder` 统一供绘制和快照采集使用。
- `CanvasClipboardKind`、药剂值快照和汇流的 `selectedAnnotationStreamId()` 都属于会话状态，不放入文档或项目 JSON。回流的 `selectedSourceStreamId()` 只服务于路线高亮和断开来源，不能作为所有类型物流的药剂粘贴目标。药剂粘贴只修改当前方案标注，不能使计算结果失效。
- `FlowGroupClipboard` 只管理值快照和粘贴次数；`FlowGroupSnapshotCapture` 采集选择，`FlowGroupSnapshotPaste` 从只读快照重建副本。快照保存独立的单元、文字和当前方案药剂；药剂以稳定物流 ID 重映射，不能保留原图元指针，也不能重复叠加场景偏移。
- `FlotationUnitItem::inputAnchorScenePosition()` 与 `ProductLineItem::sourceAnchorScenePosition()` 提供实际端口位置；球磨机的圆顶入料、圆底出料不能由左右侧别或设备宽度重新推导。

## 已落实的职责边界

| 职责 | 实现入口 | 约束 |
|---|---|---|
| 主题、状态与日志 | `MainWindowPresentation.cpp` | 不构建自定义编辑窗口 |
| 计算命令与结果联动 | `MainWindowResults.cpp` | 委托计算服务，更新状态与诊断并处理矛盾时的跳转，不实现公式 |
| 样式与排序编辑窗口 | `ui/AnnotationTextStyleDialog.*`、`ui/StreamOrderDialog.*` | 接收值、返回可选值，不读取或修改文档 |
| 剪贴板状态、采集与重建 | `FlowGroupClipboard.*`、`FlowGroupSnapshotCapture.cpp`、`FlowGroupSnapshotPaste.cpp` | 以 `FlowGroupSnapshot.h` 的值类型交接，重建只读快照 |
| 画布复制分派与单条药剂命令 | `MainWindowClipboard.cpp`、`MainWindowReagents.cpp`、`ReagentClipboard.*` | 复用画布快捷键及右键命令，按类型分派；快照不持有图元，每次药剂粘贴独立撤销 |
| 右键目标、菜单与药剂摆放 | `AnnotationContextTarget.*`、`AnnotationManagerContextMenu.cpp`、`AnnotationManagerReagentPlacement.cpp` | 按本次右键位置识别唯一物流；只管理显示和交互，不创建另一套拓扑或剪贴板 |
| 合流状态、路线、交互与命中 | `MergeJunctionItem.cpp`、`MergeJunctionItemRouting.cpp`、`MergeJunctionItemInteraction.cpp`、`MergeJunctionItemHitTest.cpp` | 保留同一图元与头文件，事件委托场景修改拓扑；位置查询只返回唯一物流 ID |
| 入料汇流命中 | `FeedJunctionItemHitTest.cpp` | 区分回流、主入料、外部入料和输出；共用位置不猜测目标 |
| 表格装配与组分编辑 | `TerminalProductDockLayout.cpp`、`TerminalProductDockComponents.cpp` | 布局不求解、不持久化；组分编辑由文档统一提交 |
| 计算说明 | `CalculationExplanationDialog.*` | 只接收结果值与计算模式，不能访问文档、场景或计算服务 |
| 标注文档状态 | `FlowsheetDocumentAnnotations.cpp` | 统一分派当前方案文字/药剂及共享结果标注，不复制 UI 状态 |
| 自由文字编辑器 | `AnnotationItemEditor.cpp` | 只编辑传入记录，调用方决定提交；绘制和图元事件留在原文件 |
| 项目类型编码 | `ProjectUnitKindCodec.*` | 读写共用当前格式键表，不处理中文名称或旧格式迁移 |

表格列布局由 `TerminalProductDockColumns.cpp` 管理：只读视口尺寸，调整列宽；初始分配与用户拖动区分处理，不写入文档或项目文件。筛选仅保留物流类别及选中流程的一跳关联，移除的关注对象按钮不保留隐藏筛选状态。

图元的绘制上下文与选中态留在 `FlotationUnitItemPainting.cpp`，设备符号实现位于 `graphics/symbols`，只接收设备值与画笔，不持有场景或文档。表格角色呈现位于 `TerminalProductTableModelData.cpp`，文档编辑和模型通知留在 `TerminalProductTableModel.cpp`，两者共享同一模型及 MOC 头。`core` 与 `topology` 不依赖 UI、场景或文件服务。

## 工作区工具接口

- 属性数值与目标校验属于 `editor/CanvasProperties`；`ui/ObjectPropertiesDock` 不持有图元指针，不直接修改场景；负责数值实时输入、编辑器原位刷新和本机增减步长偏好。
- 问题条目转换属于 `services/CalculationDiagnostics`，定位协调属于 `app/MainWindowDiagnostics`，输入聚焦属于 `TerminalProductDock`。求解器不依赖问题面板。
- 本机模板文件属于 `services/FlowTemplateStore`，模板列表及名称输入属于 `ui/FlowTemplateDock`。`app/FlowTemplatePreviewSession` 拥有预览专用文档、场景和标注管理器，`ui/FlowTemplatePreview` 负责只读查看和适应窗口；预览不得操作当前项目、ID 分配、剪贴板或撤销栈。模板的项目片段必须经当前项目序列化接口校验，插入复用只读快照和新 ID 映射。
- `ui/WorkspaceSidebar` 只管理右侧上方四个页面的可见性、供显示设置菜单和顶部下拉框共用的页面注册表及互斥切换动作，以及底部固定日志的 7:1 布局，不操作文档；需要跳转时由 `app` 显式选择页面。`ui/WorkspacePanelSelector` 从注册表生成选项，通过 `showPanel()` 请求切换并监听 `panelChanged` 同步；滚轮处理属于该控件，不写入主窗口或画布事件。切换状态和本机模板库不属于项目业务状态；属性提交与模板插入才通过既有文档/场景信号产生项目变更和撤销记录。

已撤销 UI 的专用目录、固定取样清单和隐藏按钮不留在运行时路径中。指标菜单直接使用 `QMenu`；计算诊断从计算图识别节点及汇流角色；线路右键由标注管理器统一处理。Qt 原生停靠显隐动作仍须禁用，防止绕过工作区的互斥页面管理，这与旧按钮功能不同。

## 修改原则

1. 先扩展内层数据类型和测试，再向外连接服务与 UI。
2. 不允许 UI 根据图形位置推断拓扑或产品语义。
3. 不允许序列化器复制计算输入组装逻辑。
4. 几何变化只发出 `geometryChanged`；连接关系变化才发出 `topologyChanged`。
5. 项目保存仅使用 v24，读取兼容 v23 和 v24；v23 到 v24 的文字归属转换仅放在 `ProjectJsonReader` 入口，不让兼容分支进入文档或其他模块。新增持久字段时同步更新读写器、格式版本和测试样例，并增加项目往返及格式拒绝测试。
6. 新增求解约束必须覆盖严格、欠定、矛盾和数据协调模式。
7. 本机显示偏好不写入 `FlowsheetDocument` 或项目快照；切换来源区分或线宽不得触发项目变更、计算失效或撤销记录。
8. 设备图标下方不绘制名称和说明小字；产品名称默认留空，定位描述、用户名称和设备内部标签分别处理。
9. 同一类按职责拆成多个实现文件时保留统一头文件，Qt MOC 头只登记一次；显式链接源码的测试目标必须同时收集全部相关实现，不能依赖主程序已构建或残留对象文件。

维护验证使用 `python3 scripts/validate.py --clean`：脚本从 `xmake.lua` 发现测试目标，通过 xmake 提供 Qt 运行环境，并将逐项日志与退出码保存到忽略的构建目录。不要在脚本中复制测试目标清单或 Qt SDK 本机路径。
