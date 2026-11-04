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

ui ─────────► document / topology
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
| 菜单装配、跨模块信号连接？ | `app` |
| 结果来源边框、线宽等本机显示偏好？ | `annotations`，通过 `QSettings` 保存 |

## 关键接口

- `FlowsheetDocument` 是持久业务状态的唯一来源；图元不能保存第二份测量数据。
- `CanvasTopologyBuilder` 是画布进入计算模型的唯一入口。
- `CalculationInputBuilder` 是文档测量转换成单组分求解输入的唯一入口。
- `OpenCircuitCalculator` 负责输入、物理解校验和结果编排；内部 `BalanceEquationSolver` 组装标量守恒方程，通用消元和加权最小二乘分别委托给数值求解器。
- `ProjectSerializer` 只编排文件 I/O、安全验证和恢复，JSON 读写与场景恢复分别由独立类负责；设备类型键只在 `ProjectUnitKindCodec` 中定义，不与显示名称或设备绘制混用。
- `MainWindowActions.cpp` 负责菜单和快捷键装配；`MainWindow` 的其他实现按装配、画布命令、呈现命令、结果同步/计算命令、项目生命周期、方案和导出命令拆分，不承载数值公式。
- `ExcelDataExporter::namedStreams()` 统一筛选和排序用户已命名的物流，Excel 写出与排序对话框共用该入口；`NameColumn` 的定位信息不能作为 `ProductNameColumn` 或导出名称的回退。
- `FlowGroupClipboard` 只管理值快照和粘贴次数；`FlowGroupSnapshotCapture` 采集选择，`FlowGroupSnapshotPaste` 从只读快照重建副本。快照保存独立的单元、文字和当前方案药剂；药剂以稳定物流 ID 重映射，不能保留原图元指针，也不能重复叠加场景偏移。
- `FlotationUnitItem::inputAnchorScenePosition()` 与 `ProductLineItem::sourceAnchorScenePosition()` 提供实际端口位置；球磨机的圆顶入料、圆底出料不能由左右侧别或设备宽度重新推导。

## 已落实的职责边界

| 职责 | 实现入口 | 约束 |
|---|---|---|
| 主题、状态与日志 | `MainWindowPresentation.cpp` | 不构建自定义编辑窗口 |
| 计算命令与结果联动 | `MainWindowResults.cpp` | 委托计算服务和结果面板，不实现公式 |
| 样式与排序编辑窗口 | `ui/AnnotationTextStyleDialog.*`、`ui/StreamOrderDialog.*` | 接收值、返回可选值，不读取或修改文档 |
| 剪贴板状态、采集与重建 | `FlowGroupClipboard.*`、`FlowGroupSnapshotCapture.cpp`、`FlowGroupSnapshotPaste.cpp` | 以 `FlowGroupSnapshot.h` 的值类型交接，重建只读快照 |
| 合流状态、路线与交互 | `MergeJunctionItem.cpp`、`MergeJunctionItemRouting.cpp`、`MergeJunctionItemInteraction.cpp` | 保留同一图元与头文件，事件委托场景修改拓扑 |
| 项目类型编码 | `ProjectUnitKindCodec.*` | 读写共用当前格式键表，不处理中文名称或旧格式迁移 |

设备绘制集中在 `FlotationUnitItemPainting.cpp`，表格单元格呈现与编辑集中在 `TerminalProductTableModel.cpp`；两者各自具有完整职责。`core` 与 `topology` 不依赖 UI、场景或文件服务。

## 修改原则

1. 先扩展内层数据类型和测试，再向外连接服务与 UI。
2. 不允许 UI 根据图形位置推断拓扑或产品语义。
3. 不允许序列化器复制计算输入组装逻辑。
4. 几何变化只发出 `geometryChanged`；连接关系变化才发出 `topologyChanged`。
5. 项目仅支持当前格式；新增持久字段时同步更新读写器、格式版本和测试样例，并增加项目往返及格式拒绝测试，不添加旧版本迁移或字段回退。
6. 新增求解约束必须覆盖严格、欠定、矛盾和数据协调模式。
7. 本机显示偏好不写入 `FlowsheetDocument` 或项目快照；切换来源区分或线宽不得触发项目变更、计算失效或撤销记录。
8. 设备图标下方不绘制名称和说明小字；产品名称默认留空，定位描述、用户名称和设备内部标签分别处理。
9. 同一类按职责拆成多个实现文件时保留统一头文件，Qt MOC 头只登记一次；显式链接源码的测试目标必须同时收集全部相关实现，不能依赖主程序已构建或残留对象文件。
