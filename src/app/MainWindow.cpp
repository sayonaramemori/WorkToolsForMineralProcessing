#include "app/MainWindow.h"
#include "core/FlotationUnit.h"
#include "annotations/AnnotationManager.h"
#include "adapters/CanvasTopologyBuilder.h"
#include "document/FlowsheetDocument.h"
#include "editor/CanvasActions.h"
#include "editor/CanvasView.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/FeedJunctionItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"
#include "services/RasterExporter.h"
#include "services/ExcelDataExporter.h"
#include "services/ProjectSerializer.h"
#include "services/SvgExporter.h"
#include "services/FlowsheetCalculationService.h"
#include "services/ThemeService.h"
#include "ui/TerminalProductDock.h"
#include "ui/ResultMetricMenu.h"
#include "ui/ScenarioComparisonDialog.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGraphicsScene>
#include <QFileInfo>
#include <QKeySequence>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMenu>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSet>
#include <QShortcut>
#include <QSpinBox>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

namespace afs {

MainWindow::MainWindow() {
    m_scene = new FlowsheetScene(this);
    m_scene->setSceneRect(-900, -600, 1800, 1200);
    m_view = new CanvasView(m_scene, this);
    m_view->setRenderHints(QPainter::Antialiasing);
    m_view->setDragMode(QGraphicsView::RubberBandDrag);
    m_view->setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
    setCentralWidget(m_view);

    m_document = new FlowsheetDocument(this);
    m_annotationManager = new AnnotationManager(
        *static_cast<FlowsheetScene*>(m_scene), *m_document, this);
    m_view->setVerticalNudgeHandler([this](qreal delta) {
        return m_annotationManager->nudgeSelectedReagents(delta);
    });
    m_terminalDock = new TerminalProductDock(*m_document, this);
    addDockWidget(Qt::RightDockWidgetArea, m_terminalDock);
    connect(static_cast<FlowsheetScene*>(m_scene), &FlowsheetScene::topologyChanged,
            this, [this] { refreshTerminalProducts(); });
    connect(static_cast<FlowsheetScene*>(m_scene), &FlowsheetScene::topologyChanged,
            this, [this] { setProjectDirty(); });
    connect(static_cast<FlowsheetScene*>(m_scene), &FlowsheetScene::geometryChanged,
            this, [this] { setProjectDirty(); });
    connect(m_document, &FlowsheetDocument::projectChanged,
            this, [this] { setProjectDirty(); });
    connect(m_scene, &QGraphicsScene::selectionChanged,
            this, [this] { syncCanvasSelectionToTable(); });
    connect(m_terminalDock, &TerminalProductDock::graphicsItemRequested, this,
            [this](QGraphicsItem* item) {
                if (!item) return;
                m_scene->clearSelection();
                item->setSelected(true);
                m_view->centerOn(item);
            });
    connect(m_terminalDock, &TerminalProductDock::calculationRequested,
            this, [this] { calculateFlowsheet(); });
    connect(m_document, &FlowsheetDocument::calculationChanged,
            this, [this] { refreshSelectedResult(); });
    connect(m_document, &FlowsheetDocument::productNameChanged,
            this, [this](const QString&) { refreshProductNames(); });
    connect(m_document, &FlowsheetDocument::annotationTextSettingsChanged,
            this, [this] { refreshProductNames(); });
    connect(m_document, &FlowsheetDocument::scenariosChanged,
            this, [this] { refreshScenarioUi(); });
    connect(m_document, &FlowsheetDocument::currentScenarioChanged, this, [this] {
        refreshScenarioUi();
        m_terminalDock->setSnapshot(CanvasTopologyBuilder::build(
            *static_cast<FlowsheetScene*>(m_scene)));
        m_scene->clearSelection();
        statusBar()->showMessage(tr("已切换至：%1").arg(m_document->currentScenarioName()), 3000);
    });

    auto* toolbar = addToolBar(tr("工具"));
    toolbar->setObjectName("mainToolbar");
    const auto addMenuButton = [toolbar](const QString& title, QMenu* menu) {
        auto* button = new QToolButton(toolbar);
        button->setText(title);
        button->setPopupMode(QToolButton::InstantPopup);
        button->setMenu(menu);
        toolbar->addWidget(button);
        return button;
    };

    auto* projectMenu = new QMenu(tr("项目管理"), toolbar);
    auto* saveAction = projectMenu->addAction(tr("保存项目"));
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, [this] { saveProject(); });
    auto* importAction = projectMenu->addAction(tr("导入项目"));
    importAction->setShortcut(QKeySequence::Open);
    connect(importAction, &QAction::triggered, this, [this] { importProject(); });
    addMenuButton(tr("项目管理"), projectMenu);

