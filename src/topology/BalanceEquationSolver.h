#pragma once

#include "topology/OpenCircuitCalculator.h"
#include "topology/LinearSystemSolver.h"

namespace afs::topology::calculation_detail {

LinearSystemSolution solveScalarSystem(
    const TopologyGraph& graph,
    const QHash<StreamId, StreamValue>& knownValues,
    const QHash<StreamId, BranchAllocation>& allocations,
    const QVector<LinearBalanceConstraint>& constraints,
    bool componentMass,
    const QHash<StreamId, StreamUncertainty>& uncertainties);

} // namespace afs::topology::calculation_detail
