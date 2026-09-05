#include "services/FlowsheetCalculationService.h"
#include "adapters/CanvasTopologyBuilder.h"
#include "document/FlowsheetDocument.h"
#include "editor/FlowsheetScene.h"
#include "topology/OpenCircuitCalculator.h"

#include <algorithm>

namespace afs {
namespace {

CanvasTopologySnapshot scopedSnapshot(const CanvasTopologySnapshot& source,
                                      const QSet<QString>& objectScope) {
    if (objectScope.isEmpty()) return source;
    CanvasTopologySnapshot result;
    result.interestObjects = source.interestObjects;
    for (const auto& nodeId : source.graph.nodeIds()) {
        if (!objectScope.contains(nodeId)) continue;
        if (source.graph.nodeKind(nodeId) == topology::NodeKind::Flotation)
            result.graph.addFlotationNode(*source.graph.flotationNode(nodeId));
        else
            result.graph.addMergeNode(*source.graph.mergeNode(nodeId));
    }
    QSet<QString> retainedStreams;
    for (const auto& streamId : source.graph.streamIds()) {
        const auto* stream = source.graph.stream(streamId);
        const bool sourceSelected = stream->source
            && objectScope.contains(stream->source->nodeId);
        const bool targetSelected = stream->target
            && objectScope.contains(stream->target->nodeId);
        if (!sourceSelected && !targetSelected) continue;
        result.graph.addStream({streamId,
            sourceSelected ? stream->source : std::nullopt,
            targetSelected ? stream->target : std::nullopt});
        retainedStreams.insert(streamId);
    }
    const auto appendRetained = [&retainedStreams](const auto& input, auto& output) {
        for (const auto& descriptor : input)
            if (retainedStreams.contains(descriptor.streamId)) output.append(descriptor);
    };
    appendRetained(source.reportStreams, result.reportStreams);
    appendRetained(source.productStreams, result.productStreams);
    appendRetained(source.terminalProducts, result.terminalProducts);
    appendRetained(source.requiredMeasurements, result.requiredMeasurements);
    return result;
}

} // namespace

topology::CalculationResult FlowsheetCalculationService::calculate(
    const FlowsheetScene& scene, const FlowsheetDocument& document) {
    const auto snapshot = CanvasTopologyBuilder::build(scene);
    return calculate(snapshot, document);
}

topology::CalculationResult FlowsheetCalculationService::calculate(
    const CanvasTopologySnapshot& snapshot, const FlowsheetDocument& document) {
    return calculate(snapshot, document, {});
}

topology::CalculationResult FlowsheetCalculationService::calculate(
    const FlowsheetScene& scene, const FlowsheetDocument& document,
    const QSet<QString>& objectScope) {
    return calculate(CanvasTopologyBuilder::build(scene), document, objectScope);
}

topology::CalculationResult FlowsheetCalculationService::calculate(
    const CanvasTopologySnapshot& completeSnapshot, const FlowsheetDocument& document,
    const QSet<QString>& objectScope) {
    const auto snapshot = scopedSnapshot(completeSnapshot, objectScope);
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
        for (const auto& stream : snapshot.reportStreams) {
            const auto measurement = document.measurement(stream.streamId);
            const auto grade = measurement.grade(component.id);
            if (!measurement.dryMass || !grade) continue;
            if (auto value = topology::StreamValue::fromMassAndGrade(*measurement.dryMass, *grade))
                knownValues.insert(stream.streamId, *value);
        }
        auto result = topology::OpenCircuitCalculator::calculate(
            snapshot.graph, knownValues, allocations, !objectScope.isEmpty());
        if (first) { combined = result; first = false; }
        combined.components.insert(component.id, {result.values, result.flotationPerformance,
            result.relativeToExternalFeed, result.complete, result.fullySolved});
        if (!result.complete) combined.complete = false;
        if (!result.fullySolved) combined.fullySolved = false;
        combined.dryMassDegreesOfFreedom = std::max(
            combined.dryMassDegreesOfFreedom, result.dryMassDegreesOfFreedom);
        combined.componentMassDegreesOfFreedom = std::max(
            combined.componentMassDegreesOfFreedom, result.componentMassDegreesOfFreedom);
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
