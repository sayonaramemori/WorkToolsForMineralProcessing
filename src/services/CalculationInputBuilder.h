#pragma once

#include "topology/OpenCircuitCalculator.h"

namespace afs {
class FlowsheetDocument;
struct FlowsheetSnapshot;

struct ComponentCalculationInput {
    QHash<topology::StreamId, topology::StreamValue> knownValues;
    QHash<topology::StreamId, topology::BranchAllocation> allocations;
    QVector<topology::LinearBalanceConstraint> constraints;
    QHash<topology::StreamId, topology::StreamUncertainty> uncertainties;
};

class CalculationInputBuilder final {
public:
    CalculationInputBuilder() = delete;
    [[nodiscard]] static ComponentCalculationInput build(
        const FlowsheetSnapshot& snapshot,
        const FlowsheetDocument& document,
        const QString& componentId);
};
} // namespace afs
