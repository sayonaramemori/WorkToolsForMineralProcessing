#include "WorkspaceToolsTestSupport.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/ProductLineItem.h"
#include "graphics/items/FeedJunctionItem.h"
#include "services/CalculationDiagnostics.h"
#include "ui/CalculationIssuesDock.h"
#include "ui/TerminalProductDock.h"
#include "ui/TerminalProductTableModel.h"
#include "ui/TerminalProductViewSupport.h"
#include <QAction>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTableView>
#include <QTreeWidget>
using namespace afs;
using namespace afs::tests;
namespace {
QTreeWidgetItem* issue(QTreeWidget& tree, const QString& stream, const QString& component = {}) {
    for (int i = 0; i < tree.topLevelItemCount(); ++i) {
        auto* item = tree.topLevelItem(i);
        if (item->data(1, Qt::UserRole).toString() == stream
            && (component.isEmpty() || item->data(2, Qt::UserRole).toString() == component)) return item;
    }
    return nullptr;
}
void diagnosticObjectLabels() {
    CanvasTopologySnapshot snapshot;
    snapshot.graph.addFlotationNode({"unit"});
    snapshot.graph.addStoragePoolNode({"buffer"});
    snapshot.graph.addMergeNode({"products", topology::MergeRole::ProductMerge});
    snapshot.graph.addMergeNode({"feed", topology::MergeRole::FeedJunction});
    snapshot.reportStreams.append({"unit:feed", "单元入料", nullptr, false, false, true, false, {"unit"}});
    FlowsheetDocument document;
    topology::CalculationResult result;
    for (const auto& id : {"unit", "buffer", "products", "feed", "equation-system"})
        result.issues.append({topology::IssueSeverity::Warning, topology::IssueCode::Underdetermined, id, "检查约束"});
    const auto entries = CalculationDiagnostics::build(snapshot, document, result);
    require(entries.size() == 5 && entries[0].title == "流程单元 · unit"
        && entries[0].streamId == "unit:feed" && entries[1].title == "流程单元 · buffer"
        && entries[2].title == "产品汇流 · products" && entries[3].title == "入料汇流 · feed"
        && entries[4].title == "全局约束" && entries[4].streamId.isEmpty(),
        "Diagnostic node labels or input links depend on a removed UI catalogue");
}
void calculationPanelNavigation() {
    ToolsUi ui;
    auto* sidebar = ui.window.findChild<WorkspaceSidebar*>();
    auto* calculate = ui.window.findChild<QPushButton*>("calculateButton");
    auto* tree = ui.window.findChild<QTreeWidget*>("calculationIssueTree");
    const auto hasIssue = [&](topology::IssueCode code, topology::IssueSeverity severity) {
        const auto* result = ui.document->calculationResult();
        if (result) for (const auto& issue : result->issues)
            if (issue.code == code && issue.severity == severity) return true;
        return false;
    };
    const auto compute = [&] {
        sidebar->showPanel("table"); flush();
        require(calculate && calculate->isVisible() && calculate->isEnabled(), "Calculate button is unavailable");
        QTest::mouseClick(calculate, Qt::LeftButton); flush();
        require(ui.document->calculationResult(), "Calculate button did not calculate");
    };
    ui.document->setDryMass("1:left", 30); ui.document->setGradePercent("1:left", 2);
    compute();
    require(hasIssue(topology::IssueCode::Underdetermined, topology::IssueSeverity::Warning)
        && ui.document->calculationResult()->values.contains("1:left")
        && tree->topLevelItemCount() > 0 && sidebar->currentKey() == "table",
        "Partial underdetermination must update diagnostics without leaving the table");
    // The explicit menu command still opens the requested diagnostics page.
    ui.window.findChild<QAction*>("checkFlowsheetAction")->trigger(); flush();
    require(sidebar->currentKey() == "issues", "Explicit diagnostics command did not open its panel");
    ui.document->setDryMass("1:right", 70); ui.document->setGradePercent("1:right", 2);
    compute();
    require(ui.document->calculationResult()->complete && tree->topLevelItemCount() == 0
        && sidebar->currentKey() == "table", "Successful calculation changed the active panel");
    ui.document->setDryMass("1:feed", 120); ui.document->setGradePercent("1:feed", 2);
    compute();
    require(hasIssue(topology::IssueCode::InconsistentBalance, topology::IssueSeverity::Error)
        && sidebar->currentKey() == "issues", "Mass contradiction did not reveal diagnostics");
    ui.document->setDryMass("1:feed", 100); ui.document->setGradePercent("1:feed", 5);
    compute();
    require(hasIssue(topology::IssueCode::InconsistentBalance, topology::IssueSeverity::Error)
        && sidebar->currentKey() == "issues", "Component contradiction did not reveal diagnostics");
    ui.document->setGradePercent("1:feed", 2); compute();
    require(ui.document->calculationResult()->complete && sidebar->currentKey() == "table",
        "Correcting the contradiction still redirected the table");

    // Reconciliation uses the same issue code for a large-residual warning;
    // that warning alone is not a contradictory equation system.
    ui.document->setCalculationMode(CalculationMode::DataReconciliation);
    ui.document->setDryMass("1:feed", 200); compute();
    require(ui.document->calculationResult()->reconciled
        && hasIssue(topology::IssueCode::InconsistentBalance, topology::IssueSeverity::Warning)
        && !hasIssue(topology::IssueCode::InconsistentBalance, topology::IssueSeverity::Error)
        && sidebar->currentKey() == "table", "Reconciliation warning redirected the table");

    // A contradiction in a secondary component must also reveal diagnostics,
    // even when another component still has computable results.
    ui.document->setCalculationMode(CalculationMode::Strict);
    ui.document->setDryMass("1:feed", 100);
    ui.document->setComponents({{"component-1", "Cu"}, {"zn", "Zn"}});
    ui.document->setGradePercent("1:feed", "zn", 5);
    ui.document->setGradePercent("1:left", "zn", 1);
    ui.document->setGradePercent("1:right", "zn", 1); compute();
    require(ui.document->calculationResult()->components.value("component-1").complete
        && hasIssue(topology::IssueCode::InconsistentBalance, topology::IssueSeverity::Error)
        && sidebar->currentKey() == "issues", "Secondary component contradiction did not reveal diagnostics");
}
void diagnostics() {
    ToolsUi ui; auto* check = ui.window.findChild<QAction*>("checkFlowsheetAction"); require(check, "No diagnostic action");
    auto* panel = ui.window.findChild<CalculationIssuesDock*>(); auto* tree = panel->findChild<QTreeWidget*>("calculationIssueTree");
    auto* table = ui.window.findChild<QTableView*>("terminalProductTable"); auto* filter = ui.window.findChild<QComboBox*>("streamFilterCombo");
    ui.document->setDryMass("1:left", 30); check->trigger(); flush();
    auto* target = issue(*tree, "1:left"); require(target && target->data(2, Qt::UserRole + 1).toBool(), "Missing grade diagnostic is not linked to grade field");
    preview(ui.window, "diagnostics");
    filter->setCurrentIndex(filter->findData(StreamFilterProxyModel::Feed));
    tree->scrollToItem(target); flush();
    QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, tree->visualItemRect(target).center()); flush();
    require(filter->currentData().toInt() == StreamFilterProxyModel::All, "Issue navigation did not reveal filtered target");
    auto* dock = ui.window.findChild<TerminalProductDock*>();
    const auto current = table->model()->data(table->model()->index(table->currentIndex().row(), 0)).toString();
    require(table->currentIndex().column() == dock->model()->gradeColumn("component-1") && current.contains("1"),
        "Issue did not focus the missing grade input");
    require(unit(*ui.scene, "1")->products().front()->isSelected(), "Diagnostic did not highlight canvas stream");
    QTest::mouseDClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, tree->visualItemRect(target).center()); flush();
    auto* editor = qobject_cast<QLineEdit*>(QApplication::focusWidget()); require(editor, "Double-click diagnostic did not enter table editor");
    editor->setText("2.5"); QTest::keyClick(editor, Qt::Key_Return); flush();
    require(ui.document->measurement("1:left").grade("component-1") == 2.5 && tree->topLevelItemCount() == 0,
        "Diagnostic edit did not update input or invalidate stale issues");
    ui.document->setDryMass("1:feed", 100); ui.document->setGradePercent("1:feed", 2);
    check->trigger(); flush(); require(ui.document->calculationResult()->complete && tree->topLevelItemCount() == 0, "Solved calculation still reports stale issues");
    ui.document->setComponents({{"component-1", "Cu"}, {"zn", "Zn"}}); check->trigger(); flush();
    require(issue(*tree, "1:feed", "zn"), "Multi-component issue lost component-specific input link");
    ui.document->setDryMass("1:right", 90); ui.document->setGradePercent("1:right", 2); check->trigger(); flush();
    bool global = false;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) global |= tree->topLevelItem(i)->data(0, Qt::UserRole) == "equation-system";
    require(global, "Global conflict diagnostic is missing");
    auto* next = new FlotationUnitItem({"next", {600, 300}}); ui.scene->addItem(next);
    auto* source = unit(*ui.scene, "1");
    require(ui.scene->connectProductDirect(source->products().back(), next->inputLine()), "Could not create diagnostic feed fixture");
    auto* feed = ui.scene->connectRecycle(source->products().front(), next->inputLine());
    require(feed, "Could not create diagnostic branch");
    check->trigger(); flush(); target = issue(*tree, "1:left", "zn"); require(target, "No diagnostic for recycled stream");
    tree->scrollToItem(target); flush();
    QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, tree->visualItemRect(target).center()); flush();
    require(feed->isSelected() && feed->selectedSourceStreamId() == "1:left", "Issue navigation lost recycled branch highlight");
}
}
void runCalculationDiagnosticsChecks() { diagnosticObjectLabels(); calculationPanelNavigation(); diagnostics(); }