    auto* flowMenu = new QMenu(tr("流程编辑"), toolbar);
    auto* addAction = flowMenu->addAction(tr("添加浮选单元"));
    connect(addAction, &QAction::triggered, this, [this] { addFlotationUnit(); });
    auto* addSplitterAction = flowMenu->addAction(tr("添加二分流器"));
    connect(addSplitterAction, &QAction::triggered, this, [this] { addBinarySplitter(); });
    addMenuButton(tr("流程编辑"), flowMenu);

    toolbar->addSeparator();
    toolbar->addWidget(new QLabel(tr("试验方案："), toolbar));
    m_scenarioCombo = new QComboBox(toolbar);
    m_scenarioCombo->setMinimumWidth(140);
    toolbar->addWidget(m_scenarioCombo);
    connect(m_scenarioCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                if (index >= 0) m_document->setCurrentScenario(index);
            });
    auto* scenarioMenu = new QMenu(tr("方案设置"), toolbar);
    auto* addScenarioAction = scenarioMenu->addAction(tr("新增方案"));
    connect(addScenarioAction, &QAction::triggered, this, [this] { addScenario(false); });
    auto* copyScenarioAction = scenarioMenu->addAction(tr("复制方案"));
    connect(copyScenarioAction, &QAction::triggered, this, [this] { addScenario(true); });
    auto* renameScenarioAction = scenarioMenu->addAction(tr("重命名方案"));
    connect(renameScenarioAction, &QAction::triggered, this, [this] { renameScenario(); });
    auto* deleteScenarioAction = scenarioMenu->addAction(tr("删除方案"));
    connect(deleteScenarioAction, &QAction::triggered, this, [this] { deleteScenario(); });
    scenarioMenu->addSeparator();
    auto* compareScenarioAction = scenarioMenu->addAction(tr("方案对比"));
    connect(compareScenarioAction, &QAction::triggered, this, [this] { compareScenarios(); });
    addMenuButton(tr("方案设置"), scenarioMenu);
    toolbar->addSeparator();

    auto* displayMenu = new QMenu(tr("显示设置"), toolbar);
    m_themeAction = displayMenu->addAction(tr("暗色主题"));
    m_themeAction->setCheckable(true);
    m_themeAction->setShortcut(QKeySequence("Ctrl+Shift+T"));
    connect(m_themeAction, &QAction::toggled, this, [this](bool checked) { applyTheme(checked); });

    auto* exportMenu = new QMenu(tr("导出选项"), toolbar);
    auto* exportAction = exportMenu->addAction(tr("导出流程图"));
    connect(exportAction, &QAction::triggered, this, [this] { exportScene(); });
    auto* exportDataAction = exportMenu->addAction(tr("导出 Excel"));
    connect(exportDataAction, &QAction::triggered, this, [this] { exportExcelData(); });
    auto* exportOrderAction = exportMenu->addAction(tr("设置 Excel 行顺序"));
    connect(exportOrderAction, &QAction::triggered,
            this, [this] { editExcelExportOrder(); });

    m_annotationsAction = displayMenu->addAction(tr("显示指标"));
    m_annotationsAction->setCheckable(true);
    m_annotationsAction->setChecked(true);
    connect(m_annotationsAction, &QAction::toggled, this,
            [this](bool visible) { m_annotationManager->setAnnotationsVisible(visible); });

    auto* metricButton = new ResultMetricMenu(toolbar);
    // ResultMetricMenu is used as the signal owner only. Its QMenu is embedded
    // below; keep the backing tool button from appearing as an unmanaged child
    // on the toolbar.
    metricButton->hide();
    connect(metricButton, &ResultMetricMenu::metricVisibilityChanged,
            m_annotationManager, &AnnotationManager::setMetricVisible);
    connect(metricButton, &ResultMetricMenu::metricLabelModeChanged,
            m_annotationManager, &AnnotationManager::setMetricLabelMode);
    displayMenu->addMenu(metricButton->metricMenu());

    displayMenu->addSeparator();
    auto* annotationStyleAction = displayMenu->addAction(tr("标注样式"));
    connect(annotationStyleAction, &QAction::triggered,
            this, [this] { editAnnotationTextStyle(); });
    addMenuButton(tr("显示设置"), displayMenu);
    addMenuButton(tr("导出选项"), exportMenu);

    auto* widenShortcut = new QShortcut(QKeySequence("Ctrl++"), this);
    connect(widenShortcut, &QShortcut::activated, this, [this] { resizeSelectedUnits(40.0); });
    // 部分键盘把加号报告为 Ctrl+=，同时注册以保证一致操作。
    auto* widenEqualShortcut = new QShortcut(QKeySequence("Ctrl+="), this);
    connect(widenEqualShortcut, &QShortcut::activated, this, [this] { resizeSelectedUnits(40.0); });
    auto* narrowShortcut = new QShortcut(QKeySequence("Ctrl+-"), this);
    connect(narrowShortcut, &QShortcut::activated, this, [this] { resizeSelectedUnits(-40.0); });
    auto* breakShortcut = new QShortcut(QKeySequence("Ctrl+B"), this);
    connect(breakShortcut, &QShortcut::activated, this, [this] { disconnectSelectedLines(); });

    addFlotationUnit();
    refreshScenarioUi();
    refreshTerminalProducts();
    applyTheme(ThemeService::loadDarkPreference(), false);
    setWindowTitle(tr("浮选单元编辑器"));
    setProjectDirty(true);
    resize(1200, 760);
    statusBar()->showMessage(
        tr("单击选择，拖动图元；滚轮缩放画布，按住鼠标中键拖动画布"));
}

