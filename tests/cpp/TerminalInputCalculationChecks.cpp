#include "app/TerminalTableController.h"
#include "TerminalInputChecks.h"
#include "annotations/AnnotationManager.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/FeedJunctionItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/ProductLineItem.h"
#include "services/FlowsheetCalculationService.h"
#include "ui/TerminalProductDock.h"
#include "ui/TerminalProductTableModel.h"
#include <cmath>

namespace afs::tests {
int runTerminalCalculationChecks(TerminalInputContext& context) {
    auto& calculationScene = context.calculationScene;
    auto*& calculationUnit = context.calculationUnit;
    calculationUnit = new FlotationUnitItem({"calculation", {0, 0}});
    calculationScene.addItem(calculationUnit);
    context.calculationSnapshot = CanvasTopologyBuilder::build(calculationScene);
    FlowsheetDocument calculationDocument;
    calculationDocument.setDryMass("calculation:left", 10.0);
    calculationDocument.setGradePercent("calculation:left", 5.0);
    calculationDocument.setDryMass("calculation:right", 90.0);
    calculationDocument.setGradePercent("calculation:right", 1.0);
    calculationDocument.setComponents({{"component-1", "Cu"}, {"component-2", "Pb"}});
    calculationDocument.setGradePercent("calculation:left", "component-2", 2.0);
    calculationDocument.setGradePercent("calculation:right", "component-2", 0.5);
    auto& result = context.result;
    result = FlowsheetCalculationService::calculate(CanvasTopologyBuilder::build(calculationScene), calculationDocument);
    if (!result.complete || !result.values.contains("calculation:feed")) return 16;
    if (result.components.size() != 2
        || !result.components["component-1"].values.contains("calculation:feed")
        || !result.components["component-2"].values.contains("calculation:feed")
        || std::abs(result.components["component-1"].values["calculation:feed"].gradePercent()
                    - 1.4) > 0.001
        || std::abs(result.components["component-2"].values["calculation:feed"].gradePercent()
                    - 0.65) > 0.001) return 50;

    FlowsheetDocument flexibleDocument;
    flexibleDocument.setDryMass("calculation:feed", 100.0);
    flexibleDocument.setGradePercent("calculation:feed", 2.0);
    flexibleDocument.setDryMass("calculation:left", 30.0);
    flexibleDocument.setGradePercent("calculation:left", 5.0);
    const auto flexibleResult = FlowsheetCalculationService::calculate(CanvasTopologyBuilder::build(calculationScene), flexibleDocument);
    if (!flexibleResult.complete || !flexibleResult.fullySolved
        || flexibleResult.dryMassDegreesOfFreedom != 0
        || flexibleResult.componentMassDegreesOfFreedom != 0
        || std::abs(flexibleResult.values["calculation:right"].dryMass - 70.0) > 0.001
        || std::abs(flexibleResult.values["calculation:right"].gradePercent()
                    - (0.5 / 70.0 * 100.0)) > 0.001) return 53;

    FlowsheetScene propagationScene;
    auto* cleaner = new FlotationUnitItem({"cleaner", {0, 0}});
    auto* scavenger = new FlotationUnitItem({"scavenger", {400, 300}});
    propagationScene.addItem(cleaner);
    propagationScene.addItem(scavenger);
    if (!propagationScene.connectProduct(cleaner->products().at(1), scavenger->inputLine()))
        return 59;
    FlowsheetDocument propagationDocument;
    propagationDocument.setDryMass("scavenger:left", 35.0);
    propagationDocument.setGradePercent("scavenger:left", 4.0);
    propagationDocument.setDryMass("scavenger:right", 65.0);
    propagationDocument.setGradePercent("scavenger:right", 1.0);
    const auto propagationResult = FlowsheetCalculationService::calculate(CanvasTopologyBuilder::build(propagationScene), propagationDocument);
    if (!propagationResult.values.contains("cleaner:right")
        || std::abs(propagationResult.values["cleaner:right"].dryMass - 100.0) > 0.001
        || std::abs(propagationResult.values["cleaner:right"].gradePercent() - 2.05) > 0.001)
        return 60;
    if (propagationResult.complete) return 65;
    propagationDocument.setCalculationResult(propagationResult);
    AnnotationManager partialAnnotations(propagationScene, propagationDocument);
    partialAnnotations.synchronize();
    if (partialAnnotations.annotationCount() != propagationResult.values.size()) return 66;
    FlowsheetScene boundaryScene;
    auto* boundaryA = new FlotationUnitItem({"boundary-a", {0, 0}});
    auto* boundaryB = new FlotationUnitItem({"boundary-b", {400, 300}});
    auto* boundaryC = new FlotationUnitItem({"boundary-c", {800, 600}});
    boundaryScene.addItem(boundaryA);
    boundaryScene.addItem(boundaryB);
    boundaryScene.addItem(boundaryC);
    if (!boundaryScene.connectProduct(boundaryA->products().at(1), boundaryB->inputLine())
        || !boundaryScene.connectProduct(boundaryB->products().at(1), boundaryC->inputLine())
        || !boundaryScene.connectProduct(boundaryC->products().at(0), boundaryA->inputLine()))
        return 74;
    FlowsheetDocument boundaryDocument;
    boundaryDocument.setDryMass("boundary-a:left", 1600.0);
    boundaryDocument.setGradePercent("boundary-a:left", 3.2125);
    boundaryDocument.setDryMass("boundary-b:left", 1000.0);
    boundaryDocument.setGradePercent("boundary-b:left", 1.0);
    boundaryDocument.setDryMass("boundary-c:right", 4000.0);
    boundaryDocument.setGradePercent("boundary-c:right", 0.6);
    boundaryDocument.setDryMass("boundary-c:left", 700.0);
    boundaryDocument.setGradePercent("boundary-c:left", 0.8);
    const auto boundaryResult = FlowsheetCalculationService::calculate(CanvasTopologyBuilder::build(boundaryScene), boundaryDocument);
    const auto boundaryJunction = boundaryA->inputLine()->feedJunction();
    if (!boundaryJunction
        || !boundaryResult.values.contains(boundaryJunction->externalFeedStreamId())
        || !boundaryResult.values.contains(boundaryJunction->outputStreamId())
        || !boundaryResult.values.contains("boundary-b:left")
        || !boundaryResult.values.contains("boundary-c:right")
        || boundaryResult.dryMassDegreesOfFreedom != 0) return 75;

    return 0;
}

int runTerminalClosedLoopChecks() {
    FlowsheetScene closedScene;
    auto* closedUpper = new FlotationUnitItem({"closed-upper", {0, 0}});
    auto* closedLower = new FlotationUnitItem({"closed-lower", {500, 400}});
    closedScene.addItem(closedUpper);
    closedScene.addItem(closedLower);
    if (!closedScene.connectProduct(closedUpper->products().at(1), closedLower->inputLine())
        || !closedScene.connectProduct(closedLower->products().at(0), closedUpper->inputLine()))
        return 25;
    const auto closedSnapshot = CanvasTopologyBuilder::build(closedScene);
    if (closedSnapshot.terminalProducts.size() != 2) return 26;
    FlowsheetDocument closedDocument;
    TerminalTableController closedSession(closedDocument);
    TerminalProductDock closedDock(closedSession);
    closedSession.setStreams(closedSnapshot.reportStreams);
    if (closedDock.model()->rowCount() != closedSnapshot.reportStreams.size()
        || closedDock.model()->editableCount() != closedSnapshot.reportStreams.size()) return 27;
    closedDocument.setDryMass("closed-upper:left", 10.0);
    closedDocument.setGradePercent("closed-upper:left", 5.0);
    closedDocument.setDryMass("closed-lower:right", 40.0);
    closedDocument.setGradePercent("closed-lower:right", 1.0);
    closedDocument.setDryMass("closed-lower:left", 50.0);
    closedDocument.setGradePercent("closed-lower:left", 2.0);
    const auto closedResult = FlowsheetCalculationService::calculate(CanvasTopologyBuilder::build(closedScene), closedDocument);
    if (!closedResult.complete || closedResult.values.size() != closedSnapshot.graph.streamIds().size())
        return 28;
    closedDocument.setCalculationResult(closedResult);
    AnnotationManager closedAnnotations(closedScene, closedDocument);
    closedAnnotations.synchronize();
    if (closedAnnotations.annotationCount() != closedResult.values.size()) return 29;
    return 0;
}
}
