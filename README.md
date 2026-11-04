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
- `Ctrl+Z` 撤销以及 `Ctrl+Y` / `Ctrl+Shift+Z` 重做项目编辑；
- “项目管理”菜单支持保存、项目另存为、导入和最近项目快速切换，另存成功后自动切换当前项目路径；
- 支持 `Ctrl+N` 新建空白项目，以及删除一个或多个选中单元并用 `Ctrl+Z` 恢复；
- 任意数量的普通产品或产品合并输出汇入同一条入料线，并以多输入汇合点形成开路合流或闭路回流；
- 两条或多条产品线合流、接入下一道浮选单元及拆分；
- 右侧数据表显示最顶层总入料（合计）以及全部最终和中间产品；任意物流均可录入绝对干质量以及各组分品位，并可按关注的浮选单元/汇流节点和物流类别组合筛选；是否足够由方程秩动态判断，例如二产品流程可用“入料 + 任一产品”或“两产品”反算其余物流；
- 产品名称默认留空，可在右侧表格填写并显示在画布产品箭头下方；仅已命名产品参与 Excel 导出；
- 选中产品线、合流线或入料线后可右键添加药剂标注，支持双击编辑、`Delete` 删除，以及选中后用 `↑`/`↓` 调整垂直位置；
- 药剂标注支持显式的 `^` 上标、`_` 下标及花括号分组语法，普通数字保持原样以兼容药剂代号；
- 药剂标注可记录名称、用量、单位和备注；
- 合流产品进入下游时，自动要求一条合流支路实测值以消除欠定自由度；
- 使用广义线性方程组进行开路及多回流闭路的质量与多组分守恒反算，各组分独立计算品位和回收率，并识别唯一解、部分唯一解、欠定及矛盾数据；
- 终端合流产品可在中间支路欠定时先完成全流程平衡；
- 物流及浮选单元结果详情；
- 可拖动画布结果标注；
- 可通过“流程编辑 → 添加球磨机”插入圆形球磨机，支持一入一出连线及物料平衡；
- 球磨机从圆底正中向下出料；所有设备图标下方均不绘制名称或说明小字；
- 选中结果指标框后可按 `Delete` 临时隐藏，重新计算时自动恢复；
- 可在画布上添加、拖动、双击编辑、删除和复制粘贴自定义文字图层；支持多条文字及文字与流程单元一起复制；
- 复制流程组时自动复制当前方案中相关物流的药剂标注，保留用量、备注、位置偏移和排列顺序，并绑定到副本物流；
- 可通过“标注样式”统一调整结果标注和产品名称的字号、粗体及文字颜色；
- 鼠标滚轮缩放及中键拖动画布视图；
- 底部状态栏持续显示当前选中的浮选单元类型、编号和多选数量；
- 左侧底部操作日志记录保存、导入、计算、导出和画布编辑结果，并展开计算错误与欠定提示；
- 复杂回流支路支持独立调整横向走廊和汇入高度；选中具体回流时自动高亮该支路并弱化其他回流；
- 干质量、各组分品位、全流程产率和各组分回收率显示设置；
- 画布中含计算质量或计算品位的结果卡使用加粗边框，产率和回收率不参与区分；可在“显示设置”中开关并自定义线宽，作为本机显示偏好记住；
- 指标框的质量单位可选择 `g`、`kg`、`t` 或自定义文本，并随项目保存；
- 指标标签可在中文名称与论文符号 `m / β / γ / ε` 之间切换；
- 明暗主题及 SVG、PNG、JPEG 流程图导出；支持将各方案中已命名产品的数据导出为 Excel 工作簿。
- 项目保存与导入仅支持当前 v23 格式，可恢复拓扑、闭路、实测数据和标注位置；其他版本直接拒绝。
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
xmake f -p linux -m release --qt=/opt/Qt/6.11.1/gcc_64
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

当前有 14 个测试目标。`xmake build -a` 可一次构建主程序及全部测试；涉及主窗口交互的目标额外依赖 Qt Test。各目标职责与单独运行命令如下。

Linux：