void MainWindow::refreshScenarioUi() {
    if (!m_scenarioCombo) return;
    const QSignalBlocker blocker(m_scenarioCombo);
    m_scenarioCombo->clear();
    for (const auto& scenario : m_document->scenarios())
        m_scenarioCombo->addItem(scenario.name, scenario.id);
    m_scenarioCombo->setCurrentIndex(m_document->currentScenarioIndex());
    if (m_terminalDock) m_terminalDock->setScenarioName(m_document->currentScenarioName());
}

void MainWindow::addScenario(bool copyCurrent) {
    bool accepted = false;
    const QString suggested = copyCurrent
        ? tr("%1 - 副本").arg(m_document->currentScenarioName())
        : tr("方案 %1").arg(m_document->scenarios().size() + 1);
    const QString name = QInputDialog::getText(
        this, copyCurrent ? tr("复制试验方案") : tr("新增试验方案"),
        tr("方案名称"), QLineEdit::Normal, suggested, &accepted).simplified();
    if (!accepted || name.isEmpty()) return;
    const int index = m_document->addScenario(name, copyCurrent);
    if (index >= 0) m_document->setCurrentScenario(index);
}

void MainWindow::renameScenario() {
    bool accepted = false;
    const QString name = QInputDialog::getText(
        this, tr("重命名试验方案"), tr("方案名称"), QLineEdit::Normal,
        m_document->currentScenarioName(), &accepted).simplified();
    if (accepted && !name.isEmpty())
        m_document->renameScenario(m_document->currentScenarioIndex(), name);
}

void MainWindow::deleteScenario() {
    if (m_document->scenarios().size() <= 1) {
        QMessageBox::information(this, tr("删除试验方案"), tr("项目至少需要保留一个试验方案。"));
        return;
    }
    if (QMessageBox::question(this, tr("删除试验方案"),
            tr("确定删除“%1”及其药剂和实验数据吗？").arg(
                m_document->currentScenarioName())) != QMessageBox::Yes) return;
    m_document->removeScenario(m_document->currentScenarioIndex());
}

void MainWindow::compareScenarios() {
    const auto snapshot = CanvasTopologyBuilder::build(*static_cast<FlowsheetScene*>(m_scene));
    if (snapshot.terminalProducts.isEmpty()) {
        QMessageBox::information(this, tr("方案对比"), tr("当前流程没有可对比的终端产品。"));
        return;
    }
    ScenarioComparisonDialog(*m_document, snapshot, this).exec();
}

