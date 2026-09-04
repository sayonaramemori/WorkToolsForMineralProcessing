# AutoFlotationSheet

完整操作说明见 [用户操作手册](docs/USER_GUIDE.md)。

使用纯 C++20、Qt 6 Widgets 和 xmake 开发的浮选工艺流程图编辑器。

## 当前功能

- 论文式浮选单元、入料线和左右产品线；
- 三产品浮选单元支持左、中、右三条独立产品物流，并参与质量及多组分守恒计算；
- 可添加一进二出的二分流器，指定左支路比例后自动计算右支路比例，并在质量与各组分守恒中按相同比例分流；
- 浮选单元宽度和连接长度调整；
- 终端产品箭头可通过 `Ctrl++` / `Ctrl+-` 独立调节长度，支持较大范围并随项目保存；
- 产品到入料连接、整体移动和 `Ctrl+B` 断开；
- 任意数量的普通产品或产品合并输出汇入同一条入料线，并以多输入汇合点形成开路合流或闭路回流；
- 两条或多条产品线合流、接入下一道浮选单元及拆分；
- 右侧数据表显示最顶层总入料（合计）以及全部最终和中间产品；可通过“组分设置”定义 Cu、Pb、Zn 等多个组分，实测物流分别录入质量和各组分品位，合计及中间产品在计算后显示反算值；
- 可选产品名称录入，并显示在画布产品箭头下方；
- 选中产品线、合流线或入料线后可右键添加药剂标注，支持双击编辑、`Delete` 删除，以及选中后用 `↑`/`↓` 调整垂直位置；
- 药剂标注支持显式的 `^` 上标、`_` 下标及花括号分组语法，普通数字保持原样以兼容药剂代号；
- 药剂标注可记录名称、用量、单位和备注；
- 合流产品进入下游时，自动要求一条合流支路实测值以消除欠定自由度；
- 使用广义线性方程组进行开路及多回流闭路的质量与多组分守恒反算，各组分独立计算品位和回收率，并识别唯一解、部分唯一解、欠定及矛盾数据；
- 终端合流产品可在中间支路欠定时先完成全流程平衡；可选填写各支路干质量占比和组分占比以完整反算；
- 物流及浮选单元结果详情；
- 可拖动画布结果标注；
- 可在画布上添加、拖动、双击编辑和删除自定义文字图层；
- 可通过“标注样式”统一调整结果标注和产品名称的字号、粗体及文字颜色；
- 鼠标滚轮缩放及中键拖动画布视图；
- 复杂回流支路支持独立调整横向走廊和汇入高度；选中具体回流时自动高亮该支路并弱化其他回流；
- 干质量、各组分品位、全流程产率和各组分回收率显示设置；
- 指标标签可在中文名称与论文符号 `m / β / γ / ε` 之间切换；
- 明暗主题及 SVG、PNG、JPEG 流程图导出；支持将全部方案和中间产品数据导出为 Excel 工作簿。
- 带版本校验的项目保存与导入，可恢复拓扑、闭路、实测数据和标注位置。
- 同一拓扑支持多套试验方案，方案间独立保存药剂制度、实测数据和计算结果，并支持复制、切换、重命名、删除及结果对比。

Python/PySide6 验证原型保存在 `python-prototype/`，当前产品代码位于 `src/`。

## 环境

### Linux

- GCC 或 Clang（支持 C++20）；
- Qt 6 开发包（Core、Gui、Widgets、Svg）；
- xmake。

以 Ubuntu/Debian 为例，Qt 依赖可通过系统包安装：

```bash
sudo apt install qt6-base-dev qt6-svg-dev
```

如果 `qmake6` 在 `PATH` 中，xmake 会自动发现 Qt；否则在配置时通过 `--qt=/path/to/Qt` 指定 Qt SDK 根目录。

### Windows

- Qt：`C:/Qt/6.11.1/mingw_64`
- MinGW：`C:/Qt/Tools/mingw1310_64`
- xmake：`C:/Users/13427/xmake/xmake.exe`

