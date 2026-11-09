#include "document/FlowsheetDocument.h"
#include "document/FlowsheetSnapshot.h"
#include "services/CalculationInputBuilder.h"
#include "services/FlowsheetCalculationService.h"
#include <QCoreApplication>
#include <QDebug>
#include <cmath>

using namespace afs;
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    FlowsheetSnapshot snapshot;
    snapshot.graph.addFlotationNode({"unit"});
    snapshot.graph.addStream({"feed", std::nullopt, topology::PortRef{"unit", topology::PortKind::Feed}});
    snapshot.graph.addStream({"left", topology::PortRef{"unit", topology::PortKind::LeftProduct}, std::nullopt});
    snapshot.graph.addStream({"right", topology::PortRef{"unit", topology::PortKind::RightProduct}, std::nullopt});
    snapshot.reportStreams = {{"feed", "入料"}, {"left", "左产品"}, {"right", "右产品"}};
    snapshot.productStreams = {snapshot.reportStreams[1], snapshot.reportStreams[2]};
    snapshot.terminalProducts = snapshot.productStreams;
    FlowsheetDocument document;
    document.setDryMass("left", 30); document.setGradePercent("left", 4);
    auto partial = FlowsheetCalculationService::calculate(snapshot, document);
    if (partial.complete || !partial.values.contains("left") || partial.dryMassDegreesOfFreedom != 1) return 1;
    document.setDryMass("right", 70); document.setGradePercent("right", 1);
    auto complete = FlowsheetCalculationService::calculate(snapshot, document);
    if (!complete.complete || !complete.fullySolved
        || std::abs(complete.values.value("feed").dryMass - 100) > 1e-8
        || std::abs(complete.values.value("feed").gradePercent() - 1.9) > 1e-8) return 2;
    document.setComponents({{"component-1", "Cu"}, {"zn", "Zn"}});
    document.setGradePercent("left", "zn", 2); document.setGradePercent("right", "zn", 3);
    auto multi = FlowsheetCalculationService::calculate(snapshot, document);
    if (!multi.complete || std::abs(multi.components.value("zn").values.value("feed").gradePercent() - 2.7) > 1e-8) return 3;
    document.setDryMass("feed", 120); document.setGradePercent("feed", 1.9);
    document.setGradePercent("feed", "zn", 2.7);
    auto conflict = FlowsheetCalculationService::calculate(snapshot, document);
    bool inconsistent = false;
    for (const auto& issue : conflict.issues)
        inconsistent |= issue.code == topology::IssueCode::InconsistentBalance;
    if (conflict.complete || !inconsistent) return 4;
    document.setCalculationMode(CalculationMode::DataReconciliation);
    auto reconciled = FlowsheetCalculationService::calculate(snapshot, document);
    if (!reconciled.complete || !reconciled.reconciled || reconciled.residuals.isEmpty()) return 5;
    // Allocation input survives the split of display/navigation descriptors.
    snapshot.productStreams[0].mergeBranch = true;
    document.setDryMassSharePercent("left", 30);
    document.setComponentSharePercent("left", "component-1", 40);
    auto input = CalculationInputBuilder::build(snapshot, document, "component-1");
    if (!input.allocations.contains("left")) return 6;
    qInfo() << "Canvas-free calculation service checks passed";
    return 0;
}
