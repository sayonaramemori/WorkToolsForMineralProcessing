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
#include "services/ProjectUndoManager.h"
#include "services/SvgExporter.h"
#include "services/FlowsheetCalculationService.h"
#include "services/ThemeService.h"
#include "ui/TerminalProductDock.h"
#include "ui/OperationLogDock.h"
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
#include <QUndoStack>
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

    m_selectionStatusLabel = new QLabel(tr("当前未选中浮选单元"), this);
    m_selectionStatusLabel->setMinimumWidth(260);
    m_selectionStatusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    statusBar()->addPermanentWidget(m_selectionStatusLabel, 1);

    m_document = new FlowsheetDocument(this);
    m_annotationManager = new AnnotationManager(
        *static_cast<FlowsheetScene*>(m_scene), *m_document, this);
    m_view->setVerticalNudgeHandler([this](qreal delta) {
        return m_annotationManager->nudgeSelectedReagents(delta);
    });
    m_view->setDeleteHandler([this] { return deleteSelectedUnits(); });
    m_terminalDock = new TerminalProductDock(*m_document, this);
    addDockWidget(Qt::RightDockWidgetArea, m_terminalDock);

    m_operationLog = new OperationLogDock(this);
    addDockWidget(Qt::BottomDockWidgetArea, m_operationLog);
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
    resizeDocks({m_operationLog}, {130}, Qt::Vertical);
    connect(statusBar(), &QStatusBar::messageChanged, this,
            [this](const QString& message) {
                if (!message.isEmpty()) appendOperationLog(message);
            });
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
    auto* newAction = projectMenu->addAction(tr("新建项目"));
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, [this] { newProject(); });
    projectMenu->addSeparator();
    auto* saveAction = projectMenu->addAction(tr("保存项目"));
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, [this] { saveProject(); });
    auto* saveAsAction = projectMenu->addAction(tr("项目另存为"));
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction, &QAction::triggered, this, [this] { saveProjectAs(); });
    projectMenu->addSeparator();
    auto* importAction = projectMenu->addAction(tr("导入项目"));
    importAction->setShortcut(QKeySequence::Open);
    connect(importAction, &QAction::triggered, this, [this] { importProject(); });
    m_recentProjectsMenu = projectMenu->addMenu(tr("最近项目"));
    connect(m_recentProjectsMenu, &QMenu::aboutToShow,
            this, [this] { refreshRecentProjectsMenu(); });
    addMenuButton(tr("项目管理"), projectMenu);

    auto* flowMenu = new QMenu(tr("流程编辑"), toolbar);
    auto* undoAction = flowMenu->addAction(tr("撤销"));
    undoAction->setShortcut(QKeySequence::Undo);
    connect(undoAction, &QAction::triggered, this, [this] {
        if (m_undoManager) m_undoManager->undo();
    });
    auto* redoAction = flowMenu->addAction(tr("重做"));
    redoAction->setShortcuts({QKeySequence::Redo, QKeySequence("Ctrl+Shift+Z")});
    connect(redoAction, &QAction::triggered, this, [this] {
        if (m_undoManager) m_undoManager->redo();
    });
    flowMenu->addSeparator();
    auto* deleteUnitAction = flowMenu->addAction(tr("删除选中单元"));
    connect(deleteUnitAction, &QAction::triggered, this, [this] { deleteSelectedUnits(); });
    flowMenu->addSeparator();
    auto* addAction = flowMenu->addAction(tr("添加浮选单元"));
    connect(addAction, &QAction::triggered, this, [this] { addFlotationUnit(); });
    auto* addThreeProductAction = flowMenu->addAction(tr("添加三产品浮选单元"));
    connect(addThreeProductAction, &QAction::triggered, this, [this] { addThreeProductUnit(); });
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
    connect(metricButton, &ResultMetricMenu::massUnitChanged,
            m_annotationManager, &AnnotationManager::setMassUnit);
    connect(metricButton, &ResultMetricMenu::customMassUnitChanged,
            m_annotationManager, &AnnotationManager::setCustomMassUnit);
    metricButton->setMassUnit(m_document->annotationTextSettings().massUnit,
                              m_document->annotationTextSettings().customMassUnit);
    connect(m_document, &FlowsheetDocument::annotationTextSettingsChanged,
            metricButton, [this, metricButton] {
                metricButton->setMassUnit(m_document->annotationTextSettings().massUnit,
                                          m_document->annotationTextSettings().customMassUnit);
                if (m_annotationsAction) {
                    const QSignalBlocker blocker(m_annotationsAction);
                    m_annotationsAction->setChecked(
                        m_document->annotationTextSettings().resultAnnotationsVisible);
                }
            });
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
    m_undoManager = new ProjectUndoManager(
        *static_cast<FlowsheetScene*>(m_scene), *m_document,
        [this] {
            m_loadingProject = true;
            m_annotationManager->clearGraphicsItems();
            m_scene->clearSelection();
        },
        [this] {
            m_loadingProject = false;
            updateNextUnitId();
            static_cast<FlowsheetScene*>(m_scene)->refreshAppearance();
            refreshSelectionStatus();
            setProjectDirty(true);
        }, this);
    connect(static_cast<FlowsheetScene*>(m_scene), &FlowsheetScene::topologyChanged,
            m_undoManager, [this] { m_undoManager->scheduleCheckpoint("编辑流程拓扑"); });
    connect(static_cast<FlowsheetScene*>(m_scene), &FlowsheetScene::geometryChanged,
            m_undoManager, [this] { m_undoManager->scheduleCheckpoint("调整流程图"); });
    connect(m_document, &FlowsheetDocument::projectChanged,
            m_undoManager, [this] { m_undoManager->scheduleCheckpoint("编辑项目数据"); });
    connect(m_undoManager, &ProjectUndoManager::restoreFailed, this,
            [this](const QString& error) {
                QMessageBox::warning(this, tr("撤销失败"), error);
            });
    undoAction->setEnabled(false);
    redoAction->setEnabled(false);
    connect(m_undoManager->stack(), &QUndoStack::canUndoChanged,
            undoAction, &QAction::setEnabled);
    connect(m_undoManager->stack(), &QUndoStack::canRedoChanged,
            redoAction, &QAction::setEnabled);
    m_undoManager->initialize();
    statusBar()->showMessage(
        tr("单击选择，拖动图元；滚轮缩放画布，按住鼠标中键拖动画布"));
}

