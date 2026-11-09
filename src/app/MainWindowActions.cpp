#include "app/MainWindow.h"
#include "document/FlowsheetDocument.h"
#include "ui/WorkspacePanelSelector.h"
#include <QAction>
#include <QComboBox>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
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
    const auto undoActions = populateFlowMenu(flowMenu);
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
    populateDisplayMenu(displayMenu, toolbar);
    auto* exportMenu = new QMenu(tr("导出选项"), toolbar);
    auto* exportAction = exportMenu->addAction(tr("导出流程图"));
    connect(exportAction, &QAction::triggered, this, [this] { exportScene(); });
    auto* exportDataAction = exportMenu->addAction(tr("导出 Excel"));
    connect(exportDataAction, &QAction::triggered, this, [this] { exportExcelData(); });
    auto* exportOrderAction = exportMenu->addAction(tr("设置 Excel 行顺序"));
    connect(exportOrderAction, &QAction::triggered,
            this, [this] { editExcelExportOrder(); });

    addMenuButton(tr("显示设置"), displayMenu);
    toolbar->addWidget(new WorkspacePanelSelector(*m_workspaceSidebar, toolbar));
    addMenuButton(tr("导出选项"), exportMenu);
    setupCanvasShortcuts();
    return undoActions;
}
} // namespace afs