```bash
xmake build topology_test
xmake build connection_test
xmake build merge_test
xmake build terminal_input_test
xmake build stream_filter_interaction_test
xmake build clipboard_interaction_test
xmake build result_source_interaction_test
xmake build ball_mill_interaction_test
xmake build thickener_interaction_test
xmake build canvas_view_test
xmake build export_test
xmake build excel_export_test
xmake build closed_loop_test
xmake build project_io_test
xmake run topology_test
QT_QPA_PLATFORM=offscreen xmake run connection_test
QT_QPA_PLATFORM=offscreen xmake run merge_test
QT_QPA_PLATFORM=offscreen xmake run terminal_input_test
QT_QPA_PLATFORM=offscreen xmake run stream_filter_interaction_test
QT_QPA_PLATFORM=offscreen xmake run clipboard_interaction_test
QT_QPA_PLATFORM=offscreen xmake run result_source_interaction_test
QT_QPA_PLATFORM=offscreen xmake run ball_mill_interaction_test
QT_QPA_PLATFORM=offscreen xmake run thickener_interaction_test
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
C:\Users\13427\xmake\xmake.exe build stream_filter_interaction_test
C:\Users\13427\xmake\xmake.exe build clipboard_interaction_test
C:\Users\13427\xmake\xmake.exe build result_source_interaction_test
C:\Users\13427\xmake\xmake.exe build ball_mill_interaction_test
C:\Users\13427\xmake\xmake.exe build thickener_interaction_test
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
- `stream_filter_interaction_test`：通过真实主窗口的鼠标和键盘交互，覆盖单选/多选、一跳物流筛选、表格双向定位与编辑、关注对象组合、回流和产品汇流、方案/项目切换、删除和撤销后的筛选刷新，以及产品命名和 Excel 排序/导出提示；该目标额外链接 Qt Test。
- `clipboard_interaction_test`：文字单独/多选/混合复制、流程组药剂归属和堆叠、方案切换、快捷键与菜单、样式和位置、重复粘贴、撤销重做、保存读取及普通输入框的剪贴板行为；该目标额外链接 Qt Test。
- `result_source_interaction_test`：结果卡边框、多组分混合来源、指标显隐、绘制及 SVG 导出、本机显示偏好、样式编辑、项目切换及撤销重做不影响偏好、方案切换和数据协调；该目标额外链接 Qt Test。
- `ball_mill_interaction_test`：设备图标无下方小字、菜单添加、圆形选择、球磨机顶部入料/底部出料、直连/汇流/回流、一入一出质量和品位守恒、项目读写、复制粘贴、撤销重做及 SVG 圆形导出；该目标额外链接 Qt Test。
- `thickener_interaction_test`：浓密池一入一出、菜单与尺寸交互、上下端口、物料守恒、连接/汇流/回流、复制、项目读写、撤销重做和 SVG 图形。
- `canvas_view_test`：滚轮缩放边界、鼠标中键平移，以及药剂标注方向键优先级。
- `export_test`：SVG、PNG 和 JPEG 文件生成及格式校验。
- `excel_export_test`：多方案 XLSX 数据、仅命名产品导出、命名/清空名称后的筛选、排序与合并单元格、空导出保护及文件结构校验。
- `closed_loop_test`：回流汇合、闭路拓扑识别、断开及计算保护。
- `project_io_test`：项目数据与方案/标注往返、当前格式拒绝检查、开闭路连接、设备类型、撤销/复制及样例读取；入口位于 `ProjectIoTest.cpp`，具体检查按职责放在 `ProjectIo*Checks.cpp`。

## 常用操作

- 顶部工具栏按“项目管理”“流程编辑”“方案设置”“显示设置”“导出选项”分组为下拉菜单；当前试验方案仍可直接通过下拉框切换；
- “试验方案”下拉框：切换当前药剂制度和实验数据组；
- 右侧“计算模式”：按方案切换严格平衡或带测量标准差的数据协调；
- “新增方案”：创建空白数据组；“复制方案”：基于当前方案创建副本；
- “组分设置”：每行输入一个组分名称；右侧表格会为每种组分生成品位列；
- “方案对比”：选择任一最终或中间产品，横向比较各已计算方案的质量、产率以及每种组分的品位和回收率；
- `Ctrl + +` / `Ctrl + -`：调整选中浮选单元宽度或连接长度；
- `Ctrl + B`：断开连接或拆分合流；
- 将产品箭头拖到同一流程中的上游入料线：建立回流闭路；
- 将产品合并线末端箭头拖到新浮选单元的入料线：将合流产品送入下一道流程；
- `Ctrl + Shift + T`：切换 CAD 暗色画布与明亮主题；
- `Ctrl + S`：保存当前项目；`Ctrl + O`：导入已有项目；
- 鼠标滚轮：以指针位置为中心缩放画布；
- 按住鼠标中键拖动：平移画布视图；
- 选中药剂标注后按 `↑` / `↓`：上下微调 2 个画布单位；按住 `Shift` 时每次调整 10 个画布单位；
- “显示指标”：整体显示或隐藏结果标注；
- “指标设置”：选择标注显示的计算指标；
- “导出”：选择 SVG、PNG 或 JPEG 格式保存完整流程图及当前可见标注。
- “导出Excel”：按方案分组，仅导出用户指定名称且具有计算结果的物流，包括已命名的总入料、最终产品和中间产品；名称默认留空，未命名物流不导出，也不使用默认名称回退。
- “设置 Excel 行顺序”：仅列出已命名物流，可拖动调整在工作簿中的顺序；可恢复为流程顺序，尚未配置顺序的已命名物流按流程顺序追加。

## 闭路操作

将尚未连接的普通产品箭头或产品合并后的输出箭头拖到目标入料线上，入料线高亮后松开，即可创建入料汇合；可以继续向同一位置拖入更多物流，来源数量不设固定上限。同一连通流程内的上游连接会作为回流闭路处理。目标入料既可以是未连接的新鲜入料，也可以已经连接上一段产品或合流产品；已连接的入料线虽然平时隐藏，原位置仍保留投放热区，拖入时会临时显示并高亮。程序会保留原正常上游物流并增加汇合点。拖动产品合并输出时，应从合并线末端的箭头处开始。各输入线沿浮选单元外侧错开布置。在多输入汇流中单击具体来源支路使其高亮，再按 `Ctrl+B` 只断开该来源；选中公共汇流线或汇流点且未选中具体来源时，`Ctrl+B` 才整体断开并恢复原正常上游连接。

闭路建立后，“物流实测参数”面板会自动增加所有回流环所需的数据行：普通产品回流填写该回流物流的干质量和品位；产品合并输出回流填写合并前各回流支路的数据，合并后的总回流由守恒关系自动计算。一个项目可包含多个彼此独立的主流程，每个流程均可有自己的外部新鲜入料；填写全部终端产品和回流支路后即可联动反算内部物流及各外部入料。多个外部入料并存时，流程图中的产率与回收率以全部外部入料合计为基准。

如果缺少回流实测值，闭路方程通常不唯一，计算结果会报告数据不足。全流程产率和回收率始终以反算得到的外部新鲜入料合计为基准。

## 导出说明

- Excel 数据导出生成标准 `.xlsx` 工作簿，第一列按方案合并；未命名物流、未计算方案和没有计算结果的产品物料行会自动忽略；没有可导出数据时提示补充名称或计算，不生成空工作簿；
- Excel 行顺序是项目共享设置，所有方案使用相同顺序；被删除或未命名物流自动忽略，尚未配置顺序的已命名物流按流程顺序追加；
- “产品名称”默认留空；“物流/入料定义”只负责定位，不作为产品名称或 Excel 名称回退。右侧显示筛选不限制 Excel 导出范围；

- SVG 为矢量格式，适合论文排版和后续编辑；
- PNG 为无损栅格格式，适合文档和演示文稿；
- JPEG 以 95% 品质输出，适合需要较小文件体积的场景；
- PNG/JPEG 可选择 1×、2×、3× 或 4× 输出倍率，默认 2×；文字较多时建议使用 3× 或 4×；
- 三种格式均按所有画布图元的实际范围导出，并在四周保留 30 个画布单位的空白；
- 导出统一使用白色背景。暗色主题下导出时，程序会临时按明亮主题绘制，完成后恢复界面主题；
- PNG/JPEG 会限制最长边和总像素数，超大画布会等比例缩小以控制内存占用。

详细模块边界见 [架构说明](docs/ARCHITECTURE.md)，开发流程见 [开发指南](docs/DEVELOPMENT.md)，标注扩展见 [标注系统](docs/ANNOTATIONS.md)。