void MainWindow::appendOperationLog(const QString& message) {
    if (m_operationLog) m_operationLog->appendMessage(message);
}

void MainWindow::addFlotationUnit() {
    const QPointF center = m_view->mapToScene(m_view->viewport()->rect().center());
    FlotationUnit unit{QString::number(m_nextUnitId++), center};
    m_scene->addItem(new FlotationUnitItem(std::move(unit)));
    static_cast<FlowsheetScene*>(m_scene)->notifyTopologyChanged();
}

void MainWindow::addThreeProductUnit() {
    const QPointF center = m_view->mapToScene(m_view->viewport()->rect().center());
    FlotationUnit unit{QString::number(m_nextUnitId++), center};
    unit.kind = UnitKind::ThreeProductFlotation;
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
        followTheme->isChecked() ? QColor() : selectedColor,
        current.massUnit, current.customMassUnit});
}

void MainWindow::syncCanvasSelectionToTable() {
    const auto selected = m_scene->selectedItems();
    m_terminalDock->selectGraphicsItem(selected.isEmpty() ? nullptr : selected.first());
    refreshSelectedResult();
    refreshSelectionStatus();
}

void MainWindow::refreshSelectionStatus() {
    FlotationUnitItem* selectedUnit = nullptr;
    int selectedUnitCount = 0;
    for (auto* item : m_scene->selectedItems()) {
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item)) {
            if (!selectedUnit) selectedUnit = unit;
            ++selectedUnitCount;
        }
    }
    if (!selectedUnit) {
        m_selectionStatusLabel->setText(tr("当前未选中浮选单元"));
        return;
    }
    QString type;
    switch (selectedUnit->unit().kind) {
    case UnitKind::Flotation: type = tr("浮选单元"); break;
    case UnitKind::BinarySplitter: type = tr("二分流器"); break;
    case UnitKind::ThreeProductFlotation: type = tr("三产品浮选单元"); break;
    }
    QString text = tr("当前已选中：“%1 %2”").arg(type, selectedUnit->unit().id);
    if (selectedUnitCount > 1)
        text += tr("（共 %1 个单元）").arg(selectedUnitCount);
    m_selectionStatusLabel->setText(text);
}

void MainWindow::calculateFlowsheet() {
    auto result = FlowsheetCalculationService::calculate(
        *static_cast<FlowsheetScene*>(m_scene), *m_document);
    const bool complete = result.complete;
    const int issueCount = result.issues.size();
    m_document->setCalculationResult(std::move(result));
    if (!complete) {
        const auto* storedResult = m_document->calculationResult();
        if (storedResult) {
            for (const auto& issue : storedResult->issues) {
                appendOperationLog(tr("%1：%2")
                    .arg(issue.severity == topology::IssueSeverity::Error
                             ? tr("错误") : tr("提示"),
                         issue.message));
            }
        }
    }
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
            tr("已调整 %1 个浮选单元宽度和 %2 条产品线长度")
                .arg(result.resizedUnits).arg(result.resizedConnections),
            3000);
    } else if (result.resizedConnections > 0) {
        statusBar()->showMessage(tr("已调整 %1 条产品线长度").arg(result.resizedConnections), 3000);
    } else if (result.resizedUnits > 0) {
        statusBar()->showMessage(
            tr("已调整 %1 个浮选单元，当前横向长度：%2")
                .arg(result.resizedUnits).arg(result.lastUnitWidth, 0, 'f', 0),
            3000);
    } else if (m_scene->selectedItems().isEmpty()) {
        statusBar()->showMessage(tr("请先选中浮选单元或产品线"), 3000);
    } else {
        statusBar()->showMessage(tr("所选对象不支持此操作，或已达到允许的最小/最大长度"), 3000);
    }
}

void MainWindow::disconnectSelectedLines() {
    auto* flowsheet = static_cast<FlowsheetScene*>(m_scene);
    const int disconnected = CanvasActions::disconnectSelection(*flowsheet);
    statusBar()->showMessage(disconnected > 0
        ? tr("已断开 %1 条浮选单元连接线").arg(disconnected)
        : tr("请先选中已连接的产品线、产品汇流或入料回流汇合点"), 3000);
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
