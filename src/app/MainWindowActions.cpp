#include "app/MainWindow.h"
#include "annotations/AnnotationManager.h"
#include "core/FlotationUnit.h"
#include "document/FlowsheetDocument.h"
#include "editor/CanvasView.h"
#include "services/ProjectUndoManager.h"
#include "ui/ResultMetricMenu.h"

#include <QAction>
#include <QComboBox>
#include <QKeySequence>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QShortcut>
#include <QSignalBlocker>
#include <QToolBar>
#include <QToolButton>

namespace afs {

MainWindow::UndoActions MainWindow::createToolbar() {
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
    auto* copyFlowAction = flowMenu->addAction(tr("复制选中对象 (Ctrl+C)"));
    connect(copyFlowAction, &QAction::triggered, this, [this] { copySelectedFlowGroup(); });
    auto* pasteFlowAction = flowMenu->addAction(tr("粘贴对象 (Ctrl+V)"));
    connect(pasteFlowAction, &QAction::triggered, this, [this] { pasteFlowGroup(); });
    flowMenu->addSeparator();
    auto* addAction = flowMenu->addAction(tr("添加浮选单元"));
    connect(addAction, &QAction::triggered, this, [this] { addFlotationUnit(); });
    auto* ballMillAction = flowMenu->addAction(tr("添加球磨机"));
    ballMillAction->setObjectName("addBallMillAction");
    connect(ballMillAction, &QAction::triggered, this, [this] { addBallMill(); });
    auto* thickenerAction = flowMenu->addAction(tr("添加浓密池"));
    thickenerAction->setObjectName("addThickenerAction");
    connect(thickenerAction, &QAction::triggered, this, [this] { addThickener(); });
    auto* magneticMenu = flowMenu->addMenu(tr("添加磁选机"));
    auto* addWeakMagneticAction = magneticMenu->addAction(tr("添加弱磁选机"));
    connect(addWeakMagneticAction, &QAction::triggered,
            this, [this] { addMagneticSeparator(UnitKind::WeakMagneticSeparation); });
    auto* addStrongMagneticAction = magneticMenu->addAction(tr("添加强磁选机"));
    connect(addStrongMagneticAction, &QAction::triggered,
            this, [this] { addMagneticSeparator(UnitKind::StrongMagneticSeparation); });
    auto* addGenericMagneticAction = magneticMenu->addAction(tr("添加通用磁选机"));
    connect(addGenericMagneticAction, &QAction::triggered,
            this, [this] { addMagneticSeparator(UnitKind::MagneticSeparation); });
    auto* gravityMenu = flowMenu->addMenu(tr("添加重选设备"));
    auto* addSpiralAction = gravityMenu->addAction(tr("添加螺旋溜槽"));
    connect(addSpiralAction, &QAction::triggered,
            this, [this] { addGravitySeparator(UnitKind::SpiralChute); });
    auto* addShakingTableAction = gravityMenu->addAction(tr("添加摇床"));
    connect(addShakingTableAction, &QAction::triggered,
            this, [this] { addGravitySeparator(UnitKind::ShakingTable); });
    auto* addDenseMediumAction = gravityMenu->addAction(tr("添加重介质旋流器"));
    connect(addDenseMediumAction, &QAction::triggered,
            this, [this] { addGravitySeparator(UnitKind::DenseMediumCyclone); });
    auto* addSedimentationAction = gravityMenu->addAction(tr("添加沉降箱"));
    connect(addSedimentationAction, &QAction::triggered,
            this, [this] { addGravitySeparator(UnitKind::SedimentationTank); });
    auto* addDemediumAction = gravityMenu->addAction(tr("添加脱介筛"));
    connect(addDemediumAction, &QAction::triggered,
            this, [this] { addGravitySeparator(UnitKind::DemediumScreen); });
    auto* addThreeProductDemediumAction = gravityMenu->addAction(tr("添加三产品脱介筛"));
    connect(addThreeProductDemediumAction, &QAction::triggered,
            this, [this] { addThreeProductDemediumScreen(); });
    auto* addMediaTankAction = gravityMenu->addAction(tr("添加介质桶"));
    connect(addMediaTankAction, &QAction::triggered,
            this, [this] { addStoragePool(UnitKind::MediaTank); });
    auto* sizingMenu = flowMenu->addMenu(tr("添加筛分分级设备"));
    auto* addScreeningAction = sizingMenu->addAction(tr("添加筛分机"));
    connect(addScreeningAction, &QAction::triggered,
            this, [this] { addSizingSeparator(UnitKind::Screening); });
    auto* addClassificationAction = sizingMenu->addAction(tr("添加分级机"));
    connect(addClassificationAction, &QAction::triggered,
            this, [this] { addSizingSeparator(UnitKind::Classification); });
    auto* addThreeProductScreenAction = sizingMenu->addAction(tr("添加三产品筛分器"));
    connect(addThreeProductScreenAction, &QAction::triggered,
            this, [this] { addThreeProductScreening(); });
    auto* poolMenu = flowMenu->addMenu(tr("添加贮池"));
    auto* addTailingsPoolAction = poolMenu->addAction(tr("添加尾矿池"));
    connect(addTailingsPoolAction, &QAction::triggered,
            this, [this] { addStoragePool(UnitKind::TailingsPool); });
    auto* addConcentratePoolAction = poolMenu->addAction(tr("添加精矿池"));
    connect(addConcentratePoolAction, &QAction::triggered,
            this, [this] { addStoragePool(UnitKind::ConcentratePool); });
    auto* addWaterPoolAction = poolMenu->addAction(tr("添加回水池"));
    connect(addWaterPoolAction, &QAction::triggered,
            this, [this] { addStoragePool(UnitKind::WaterPool); });
    auto* addMixingTankAction = poolMenu->addAction(tr("添加混料桶"));
    connect(addMixingTankAction, &QAction::triggered,
            this, [this] { addStoragePool(UnitKind::MixingTank); });
    auto* addThreeProductAction = flowMenu->addAction(tr("添加三产品浮选单元"));
    connect(addThreeProductAction, &QAction::triggered, this, [this] { addThreeProductUnit(); });
    auto* addSplitterAction = flowMenu->addAction(tr("添加二分流器"));
    connect(addSplitterAction, &QAction::triggered, this, [this] { addBinarySplitter(); });
    addMenuButton(tr("流程编辑"), flowMenu);

    toolbar->addSeparator();
    toolbar->addWidget(new QLabel(tr("试验方案："), toolbar));
    m_scenarioCombo = new QComboBox(toolbar);
    m_scenarioCombo->setObjectName("scenarioCombo");
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
    m_themeAction = displayMenu->addAction(tr("CAD 暗色画布"));
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
    connect(metricButton, &ResultMetricMenu::productNameVisibilityChanged,
            m_annotationManager, &AnnotationManager::setProductNamesVisible);
    connect(metricButton, &ResultMetricMenu::metricLabelModeChanged,
            m_annotationManager, &AnnotationManager::setMetricLabelMode);
    connect(metricButton, &ResultMetricMenu::massUnitChanged,
            m_annotationManager, &AnnotationManager::setMassUnit);
    connect(metricButton, &ResultMetricMenu::customMassUnitChanged,
            m_annotationManager, &AnnotationManager::setCustomMassUnit);
    metricButton->setSettings(m_document->annotationTextSettings());
    connect(m_document, &FlowsheetDocument::annotationTextSettingsChanged,
            metricButton, [this, metricButton] {
                metricButton->setSettings(m_document->annotationTextSettings());
                if (m_annotationsAction) {
                    const QSignalBlocker blocker(m_annotationsAction);
                    m_annotationsAction->setChecked(
                        m_document->annotationTextSettings().resultAnnotationsVisible);
                }
            });
    displayMenu->addMenu(metricButton->metricMenu());

    auto* valueSources = displayMenu->addAction(tr("区分实测值与计算值"));
    valueSources->setObjectName("distinguishValueSourcesAction");
    valueSources->setCheckable(true);
    valueSources->setChecked(m_annotationManager->distinguishValueSources());
    valueSources->setToolTip(tr("含计算质量或计算品位的结果卡使用加粗边框；产率和回收率不参与区分"));
    connect(valueSources, &QAction::toggled,
            m_annotationManager, &AnnotationManager::setDistinguishValueSources);
    connect(m_annotationManager, &AnnotationManager::valueSourcesVisibilityChanged,
            valueSources, [valueSources](bool enabled) {
                const QSignalBlocker blocker(valueSources);
                valueSources->setChecked(enabled);
            });

    auto* borderWidth = displayMenu->addAction(tr("计算值边框线宽…"));
    borderWidth->setObjectName("calculatedResultBorderWidthAction");
    connect(borderWidth, &QAction::triggered, this, [this] {
        bool accepted = false;
        const double width = QInputDialog::getDouble(this, tr("计算值边框线宽"),
            tr("线宽（画布单位）"), m_annotationManager->calculatedResultBorderWidth(),
            ResultBorderStyle::MinimumWidth, ResultBorderStyle::MaximumWidth,
            1, &accepted, Qt::WindowFlags(), 0.1);
        if (accepted) m_annotationManager->setCalculatedResultBorderWidth(width);
    });

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
    // Keep the ordinary text/table clipboard available.  These shortcuts are
    // active only while the canvas (or one of its viewport children) has focus.
    auto* copyFlowShortcut = new QShortcut(QKeySequence::Copy, m_view);
    copyFlowShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(copyFlowShortcut, &QShortcut::activated, this, [this] { copySelectedFlowGroup(); });
    auto* pasteFlowShortcut = new QShortcut(QKeySequence::Paste, m_view);
    pasteFlowShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(pasteFlowShortcut, &QShortcut::activated, this, [this] { pasteFlowGroup(); });

    return {undoAction, redoAction};
}

} // namespace afs
