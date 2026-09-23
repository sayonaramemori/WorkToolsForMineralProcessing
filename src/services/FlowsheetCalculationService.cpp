#include "services/FlowsheetCalculationService.h"
#include "adapters/CanvasTopologyBuilder.h"
#include "document/FlowsheetDocument.h"
#include "editor/FlowsheetScene.h"
#include "topology/OpenCircuitCalculator.h"
#include "services/CalculationInputBuilder.h"

#include <algorithm>

namespace afs {
namespace {

void appendComponentResult(topology::CalculationResult& combined,
                           const ComponentDefinition& component,
                           topology::CalculationResult result,
                           bool primaryComponent) {
    if (primaryComponent) combined = result;
    combined.components.insert(component.id, {
        result.values, result.flotationPerformance, result.relativeToExternalFeed,
        result.complete, result.fullySolved, result.residuals,
        result.maximumAbsoluteStandardizedResidual});
    combined.complete = combined.complete && result.complete;
    combined.fullySolved = combined.fullySolved && result.fullySolved;
    combined.dryMassDegreesOfFreedom = std::max(
        combined.dryMassDegreesOfFreedom, result.dryMassDegreesOfFreedom);
    combined.componentMassDegreesOfFreedom = std::max(
        combined.componentMassDegreesOfFreedom, result.componentMassDegreesOfFreedom);
    if (!primaryComponent) {
        for (auto issue : result.issues) {
            issue.message = QStringLiteral("%1：%2").arg(component.name, issue.message);
            combined.issues.append(std::move(issue));
        }
    }
}

} // namespace

topology::CalculationResult FlowsheetCalculationService::calculate(
    const FlowsheetScene& scene, const FlowsheetDocument& document) {
    const auto snapshot = CanvasTopologyBuilder::build(scene);
    return calculate(snapshot, document);
}

topology::CalculationResult FlowsheetCalculationService::calculate(
    const CanvasTopologySnapshot& snapshot, const FlowsheetDocument& document) {
    topology::CalculationResult combined;
    for (int index = 0; index < document.components().size(); ++index) {
        const auto& component = document.components()[index];
        const auto input = CalculationInputBuilder::build(snapshot, document, component.id);
        auto result = topology::OpenCircuitCalculator::calculate(
            snapshot.graph, input.knownValues, input.allocations,
            false, input.constraints, input.uncertainties);
        appendComponentResult(combined, component, std::move(result), index == 0);
    }
    return combined;
}

} // namespace afs
