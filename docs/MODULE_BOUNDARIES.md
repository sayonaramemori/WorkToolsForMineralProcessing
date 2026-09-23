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

## 关键接口

- `FlowsheetDocument` 是持久业务状态的唯一来源；图元不能保存第二份测量数据。
- `CanvasTopologyBuilder` 是画布进入计算模型的唯一入口。
- `CalculationInputBuilder` 是文档测量转换成单组分求解输入的唯一入口。
- `OpenCircuitCalculator` 组装守恒方程；通用消元和加权最小二乘分别委托给数值求解器。
- `ProjectSerializer` 只编排文件 I/O、安全验证和恢复，JSON 读写与场景恢复分别由独立类负责。
- `MainWindow` 的实现按装配、画布命令、呈现/计算命令、项目生命周期、方案和导出命令拆分，不承载数值公式。

## 修改原则

1. 先扩展内层数据类型和测试，再向外连接服务与 UI。
2. 不允许 UI 根据图形位置推断拓扑或产品语义。
3. 不允许序列化器复制计算输入组装逻辑。
4. 几何变化只发出 `geometryChanged`；连接关系变化才发出 `topologyChanged`。
5. 新增持久字段必须提供旧版本缺省值并增加项目往返测试。
6. 新增求解约束必须覆盖严格、欠定、矛盾和数据协调模式。
