#include "WorkspaceToolsTestSupport.h"
#include "services/ProjectSerializer.h"
#include "ui/OperationLogDock.h"
#include <QAbstractItemView>
#include <QAction>
#include <QComboBox>
#include <QMenu>
#include <QSignalSpy>
#include <QToolBar>
#include <QToolButton>
#include <QUndoStack>
#include <QWheelEvent>

using namespace afs;
using namespace afs::tests;

void runWorkspacePanelSelectorChecks() {
    ToolsUi ui;
    auto* sidebar = ui.window.findChild<WorkspaceSidebar*>();
    auto* combo = ui.window.findChild<QComboBox*>("workspacePanelCombo");
    auto* toolbar = ui.window.findChild<QToolBar*>("mainToolbar");
    QToolButton* display = nullptr;
    for (auto* button : toolbar->findChildren<QToolButton*>())
        if (button->text() == "显示设置") display = button;
    require(combo && display, "Missing toolbar panel selector");
    const auto actions = toolbar->actions();
    int displayIndex = -1;
    for (int i = 0; i < actions.size(); ++i) if (toolbar->widgetForAction(actions[i]) == display) displayIndex = i;
    require(displayIndex >= 0 && displayIndex + 1 < actions.size()
        && toolbar->widgetForAction(actions[displayIndex + 1]) == combo, "Panel selector is not immediately after display settings");
    const QStringList keys{"table", "properties", "issues", "templates"};
    require(combo->count() == keys.size() && combo->currentData() == "table", "Panel selector must default to the table");
    for (int i = 0; i < keys.size(); ++i)
        require(combo->itemData(i) == keys[i] && combo->itemText(i) == sidebar->panelAction(keys[i])->text(),
                "Panel selector disagrees with the sidebar page registry");
    require(combo->mapTo(toolbar, QPoint(0, 0)).x() >= display->mapTo(toolbar, QPoint(0, 0)).x() + display->width(),
            "Panel selector is not next to display settings");
    const auto before = ProjectSerializer::serialize(*ui.scene, *ui.document);
    const auto undoIndex = ui.undo->stack()->index();
    QSignalSpy projects(ui.document, &FlowsheetDocument::projectChanged);
    QSignalSpy calculations(ui.document, &FlowsheetDocument::calculationChanged);
    int checkNumber = 0;
    const auto check = [&](const QString& key) {
        ++checkNumber;
        require(sidebar->currentKey() == key && combo->currentData() == key && sidebar->panelAction(key)->isChecked(),
                QString("Panel selector out of sync at check %1: expected %2, page %3, combo %4, menu %5")
                    .arg(checkNumber).arg(key, sidebar->currentKey(), combo->currentData().toString()).arg(sidebar->panelAction(key)->isChecked()));
        int checked = 0, visible = 0;
        for (const auto& page : keys) checked += sidebar->panelAction(page)->isChecked();
        for (auto* panel : sidebar->findChildren<QDockWidget*>())
            if (panel->isVisible() && panel->objectName() != "operationLogDock") ++visible;
        require(checked == 1 && visible == 1, "Panel switching lost exclusive visibility");
        require(ui.window.findChild<QDockWidget*>("operationLogDock")->isVisible(), "Panel switching hid the fixed log");
    };
    check("table");
    // Exercise the actual popup and menu, rather than setting their indexes.
    QTest::mouseClick(combo, Qt::LeftButton, Qt::NoModifier, QPoint(combo->width() - 10, combo->height() / 2)); flush();
    require(combo->view()->isVisible(), "Panel selector popup did not open");
    const auto row = combo->model()->index(combo->findData("templates"), combo->modelColumn());
    combo->view()->scrollTo(row); flush();
    QTest::mouseMove(combo->view()->viewport(), combo->view()->visualRect(row).center());
    QTest::mouseClick(combo->view()->viewport(), Qt::LeftButton, Qt::NoModifier, combo->view()->visualRect(row).center()); flush();
    check("templates");
    auto* menu = display->menu();
    menu->popup(display->mapToGlobal(QPoint(0, display->height()))); flush();
    QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier, menu->actionGeometry(sidebar->panelAction("properties")).center()); flush();
    check("properties");
    sidebar->showPanel("issues"); flush(); check("issues");
    sidebar->showPanel("table"); flush();
    ui.canvas->setFocus(); flush();
    const auto wheel = [&](int vertical, int horizontal = 0, Qt::ScrollPhase phase = Qt::NoScrollPhase) {
        const QPointF local = combo->rect().center();
        QWheelEvent event(local, combo->mapToGlobal(local.toPoint()), {}, QPoint(horizontal, vertical),
                          Qt::NoButton, Qt::NoModifier, phase, false);
        QApplication::sendEvent(combo, &event); flush();
        require(ui.canvas->hasFocus(), "Hover-wheel switching stole the canvas keyboard focus");
    };
    wheel(-120); check("properties");
    wheel(-120); check("issues");
    wheel(-120); check("templates");
    wheel(-120); check("templates"); // Bounds do not wrap.
    wheel(120); check("issues");
    wheel(240); check("table");
    wheel(120); check("table");
    wheel(-60); check("table");
    wheel(-60); check("properties");
    wheel(-180); check("issues");
    wheel(-60); check("templates"); // Preserve the remainder of a partial notch.
    sidebar->showPanel("properties"); flush();
    wheel(60); check("properties");
    wheel(-60); check("properties"); // Reversing direction discards the old fraction.
    wheel(-60); check("issues");
    wheel(0, 120); check("issues");
    wheel(-60); check("issues");
    wheel(0, 0, Qt::ScrollEnd); check("issues");
    wheel(-60); check("issues");
    wheel(-60); check("templates");
    wheel(60); check("templates");
    wheel(60, 0, Qt::ScrollEnd); check("issues");
    require(ProjectSerializer::serialize(*ui.scene, *ui.document) == before && projects.isEmpty()
        && calculations.isEmpty() && ui.undo->stack()->index() == undoIndex,
        "Panel selector changed project data, calculation or undo history");
    sidebar->showPanel("table"); flush(); preview(ui.window, "right-panel-selector");
}