## 配置、构建和部署

### Linux

使用系统 Qt：

```bash
xmake f -p linux -m release
xmake
xmake run AutoFlotationSheet
```

使用独立安装的 Qt：

```bash
xmake f -p linux -m release --qt=/opt/Qt/6.8.0/gcc_64
xmake
```

生成程序为 `build/bin/AutoFlotationSheet`。`xmake deploy` 会将程序整理到 `build/deploy/bin/`；Linux 默认使用系统提供的 Qt 动态库。如需制作可分发的 AppImage 或私有运行时包，可在此目录结构上继续打包。

### Windows

```powershell
C:\Users\13427\xmake\xmake.exe f --qt="C:/Qt/6.11.1/mingw_64" -p mingw
C:\Users\13427\xmake\xmake.exe
C:\Users\13427\xmake\xmake.exe deploy
```

生成程序：`build/bin/AutoFlotationSheet.exe`。

构建前请关闭正在运行的程序，否则 Windows 会锁定 EXE，链接器无法覆盖。`xmake deploy` 使用匹配版本的 `windeployqt` 强制刷新 Qt DLL，避免入口点冲突。

## 测试

Linux：

```bash
xmake build topology_test
xmake build connection_test
xmake build merge_test
xmake build terminal_input_test
xmake build canvas_view_test
xmake build export_test
xmake build excel_export_test
xmake build closed_loop_test
xmake build project_io_test
QT_QPA_PLATFORM=offscreen xmake run connection_test
QT_QPA_PLATFORM=offscreen xmake run merge_test
QT_QPA_PLATFORM=offscreen xmake run terminal_input_test
QT_QPA_PLATFORM=offscreen xmake run canvas_view_test
QT_QPA_PLATFORM=offscreen xmake run export_test
xmake run excel_export_test
QT_QPA_PLATFORM=offscreen xmake run closed_loop_test
QT_QPA_PLATFORM=offscreen xmake run project_io_test
```

Windows：

```powershell
C:\Users\13427\xmake\xmake.exe build topology_test
C:\Users\13427\xmake\xmake.exe build connection_test
C:\Users\13427\xmake\xmake.exe build merge_test
C:\Users\13427\xmake\xmake.exe build terminal_input_test
C:\Users\13427\xmake\xmake.exe build canvas_view_test
C:\Users\13427\xmake\xmake.exe build export_test
C:\Users\13427\xmake\xmake.exe build excel_export_test
C:\Users\13427\xmake\xmake.exe build closed_loop_test
C:\Users\13427\xmake\xmake.exe build project_io_test
```

GUI 测试在无界面环境运行时设置：

```powershell
$env:PATH="C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;$env:PATH"
$env:QT_QPA_PLATFORM="offscreen"
```

测试职责：

- `topology_test`：纯拓扑校验、排序和三级开路反算；
- `connection_test`：产品连接、组件移动、长度调整和断开；
- `merge_test`：产品合流几何和拆分；
- `terminal_input_test`：终端输入、主题、计算服务、详情表和标注交互。
- `canvas_view_test`：滚轮缩放边界、鼠标中键平移，以及药剂标注方向键优先级。
- `export_test`：SVG、PNG 和 JPEG 文件生成及格式校验。
- `excel_export_test`：多方案 XLSX 数据、产品名称回退及文件结构校验。
- `closed_loop_test`：回流汇合、闭路拓扑识别、断开及计算保护。
- `project_io_test`：开闭路项目数据往返、尺寸与标注恢复、损坏文件保护。

## 常用操作

