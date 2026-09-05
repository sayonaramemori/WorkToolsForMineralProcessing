#pragma once

#include "topology/TopologyGraph.h"

namespace afs::topology {

struct BranchAllocation {
    double dryMassSharePercent{0.0};
    double componentSharePercent{0.0};
};

class OpenCircuitCalculator final {
public:
    OpenCircuitCalculator() = delete;
    static CalculationResult calculate(
        const TopologyGraph& graph,
        const QHash<StreamId, StreamValue>& knownValues,
        const QHash<StreamId, BranchAllocation>& allocations = {},
        bool scopedCalculation = false);
};

} // namespace afs::topology
