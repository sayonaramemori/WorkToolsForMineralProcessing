#include "WorkspaceToolsTestSupport.h"
#include "services/ProjectSerializer.h"
#include "ui/CalculationIssuesDock.h"
#include "ui/FlowTemplateDock.h"
#include "ui/OperationLogDock.h"
#include "ui/TerminalProductDock.h"
#include "ui/TerminalProductViewSupport.h"
#include <QAction>
#include <QComboBox>
#include <QHeaderView>
#include <QMenu>
#include <QPlainTextEdit>
#include <QStackedWidget>
#include <QTableView>
#include <QTextDocument>
#include <QToolButton>
#include <cmath>

using namespace afs;
using namespace afs::tests;

void runWorkspaceSidebarChecks() {
    ToolsUi ui;
    auto* sidebar = ui.window.findChild<WorkspaceSidebar*>();
    auto* stack = sidebar->findChild<QStackedWidget*>("workspacePanelStack");
    auto* tablePanel = ui.window.findChild<TerminalProductDock*>();
    auto* properties = ui.window.findChild<ObjectPropertiesDock*>();
    auto* issues = ui.window.findChild<CalculationIssuesDock*>();
    auto* templates = ui.window.findChild<FlowTemplateDock*>();
    auto* log = dynamic_cast<OperationLogDock*>(ui.window.findChild<QDockWidget*>("operationLogDock"));
    const QVector<QDockWidget*> panels{tablePanel, properties, issues, templates};
    auto* logArea = sidebar->findChild<QWidget*>("workspaceLogArea");
    require(sidebar && stack->count() == 4 && sidebar->currentKey() == "table" && tablePanel->isVisible(),
            "Right workspace must open on the table");
    require(logArea && log->isVisible() && !sidebar->panelAction("log"), "Log is not permanently visible outside the page switcher");
    require(ui.window.dockWidgetArea(sidebar) == Qt::RightDockWidgetArea, "Workspace is not on the right");
    for (auto* panel : panels)
        require(ui.window.dockWidgetArea(panel) == Qt::NoDockWidgetArea && !panel->isFloating(),
                "A function panel remains independently docked or floating");
    const auto canvasGeometry = ui.canvas->geometry();
    require(canvasGeometry.left() <= 2 && canvasGeometry.right() < sidebar->geometry().left()
        && canvasGeometry.top() == sidebar->geometry().top() && canvasGeometry.bottom() == sidebar->geometry().bottom(),
        "Canvas does not occupy the full left workspace");
    const auto before = ProjectSerializer::serialize(*ui.scene, *ui.document);
    QToolButton* displayButton = nullptr;
    for (auto* button : ui.window.findChildren<QToolButton*>())
        if (button->text() == "显示设置") displayButton = button;
    require(displayButton && displayButton->menu(), "Missing display settings menu");
    auto* menu = displayButton->menu();
    for (const auto& key : {"table", "properties", "issues", "templates", "log"})
        require(!sidebar->findChild<QToolButton*>(QString("workspaceButton_") + key),
                "Right sidebar still contains a panel switch button");
    require(stack->geometry().top() <= 2 && stack->geometry().bottom() < logArea->geometry().top()
        && logArea->geometry().bottom() >= sidebar->widget()->height() - 2,
            "Removed switch buttons left empty space above the panel");
    const auto checkLogRatio = [&] {
        require(std::abs(logArea->height() - sidebar->widget()->height() / 8.0) <= 2,
                QString("Fixed log does not occupy one eighth of the right workspace: log %1 / total %2, page %3")
                    .arg(logArea->height()).arg(sidebar->widget()->height()).arg(sidebar->currentKey()));
    };
    checkLogRatio();
    for (const auto& key : {"properties", "issues", "templates", "table", "table"}) {
        auto* action = sidebar->panelAction(key);
        require(action && menu->actions().contains(action), "Missing panel action in display settings");
        menu->popup(displayButton->mapToGlobal(QPoint(0, displayButton->height()))); flush();
        QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier, menu->actionGeometry(action).center()); flush();
        int visible = 0, checked = 0;
        for (auto* panel : panels) visible += panel->isVisible();
        for (const auto& panelKey : {"table", "properties", "issues", "templates"})
            checked += sidebar->panelAction(panelKey)->isChecked();
        require(visible == 1 && checked == 1 && sidebar->currentKey() == key,
                "Switching does not show exactly one checked, visible panel");
        require(sidebar->currentPanel()->geometry() == stack->contentsRect(), "Selected panel does not fill the right area");
        require(ui.canvas->geometry() == canvasGeometry, "Panel switching resized the canvas");
        require(log->isVisible(), "Panel switching hid the fixed log"); checkLogRatio();
    }
    require(ProjectSerializer::serialize(*ui.scene, *ui.document) == before, "Panel switching modified project data");

    auto* table = ui.window.findChild<QTableView*>("terminalProductTable");
    auto* filter = ui.window.findChild<QComboBox*>("streamFilterCombo");
    filter->setCurrentIndex(filter->findData(StreamFilterProxyModel::Feed));
    table->horizontalHeader()->resizeSection(0, 333);
    table->setCurrentIndex(table->model()->index(0, 2));
    sidebar->showPanel("properties"); log->appendMessage("background log update");
    auto* root = unit(*ui.scene, "1"); root->setSelected(true); flush();
    require(sidebar->currentKey() == "properties", "Selection or log updates switched the active panel");
    ui.document->setProductName("1:left", "Named product"); flush();
    require(sidebar->currentKey() == "properties" && log->isVisible(), "Background property refresh stole the active panel or hid the log");
    auto* output = log->findChild<QPlainTextEdit*>("operationLog");
    require(output && output->isReadOnly() && output->toPlainText().contains("background log update"), "Fixed log did not receive messages");
    sidebar->panelAction("table")->trigger(); flush();
    require(filter->currentData() == StreamFilterProxyModel::Feed && table->horizontalHeader()->sectionSize(0) == 333
        && table->currentIndex().isValid(), "Switching discarded table filters, widths or selection");
    preview(ui.window, "right-table");
    sidebar->showPanel("properties"); flush(); preview(ui.window, "right-properties");
    for (const QSize size : {QSize(1440, 900), QSize(1600, 800), QSize(1800, 1100)}) {
        ui.window.resize(size); flush(); checkLogRatio();
        require(ui.canvas->geometry().top() == sidebar->geometry().top()
            && ui.canvas->geometry().bottom() == sidebar->geometry().bottom(), "Log shortened the left canvas");
    }
    for (int i = 0; i < 520; ++i) log->appendMessage(QString("entry %1").arg(i));
    require(output->document()->blockCount() == 500 && output->toPlainText().contains("entry 519"), "Fixed log lost its capacity limit or newest message");
    preview(ui.window, "right-fixed-log");
    ToolsUi another;
    require(another.window.findChild<WorkspaceSidebar*>()->currentKey() == "table", "A new window did not default to the table");
}
