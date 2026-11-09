#include "WorkspaceToolsTestSupport.h"
#include "app/CalculationController.h"
#include "adapters/CanvasTopologyBuilder.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/ProductLineItem.h"
#include "services/FlowsheetCalculationService.h"
#include "services/ProjectSerializer.h"
#include "ui/TerminalProductDock.h"
#include <QComboBox>
#include <QPushButton>
#include <QSignalSpy>
#include <cmath>

using namespace afs;
using namespace afs::tests;
namespace {
void detachedSnapshot() {
    FlowsheetSnapshot snapshot;
    {
        FlowsheetScene scene;
        auto* root = new FlotationUnitItem({"detached", {0, 0}});
        scene.addItem(root);
        snapshot = CanvasTopologyBuilder::build(scene);
        const auto index = CanvasTopologyBuilder::buildItemIndex(scene);
        require(index.itemForStream("detached:feed") == root->inputLine()
            && index.itemForStream("detached:left") == root->products().front()
            && index.streamsForItem(root->products().front()) == QStringList{"detached:left"},
            "Canvas navigation index does not resolve stable stream IDs");
        scene.clear();
        require(!CanvasTopologyBuilder::buildItemIndex(scene).itemForStream("detached:left"),
            "New navigation index retained a deleted graphics item");
    }
    FlowsheetDocument document;
    document.setDryMass("detached:left", 30); document.setGradePercent("detached:left", 4);
    document.setDryMass("detached:right", 70); document.setGradePercent("detached:right", 1);
    const auto result = FlowsheetCalculationService::calculate(snapshot, document);
    require(result.complete && result.values.value("detached:feed").dryMass == 100,
            "Value snapshot could not calculate after its canvas was destroyed");
}
void previews() {
    ToolsUi ui;
    auto* controller = ui.window.findChild<CalculationController*>();
    auto* calculate = ui.window.findChild<QPushButton*>("calculateButton");
    auto* filter = ui.window.findChild<QComboBox*>("streamFilterCombo");
    require(controller && calculate && filter, "Calculation controller was not assembled");
    QSignalSpy states(controller, &CalculationController::stateChanged);
    ui.document->setDryMass("1:left", 30); ui.document->setGradePercent("1:left", 2);
    ui.document->setDryMass("1:right", 70); ui.document->setGradePercent("1:right", 2);
    require(states.isEmpty(), "Input previews were not coalesced until the event loop");
    const auto beforePreview = ProjectSerializer::serialize(*ui.scene, *ui.document);
    QSignalSpy projectChanges(ui.document, &FlowsheetDocument::projectChanged);
    flush();
    require(states.size() == 1 && calculate->isEnabled() && calculate->text() == "计算"
        && !ui.document->calculationResult() && controller->displayResult().complete
        && std::abs(controller->displayResult().values.value("1:feed").dryMass - 100) < 1e-8,
        "Coalesced preview did not show current input readiness");
    require(projectChanges.isEmpty() && ProjectSerializer::serialize(*ui.scene, *ui.document) == beforePreview,
            "Preview was persisted, dirtied the project or changed calculation results");
    states.clear();
    filter->setCurrentIndex(filter->findText("终端产品"));
    ui.scene->clearSelection(); unit(*ui.scene, "1")->setSelected(true); flush();
    require(states.isEmpty(), "Table filtering or canvas selection refreshed the calculation preview");
    controller->displayResult(); controller->displayResult(); flush();
    require(states.isEmpty() && projectChanges.isEmpty(), "Reading a cached preview modified application state");
    QTest::mouseClick(calculate, Qt::LeftButton); flush();
    require(ui.document->calculationResult() && ui.document->calculationResult()->complete
        && calculate->text() == "重新计算", "Explicit calculate did not commit its result");
    ui.document->setDryMass("1:right", 80);
    const auto beforeRefresh = ProjectSerializer::serialize(*ui.scene, *ui.document);
    flush();
    require(!ui.document->calculationResult()
        && std::abs(controller->displayResult().values.value("1:feed").dryMass - 110) < 1e-8
        && ProjectSerializer::serialize(*ui.scene, *ui.document) == beforeRefresh,
        "Changing inputs reused a stale preview or saved a preview as a result");
    const int empty = ui.document->addScenario("空白", false);
    ui.document->setCurrentScenario(empty); flush();
    require(!calculate->isEnabled() && controller->displayResult().values.isEmpty(),
            "Scenario switch retained another scenario's preview");
    ui.document->setCurrentScenario(0); flush();
    require(calculate->isEnabled() && controller->displayResult().values.value("1:feed").dryMass == 110,
            "Returning to the original scenario did not refresh its preview");
    ui.document->setComponents({{"component-1", "Cu"}, {"zn", "Zn"}});
    ui.document->setGradePercent("1:left", "zn", 3); flush();
    require(!controller->displayResult().complete && controller->displayResult().components.contains("zn"),
            "Changing components did not invalidate preview inputs");
    ui.document->setGradePercent("1:right", "zn", 1);
    ui.document->setCalculationMode(CalculationMode::DataReconciliation); flush();
    require(controller->displayResult().reconciled && controller->displayResult().complete,
            "Changing calculation mode did not refresh the preview");
}
}
void runCalculationControllerChecks() { detachedSnapshot(); previews(); }
