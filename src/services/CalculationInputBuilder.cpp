#include "services/CalculationInputBuilder.h"
#include "adapters/CanvasTopologyBuilder.h"
#include "document/FlowsheetDocument.h"

#include <algorithm>
#include <cmath>

namespace afs {
namespace {
std::optional<topology::LinearBalanceConstraint> globalBoundaryConstraint(
    const CanvasTopologySnapshot& snapshot) {
    const auto feedIds = snapshot.graph.externalFeedStreams();
    if (feedIds.isEmpty()) return std::nullopt;
    topology::LinearBalanceConstraint constraint;
    constraint.id = QStringLiteral("global-boundary");
    for (const auto& streamId : feedIds)
        constraint.coefficients.insert(streamId, 1.0);
    const auto terminalIds = snapshot.graph.terminalProductStreams();
    if (terminalIds.isEmpty()) return std::nullopt;
    for (const auto& streamId : terminalIds)
        constraint.coefficients.insert(streamId,
            constraint.coefficients.value(streamId) - 1.0);
    return constraint;
}
}

ComponentCalculationInput CalculationInputBuilder::build(
    const CanvasTopologySnapshot& snapshot, const FlowsheetDocument& document,
    const QString& componentId) {
    ComponentCalculationInput input;
    for (const auto& stream : snapshot.productStreams) {
        if (!stream.mergeBranch) continue;
        const auto measurement = document.measurement(stream.streamId);
        const auto componentShare = measurement.componentShare(componentId);
        if (measurement.dryMassSharePercent && componentShare) {
            input.allocations.insert(stream.streamId,
                {*measurement.dryMassSharePercent, *componentShare});
        } else if (measurement.dryMassSharePercent || componentShare) {
            input.allocations.insert(stream.streamId, {-1.0, -1.0});
        }
    }
    for (const auto& stream : snapshot.reportStreams) {
        const auto measurement = document.measurement(stream.streamId);
        const auto grade = measurement.grade(componentId);
        if (!measurement.dryMass || !grade) continue;
        if (const auto value = topology::StreamValue::fromMassAndGrade(
                *measurement.dryMass, *grade)) {
            input.knownValues.insert(stream.streamId, *value);
            if (document.calculationMode() == CalculationMode::DataReconciliation) {
                const double massSigma = measurement.dryMassStdDev.value_or(
                    std::max(1e-6, std::abs(*measurement.dryMass) * 0.01));
                const double gradeSigma = measurement.gradeStdDev(componentId).value_or(0.1);
                const double fraction = *grade / 100.0;
                const double componentSigma = std::max(1e-9, std::hypot(
                    fraction * massSigma, *measurement.dryMass * gradeSigma / 100.0));
                input.uncertainties.insert(stream.streamId, {massSigma, componentSigma});
            }
        }
    }
    if (const auto boundary = globalBoundaryConstraint(snapshot))
        input.constraints.append(*boundary);
    return input;
}

} // namespace afs