void MainWindow::addFlotationUnit() {
    const QPointF center = m_view->mapToScene(m_view->viewport()->rect().center());
    FlotationUnit unit{QString::number(m_nextUnitId++), center};
    m_scene->addItem(new FlotationUnitItem(std::move(unit)));
    static_cast<FlowsheetScene*>(m_scene)->notifyTopologyChanged();
}

void MainWindow::addBinarySplitter() {
    bool accepted = false;
    const double leftPercent = QInputDialog::getDouble(
        this, tr("添加二分流器"), tr("左支路比例（右支路自动补足至 100%）"),
        50.0, 0.1, 99.9, 1, &accepted);
    if (!accepted) return;
    const QPointF center = m_view->mapToScene(m_view->viewport()->rect().center());
    FlotationUnit unit{QString::number(m_nextUnitId++), center};
    unit.kind = UnitKind::BinarySplitter;
    unit.leftSplitPercent = leftPercent;
    unit.bodyHeight = 110.0;
    m_scene->addItem(new FlotationUnitItem(std::move(unit)));
    static_cast<FlowsheetScene*>(m_scene)->notifyTopologyChanged();
}

bool MainWindow::saveProject() {
    QString path = m_projectPath;
    if (path.isEmpty()) {
        path = QFileDialog::getSaveFileName(
            this, tr("保存浮选项目"), "flotation-project.afs.json",
            tr("AutoFlotationSheet 项目 (*.afs.json);;JSON 文件 (*.json)"));
        if (path.isEmpty()) return false;
        if (!path.endsWith(".json", Qt::CaseInsensitive)) path += ".afs.json";
    }

    QString error;
    if (!ProjectSerializer::save(
            *static_cast<FlowsheetScene*>(m_scene), *m_document, path, &error)) {
        QMessageBox::warning(this, tr("保存项目失败"), error);
        return false;
    }
    m_projectPath = path;
    setProjectDirty(false);
    statusBar()->showMessage(tr("项目已保存：%1").arg(path), 8000);
    return true;
}

void MainWindow::importProject() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("导入浮选项目"), {},
        tr("AutoFlotationSheet 项目 (*.afs.json *.json);;所有文件 (*)"));
    if (path.isEmpty()) return;

    // 场景清空会销毁标注图元，先让管理器释放其非拥有型索引。
    // 若校验失败，旧文档仍在，再同步即可恢复原标注。
    m_annotationManager->clearGraphicsItems();
    QString error;
    m_loadingProject = true;
    const bool loaded = ProjectSerializer::load(
        *static_cast<FlowsheetScene*>(m_scene), *m_document, path, &error);
    m_loadingProject = false;
    if (!loaded) {
        m_annotationManager->synchronize();
        QMessageBox::warning(this, tr("导入项目失败"), error);
        return;
    }

    m_projectPath = path;
    setProjectDirty(false);
    updateNextUnitId();
    m_scene->clearSelection();
    static_cast<FlowsheetScene*>(m_scene)->refreshAppearance();
    const QRectF bounds = m_scene->itemsBoundingRect().adjusted(-60, -60, 60, 60);
    if (!bounds.isEmpty()) m_view->fitInView(bounds, Qt::KeepAspectRatio);
    statusBar()->showMessage(tr("项目已导入：%1").arg(path), 8000);
}

void MainWindow::setProjectDirty(bool dirty) {
    m_projectDirty = dirty;
    const QString name = m_projectPath.isEmpty()
        ? tr("未命名项目") : QFileInfo(m_projectPath).fileName();
    setWindowTitle(tr("浮选单元编辑器 · %1%2")
        .arg(name, m_projectDirty ? QStringLiteral(" *") : QString()));
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (!m_projectDirty) {
        event->accept();
        return;
    }
    const auto choice = QMessageBox::warning(
        this, tr("保存项目"), tr("当前项目有未保存的更改，是否在退出前保存？"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (choice == QMessageBox::Cancel) {
        event->ignore();
    } else if (choice == QMessageBox::Save) {
        saveProject() ? event->accept() : event->ignore();
    } else {
        event->accept();
    }
}

void MainWindow::updateNextUnitId() {
    QSet<QString> ids;
    for (auto* item : m_scene->items()) {
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item)) ids.insert(unit->unit().id);
    }
    m_nextUnitId = 1;
    while (ids.contains(QString::number(m_nextUnitId))) ++m_nextUnitId;
}