- 顶部工具栏按“项目管理”“流程编辑”“方案设置”“显示设置”“导出选项”分组为下拉菜单；当前试验方案仍可直接通过下拉框切换；
- “试验方案”下拉框：切换当前药剂制度和实验数据组；
- “新增方案”：创建空白数据组；“复制方案”：基于当前方案创建副本；
- “组分设置”：每行输入一个组分名称；右侧表格会为每种组分生成品位列，合流支路还会生成对应的组分占比列；
- “方案对比”：选择任一最终或中间产品，横向比较各已计算方案的质量、产率以及每种组分的品位和回收率；
- `Ctrl + +` / `Ctrl + -`：调整选中浮选单元宽度或连接长度；
- `Ctrl + B`：断开连接或拆分合流；
- 将产品箭头拖到同一流程中的上游入料线：建立回流闭路；
- 将产品合并线末端箭头拖到新浮选单元的入料线：将合流产品送入下一道流程；
- `Ctrl + Shift + T`：切换明暗主题；
- `Ctrl + S`：保存当前项目；`Ctrl + O`：导入已有项目；
- 鼠标滚轮：以指针位置为中心缩放画布；
- 按住鼠标中键拖动：平移画布视图；
- 选中药剂标注后按 `↑` / `↓`：上下微调 2 个画布单位；按住 `Shift` 时每次调整 10 个画布单位；
- “显示指标”：整体显示或隐藏结果标注；
- “指标设置”：选择标注显示的计算指标；
- “导出”：选择 SVG、PNG 或 JPEG 格式保存完整流程图及当前可见标注。
- “导出Excel”：按方案分组导出总入料（合计）及全部最终、中间产品的名称、重量、产率，以及每种组分的品位和回收率；未填写产品名称时使用默认物流名称。
- “Excel顺序”：拖动调整总入料、最终产品和中间产品在工作簿中的顺序；可恢复为流程顺序，新物流自动追加到末尾。

## 闭路操作

将尚未连接的普通产品箭头或产品合并后的输出箭头拖到目标入料线上，入料线高亮后松开，即可创建入料汇合；可以继续向同一位置拖入更多物流，来源数量不设固定上限。同一连通流程内的上游连接会作为回流闭路处理。目标入料既可以是未连接的新鲜入料，也可以已经连接上一段产品或合流产品；已连接的入料线虽然平时隐藏，原位置仍保留投放热区，拖入时会临时显示并高亮。程序会保留原正常上游物流并增加汇合点。拖动产品合并输出时，应从合并线末端的箭头处开始。各输入线沿浮选单元外侧错开布置。选中汇合线或汇合点后按 `Ctrl+B` 可整体断开，原正常上游连接会自动恢复；各附加来源重新成为可连接物流。

闭路建立后，“物流实测参数”面板会自动增加所有回流环所需的数据行：普通产品回流填写该回流物流的干质量和品位；产品合并输出回流填写合并前各回流支路的数据，合并后的总回流由守恒关系自动计算。同一主流程可以同时包含多个独立或嵌套回流环。填写全部终端产品和回流支路后即可计算，程序会联动反算内部物流以及唯一的外部新鲜入料。

如果缺少回流实测值，闭路方程通常不唯一，计算结果会报告数据不足。全流程产率和回收率始终以反算得到的外部新鲜入料为基准。

## 导出说明

- Excel 数据导出生成标准 `.xlsx` 工作簿，第一列按方案合并；未计算方案和没有计算结果的产品物料行会自动忽略；
- Excel 行顺序是项目共享设置，所有方案使用相同顺序；被删除物流自动忽略，新增物流自动追加；

- SVG 为矢量格式，适合论文排版和后续编辑；
- PNG 为无损栅格格式，适合文档和演示文稿；
- JPEG 以 95% 品质输出，适合需要较小文件体积的场景；
- PNG/JPEG 可选择 1×、2×、3× 或 4× 输出倍率，默认 2×；文字较多时建议使用 3× 或 4×；
- 三种格式均按所有画布图元的实际范围导出，并在四周保留 30 个画布单位的空白；
- 导出统一使用白色背景。暗色主题下导出时，程序会临时按明亮主题绘制，完成后恢复界面主题；
- PNG/JPEG 会限制最长边和总像素数，超大画布会等比例缩小以控制内存占用。

详细模块边界见 [架构说明](docs/ARCHITECTURE.md)，开发流程见 [开发指南](docs/DEVELOPMENT.md)，标注扩展见 [标注系统](docs/ANNOTATIONS.md)。
