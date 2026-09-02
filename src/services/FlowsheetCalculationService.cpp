#include "services/FlowsheetCalculationService.h"
#include "adapters/CanvasTopologyBuilder.h"
#include "document/FlowsheetDocument.h"
#include "editor/FlowsheetScene.h"
#include "topology/OpenCircuitCalculator.h"

namespace afs {

topology::CalculationResult FlowsheetCalculationService::calculate(
    const FlowsheetScene& scene, const FlowsheetDocument& document) {
    const auto snapshot = CanvasTopologyBuilder::build(scene);
    topology::CalculationResult combined;
    bool first = true;
    for (const auto& component : document.components()) {
        QHash<topology::StreamId, topology::StreamValue> knownValues;
        QHash<topology::StreamId, topology::BranchAllocation> allocations;
        for (const auto& stream : snapshot.productStreams) {
            if (!stream.mergeBranch) continue;
            const auto measurement = document.measurement(stream.streamId);
            const auto componentShare = measurement.componentShare(component.id);
            if (measurement.dryMassSharePercent && componentShare)
                allocations.insert(stream.streamId, {*measurement.dryMassSharePercent,
                                                      *componentShare});
            else if (measurement.dryMassSharePercent || componentShare)
                allocations.insert(stream.streamId, {-1.0, -1.0});
        }
        for (const auto& stream : snapshot.requiredMeasurements) {
            const auto measurement = document.measurement(stream.streamId);
            const auto grade = measurement.grade(component.id);
            if (!measurement.dryMass || !grade) continue;
            if (auto value = topology::StreamValue::fromMassAndGrade(*measurement.dryMass, *grade))
                knownValues.insert(stream.streamId, *value);
        }
        auto result = topology::OpenCircuitCalculator::calculate(
            snapshot.graph, knownValues, allocations);
        if (first) { combined = result; first = false; }
        combined.components.insert(component.id, {result.values, result.flotationPerformance,
            result.relativeToExternalFeed, result.complete, result.fullySolved});
        if (!result.complete) combined.complete = false;
        if (!result.fullySolved) combined.fullySolved = false;
        if (!first && component.id != document.components().front().id) {
            for (auto issue : result.issues) {
                issue.message = QStringLiteral("%1：%2").arg(component.name, issue.message);
                combined.issues.append(std::move(issue));
            }
        }
    }
    return combined;
}

} // namespace afs