void MainWindow::refreshTerminalProducts() {
    // Import emits topologyChanged so views can rebuild their snapshots, but
    // the imported scenarios have just been recalculated and must be retained.
    if (!m_loadingProject) m_document->invalidateAllCalculations();
    m_terminalDock->setSnapshot(CanvasTopologyBuilder::build(
        *static_cast<FlowsheetScene*>(m_scene)));
    refreshProductNames();
    syncCanvasSelectionToTable();
}

void MainWindow::refreshProductNames() {
    for (auto* item : m_scene->items()) {
        if (auto* product = dynamic_cast<ProductLineItem*>(item))
        {
            product->setTextSettings(m_document->annotationTextSettings());
            product->setProductName(m_document->productName(product->streamId()));
        } else if (auto* merge = dynamic_cast<MergeJunctionItem*>(item)) {
            merge->setTextSettings(m_document->annotationTextSettings());
            merge->setProductName(m_document->productName(merge->outputStreamId()));
        }
    }
}

void MainWindow::editAnnotationTextStyle() {
    QDialog dialog(this);
    dialog.setWindowTitle(tr("标注文字样式"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;
    const auto current = m_document->annotationTextSettings();

    auto* pointSize = new QSpinBox(&dialog);
    pointSize->setRange(7, 36);
    pointSize->setSuffix(tr(" pt"));
    pointSize->setValue(current.pointSize);
    form->addRow(tr("字体大小"), pointSize);

    auto* bold = new QCheckBox(tr("加粗"), &dialog);
    bold->setChecked(current.bold);
    form->addRow(tr("字重"), bold);

    auto* followTheme = new QCheckBox(tr("跟随界面主题颜色"), &dialog);
    followTheme->setChecked(!current.color.isValid());
    form->addRow(tr("颜色模式"), followTheme);

    QColor selectedColor = current.color.isValid()
        ? current.color : qApp->palette().color(QPalette::Text);
    auto* colorButton = new QPushButton(&dialog);
    const auto refreshColorButton = [&] {
        colorButton->setText(selectedColor.name(QColor::HexRgb));
        colorButton->setStyleSheet(QString("background:%1; color:%2;")
            .arg(selectedColor.name(), selectedColor.lightness() < 128 ? "white" : "black"));
        colorButton->setEnabled(!followTheme->isChecked());
    };
    refreshColorButton();
    connect(followTheme, &QCheckBox::toggled, &dialog,
            [&](bool) { refreshColorButton(); });
    connect(colorButton, &QPushButton::clicked, &dialog, [&] {
        const QColor chosen = QColorDialog::getColor(
            selectedColor, &dialog, tr("选择标注文字颜色"), QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) { selectedColor = chosen; refreshColorButton(); }
    });
    form->addRow(tr("文字颜色"), colorButton);
    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) return;
    m_document->setAnnotationTextSettings({
        pointSize->value(), bold->isChecked(),
        followTheme->isChecked() ? QColor() : selectedColor});
}

void MainWindow::syncCanvasSelectionToTable() {
    const auto selected = m_scene->selectedItems();
    m_terminalDock->selectGraphicsItem(selected.isEmpty() ? nullptr : selected.first());
    refreshSelectedResult();
}

void MainWindow::calculateFlowsheet() {
    auto result = FlowsheetCalculationService::calculate(
        *static_cast<FlowsheetScene*>(m_scene), *m_document);
    const bool complete = result.complete;
    const int issueCount = result.issues.size();
    m_document->setCalculationResult(std::move(result));
    statusBar()->showMessage(complete ? tr("平衡计算成功")
                                      : tr("计算未完成，共发现 %1 个问题").arg(issueCount), 5000);
}

void MainWindow::refreshSelectedResult() {
    const auto selected = m_scene->selectedItems();
    if (selected.isEmpty()) {
        m_terminalDock->clearResultDetails();
        return;
    }
    auto* item = selected.first();
    if (auto* product = dynamic_cast<ProductLineItem*>(item)) {
        m_terminalDock->showStreamResult(product->streamId());
    } else if (auto* merge = dynamic_cast<MergeJunctionItem*>(item)) {
        m_terminalDock->showStreamResult(merge->outputStreamId());
    } else if (auto* feedJunction = dynamic_cast<FeedJunctionItem*>(item)) {
        m_terminalDock->showStreamResult(feedJunction->outputStreamId());
    } else if (auto* unit = dynamic_cast<FlotationUnitItem*>(item)) {
        const auto snapshot = CanvasTopologyBuilder::build(*static_cast<FlowsheetScene*>(m_scene));
        m_terminalDock->showUnitResult(unit->unit().id, snapshot.graph);
    } else {
        m_terminalDock->clearResultDetails();
    }
}

void MainWindow::resizeSelectedUnits(double delta) {
    auto* flowsheet = static_cast<FlowsheetScene*>(m_scene);
    const auto result = CanvasActions::resizeSelection(*flowsheet, delta);
    if (result.resizedConnections > 0 && result.resizedUnits > 0) {
        statusBar()->showMessage(
            tr("已调整 %1 个浮选单元宽度和 %2 条连接线长度")
                .arg(result.resizedUnits).arg(result.resizedConnections),
            3000);
    } else if (result.resizedConnections > 0) {
        statusBar()->showMessage(tr("已调整 %1 条合并连接线长度").arg(result.resizedConnections), 3000);
    } else if (result.resizedUnits > 0) {
        statusBar()->showMessage(
            tr("已调整 %1 个浮选单元，当前横向长度：%2")
                .arg(result.resizedUnits).arg(result.lastUnitWidth, 0, 'f', 0),
            3000);
    } else if (m_scene->selectedItems().isEmpty()) {
        statusBar()->showMessage(tr("请先选中需要调整长度的浮选单元"), 3000);
    } else {
        statusBar()->showMessage(tr("所选对象未连接，或已达到允许的最小/最大长度"), 3000);
    }
}

void MainWindow::disconnectSelectedLines() {
    auto* flowsheet = static_cast<FlowsheetScene*>(m_scene);
    const int disconnected = CanvasActions::disconnectSelection(*flowsheet);
    statusBar()->showMessage(disconnected > 0
        ? tr("已断开 %1 条浮选单元连接线").arg(disconnected)
        : tr("请先选中已连接的产品线、产品汇流或入料回流汇合点"), 3000);
}

void MainWindow::exportScene() {
    const QString svgFilter = tr("SVG 文件 (*.svg)");
    const QString pngFilter = tr("PNG 图像 (*.png)");
    const QString jpegFilter = tr("JPEG 图像 (*.jpg *.jpeg)");
    QString selectedFilter = svgFilter;
    QString path = QFileDialog::getSaveFileName(
        this, tr("导出流程图"), "flotation-flowsheet",
        QStringList{svgFilter, pngFilter, jpegFilter}.join(";;"), &selectedFilter);
    if (path.isEmpty()) return;

    QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix.isEmpty()) {
        suffix = selectedFilter == pngFilter ? "png"
            : selectedFilter == jpegFilter ? "jpg" : "svg";
        path += "." + suffix;
    }

    qreal rasterScale = 1.0;
    if (suffix == "png" || suffix == "jpg" || suffix == "jpeg") {
        const QStringList qualities{
            tr("标准（1×）"), tr("高清（2×，推荐）"),
            tr("超清（3×）"), tr("超清（4×）")};
        bool accepted = false;
        const QString quality = QInputDialog::getItem(
            this, tr("栅格图像清晰度"),
            tr("选择输出倍率；倍率越高，文字越清晰，文件也越大。"),
            qualities, 1, false, &accepted);
        if (!accepted) return;
        rasterScale = quality == qualities[0] ? 1.0
            : quality == qualities[1] ? 2.0
            : quality == qualities[2] ? 3.0 : 4.0;
    }

    const bool screenWasDark = m_darkTheme;
    if (screenWasDark) applyTheme(false, false);
    const QBrush screenBackground = m_scene->backgroundBrush();
    m_scene->setBackgroundBrush(Qt::white);
    bool exported = false;
    if (suffix == "svg") {
        exported = SvgExporter::exportScene(*m_scene, path, tr("浮选工艺流程图"));
    } else if (suffix == "png") {
        exported = RasterExporter::exportScene(*m_scene, path, "png", rasterScale);
    } else if (suffix == "jpg" || suffix == "jpeg") {
        exported = RasterExporter::exportScene(*m_scene, path, "jpeg", rasterScale);
    }

    m_scene->setBackgroundBrush(screenBackground);
    if (screenWasDark) applyTheme(true, false);
    statusBar()->showMessage(
        exported ? tr("已导出：%1").arg(path) : tr("导出失败，请检查文件格式和保存路径"),
        8000);
}

