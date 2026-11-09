# 当前架构检查与职责调整

项目采用 `app → ui / annotations / services → adapters → editor / graphics` 的组织方式，`document` 保存业务状态，`topology` 独立完成求解。代码放置准则见[模块职责与依赖边界](MODULE_BOUNDARIES.md)，调用链见[架构说明](ARCHITECTURE.md)，验证命令见[开发指南](DEVELOPMENT.md)。

## 按修改内容定位实现

| 修改内容 | 实现入口 | 边界 |
|---|---|---|
| 工具栏与菜单 | `MainWindowActions.cpp`、`MainWindowFlowActions.cpp`、`MainWindowEquipmentActions.cpp`、`MainWindowDisplayActions.cpp` | 主窗口只连接用例，不实现求解或持久化细节 |
| 表格顶部与主表布局 | `TerminalProductDockLayout.cpp` | 保持三行摘要卡，主表占满剩余区域；不求解或修改文档 |
| 表格列、筛选与选中行 | `TerminalProductDockColumns.cpp`、`TerminalProductDockSelection.cpp`、`TerminalProductViewSupport.cpp` | 列宽、筛选和焦点属于视图状态 |
| 表格对象及信号 | `TerminalProductDock.cpp` | 初始化对象、连接信号、分派键盘事件和刷新主题 |
| 组分名称编辑 | `TerminalProductDockComponents.cpp` | 编辑结果统一写入文档，模型再根据组分信号重建列 |
| 计算状态与说明 | `TerminalProductDockResults.cpp`、`CalculationExplanationDialog.*` | `CalculationController` 统一选取和缓存预览；面板只显示状态，弹窗只显示传入值 |
| 表格会话 | `TerminalTableSession`、`TerminalTableController` | 应用层查询/校验和写入；表格只读快照与提交类型化命令，不持有可写文档 |
| 图元到场景的请求 | `CanvasGeometryHost`、`CanvasConnectionHost` | 图元依赖契约，场景实现契约；不反向引用 editor，保留同步移动和连接时序 |
| 单元格内容与编辑 | `TerminalProductTableModelData.cpp`、`TerminalProductTableModel.cpp` | 角色与表头呈现和命令映射分开，同一模型与 MOC 头 |
| 方案标注及显示设置 | `FlowsheetDocumentAnnotations.cpp` | 自定义文字、药剂按方案独立；结果标注位置、全局显示设置由项目共享 |
| 文字绘制与编辑 | `AnnotationItem.cpp`、`AnnotationItemEditor.cpp` | 图元渲染/事件与编辑窗口分开，保持原调用入口；编辑记录由调用方提交 |
| 标注右键及药剂摆放 | `AnnotationContextTarget.*`、`AnnotationManagerContextMenu.cpp`、`AnnotationManagerReagentPlacement.cpp` | 根据光标识别唯一物流，统一菜单和摆放，不创建第二份拓扑或剪贴板 |
| 复制分派与值快照 | `MainWindowClipboard.cpp`、`MainWindowReagents.cpp`、`FlowGroupSnapshotCapture.cpp`、`FlowGroupSnapshotPaste.cpp`、`ReagentClipboard.*` | 快照不持有图元，粘贴映射新 ID 并写入当前方案，每次药剂粘贴独立撤销 |
| 设备符号与线路命中 | `graphics/symbols`、`FeedJunctionItemHitTest.cpp`、`MergeJunctionItemHitTest.cpp` | 绘制只读设备值；交点不能猜测药剂目标物流 |
| 计算问题与导航 | `CalculationDiagnostics.cpp`、`MainWindowDiagnostics.cpp`、`MainWindowResults.cpp` | 条目转换、画布/表格定位和计算命令分别处理；仅错误级守恒矛盾自动跳转 |
| Excel 导出 | `ExcelDataExporter.cpp`、`ExcelWorksheetBuilder.*`、`XlsxWorkbookWriter.*` | 已命名物流筛选/排序、工作表 XML、XLSX 打包分别处理 |
| 模板预览 | `FlowTemplateStore.*`、`FlowTemplatePreviewSession.*`、`FlowTemplatePreview.*` | 本机存储、独立场景生命周期和只读查看分别处理 |
| 右侧页面及日志 | `WorkspaceSidebar.*`、`WorkspacePanelSelector.*` | 页面注册和互斥可见性统一管理；顶部滚轮选择复用注册表；底部日志保持八分之一高度 |
| 计算数据与图元定位 | `FlowsheetSnapshot`、`StreamDescriptor`、`CanvasItemIndex` | 计算/导出只读纯值；图元索引仅由应用层按需构建，不进入 UI 模型或求解缓存 |
| 计算预览 | `app/CalculationController.*` | 合并输入通知、缓存预览；筛选及选择不触发求解，预览不修改持久状态 |
| 测试装配与执行 | `xmake.lua`、`scripts/validate.py` | 共用源码/MOC 集合；脚本从构建文件发现测试，避免维护第二份目标清单 |

## 保持的行为

- 表格面板保留近期确认的紧凑布局和现有配色，物流结果小表格及其专用代码已移除。
- 自定义文字随方案复制，之后编辑、移动和删除相互独立；新增空白方案不继承文字。
- 项目保存使用 v24，读取支持 v23/v24；v23 共享文字仅在读取入口转换为方案文字，其他模块不接触旧格式。
- 计算后欠定或协调大残差警告保留当前页面，错误级 `InconsistentBalance` 才自动显示问题；显式查看问题仍会打开对应页面。
- Excel 仅导出用户命名且有可用计算结果的产品；定位描述不能作为名称回退。
- `CanvasTopologyBuilder`、`CalculationInputBuilder` 和 `ProjectSerializer` 仍分别是画布转图、测量转求解输入、项目读写的统一入口。
- 不保留已删除的关注对象筛选、隐藏指标按钮、备用线路右键菜单或结果小表格的运行时代码。

## 拆分后的验证入口

```bash
python3 scripts/validate.py --clean
```

脚本完成全目标干净构建、全部 17 个检查和 v23/v24 项目启动/退出检查。结果和逐项日志位于 `build/validation/latest/`，失败会返回非零退出码。脚本默认使用 Qt offscreen 平台，通过 xmake 获取配置后的 Qt 运行环境，不包含开发者机器的 SDK 路径。

工作区属性与诊断检查分别位于 `ObjectPropertyChecks.cpp`、`CalculationDiagnosticsChecks.cpp`；原有断言与执行顺序保留。终端输入、项目读写及剪贴板等现有检查继续覆盖界面、方案隔离、撤销、复制、布线与持久化。

同一 Qt 类拆为多个实现文件时不新增 MOC 头；新增独立 UI 组件若不需要信号则保持普通值接口。`terminal_input_test`、`project_io_test` 等按需链接目标同步登记所有拆出的实现，完整应用目标复用 `add_application_sources()`。

新增无画布计算服务检查与预览控制器交互检查，依赖边界和场景生命周期由构建及回归验证。

统一验证现包含 17 项检查及 v23/v24 启动检查；表格会话、计算服务可在没有 Qt Widgets 的环境下验证。逐项日志与退出码见 `build/validation/latest/`。