void MainWindow::exportExcelData() {
    const auto snapshot = CanvasTopologyBuilder::build(*static_cast<FlowsheetScene*>(m_scene));
    QString path = QFileDialog::getSaveFileName(
        this, tr("导出方案数据"), "flotation-scenarios.xlsx", tr("Excel 工作簿 (*.xlsx)"));
    if (path.isEmpty()) return;
    if (!path.endsWith(".xlsx", Qt::CaseInsensitive)) path += ".xlsx";
    QString error;
    if (!ExcelDataExporter::exportWorkbook(*m_document, snapshot, path, &error)) {
        QMessageBox::warning(this, tr("导出 Excel 失败"), error);
        return;
    }
    statusBar()->showMessage(tr("方案数据已导出：%1").arg(path), 8000);
}

void MainWindow::editExcelExportOrder() {
    const auto snapshot = CanvasTopologyBuilder::build(*static_cast<FlowsheetScene*>(m_scene));
    if (snapshot.reportStreams.isEmpty()) {
        QMessageBox::information(this, tr("Excel 导出顺序"), tr("当前流程没有可排序的物流。"));
        return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Excel 导出顺序"));
    dialog.resize(520, 520);
    auto* layout = new QVBoxLayout(&dialog);
    auto* hint = new QLabel(tr("拖动调整导出顺序。总入料、最终产品和中间产品均可自由排序。"),
                            &dialog);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto* list = new QListWidget(&dialog);
    list->setDragDropMode(QAbstractItemView::InternalMove);
    list->setDefaultDropAction(Qt::MoveAction);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(list, 1);

    QHash<QString, CanvasStreamDescriptor> byId;
    for (const auto& stream : snapshot.reportStreams) byId.insert(stream.streamId, stream);
    const auto appendItem = [this, list](const CanvasStreamDescriptor& stream) {
        const QString configuredName = m_document->productName(stream.streamId).simplified();
        const QString label = configuredName.isEmpty()
            ? stream.displayName : QString("%1（%2）").arg(configuredName, stream.displayName);
        auto* item = new QListWidgetItem(label, list);
        item->setData(Qt::UserRole, stream.streamId);
        item->setToolTip(tr("物流 ID：%1").arg(stream.streamId));
    };
    QSet<QString> added;
    for (const auto& id : m_document->exportStreamOrder()) {
        const auto found = byId.constFind(id);
        if (found == byId.cend()) continue;
        appendItem(found.value()); added.insert(id);
    }
    for (const auto& stream : snapshot.reportStreams) {
        if (added.contains(stream.streamId)) continue;
        appendItem(stream); added.insert(stream.streamId);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                         &dialog);
    auto* reset = buttons->addButton(tr("恢复流程顺序"), QDialogButtonBox::ResetRole);
    connect(reset, &QPushButton::clicked, &dialog, [list, snapshot, appendItem] {
        list->clear();
        for (const auto& stream : snapshot.reportStreams) appendItem(stream);
    });
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) return;
    QStringList order;
    for (int row = 0; row < list->count(); ++row)
        order.append(list->item(row)->data(Qt::UserRole).toString());
    m_document->setExportStreamOrder(std::move(order));
    statusBar()->showMessage(tr("Excel 导出顺序已更新"), 3000);
}

void MainWindow::applyTheme(bool dark, bool saveSetting) {
    m_darkTheme = dark;
    m_scene->setBackgroundBrush(ThemeService::applyApplicationPalette(dark));
    if (m_themeAction) {
        const QSignalBlocker blocker(m_themeAction);
        m_themeAction->setChecked(dark);
        m_themeAction->setText(dark ? tr("明亮主题") : tr("暗色主题"));
    }
    static_cast<FlowsheetScene*>(m_scene)->refreshAppearance();
    if (m_terminalDock) m_terminalDock->refreshAppearance();
    if (m_annotationManager) m_annotationManager->refreshAppearance();
    if (saveSetting) ThemeService::saveDarkPreference(dark);
}

} // namespace afs
