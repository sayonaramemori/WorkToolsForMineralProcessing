#include "topology/BalanceEquationSolver.h"
#include "topology/WeightedLeastSquaresSolver.h"
#include <utility>

namespace afs::topology::calculation_detail {

LinearSystemSolution solveScalarSystem(
    const TopologyGraph& graph,
    const QHash<StreamId, StreamValue>& knownValues,
    const QHash<StreamId, BranchAllocation>& allocations,
    const QVector<LinearBalanceConstraint>& constraints,
    bool componentMass,
    const QHash<StreamId, StreamUncertainty>& uncertainties) {
    const auto streamIds = graph.streamIds();
    const int variableCount = streamIds.size();
    QHash<StreamId, int> columns;
    for (int index = 0; index < variableCount; ++index) columns.insert(streamIds[index], index);
    QVector<QVector<double>> matrix;
    const auto equation = [variableCount] { return QVector<double>(variableCount + 1, 0.0); };

    for (const auto& nodeId : graph.nodeIds()) {
        auto row = equation();
        bool contributesBalance = true;
        if (graph.nodeKind(nodeId) == NodeKind::Flotation) {
            for (const auto& id : graph.streamsTo(nodeId, PortKind::Feed)) row[columns[id]] += 1.0;
            for (const auto& id : graph.flotationProductStreams(nodeId)) row[columns[id]] -= 1.0;
        } else if (graph.nodeKind(nodeId) == NodeKind::Merge) {
            for (const auto& id : graph.streamsTo(nodeId, PortKind::MergeInput)) row[columns[id]] += 1.0;
            for (const auto& id : graph.streamsFrom(nodeId, PortKind::MergeOutput)) row[columns[id]] -= 1.0;
        } else {
            const auto* pool = graph.storagePoolNode(nodeId);
            if (pool && pool->terminal) {
                // A terminal pond is a boundary/sink, not a zero-holdup
                // process constraint. Its incoming stream remains a terminal
                // product and must not be forced to zero.
                contributesBalance = false;
            } else {
                for (const auto& id : graph.streamsTo(nodeId, PortKind::Feed)) row[columns[id]] += 1.0;
                for (const auto& id : graph.streamsFrom(nodeId, PortKind::PoolOutput)) row[columns[id]] -= 1.0;
            }
        }
        if (contributesBalance) matrix.append(std::move(row));
        if (graph.nodeKind(nodeId) == NodeKind::Flotation) {
            const auto* node = graph.flotationNode(nodeId);
            if (node && node->leftSplitPercent) {
                const auto feedIds = graph.streamsTo(nodeId, PortKind::Feed);
                const auto leftIds = graph.streamsFrom(nodeId, PortKind::LeftProduct);
                const auto rightIds = graph.streamsFrom(nodeId, PortKind::RightProduct);
                if (!feedIds.isEmpty() && !leftIds.isEmpty() && !rightIds.isEmpty()) {
                    auto leftRow = equation();
                    leftRow[columns[leftIds.front()]] = 1.0;
                    leftRow[columns[feedIds.front()]] = -*node->leftSplitPercent / 100.0;
                    matrix.append(std::move(leftRow));
                    auto rightRow = equation();
                    rightRow[columns[rightIds.front()]] = 1.0;
                    rightRow[columns[feedIds.front()]] =
                        -(100.0 - *node->leftSplitPercent) / 100.0;
                    matrix.append(std::move(rightRow));
                }
            }
        }
    }
    for (const auto& constraint : constraints) {
        auto row = equation();
        for (auto it = constraint.coefficients.cbegin();
             it != constraint.coefficients.cend(); ++it) {
            const auto column = columns.constFind(it.key());
            if (column != columns.cend()) row[*column] += it.value();
        }
        row[variableCount] = componentMass
            ? constraint.rightHandSide.componentMass
            : constraint.rightHandSide.dryMass;
        matrix.append(std::move(row));
    }
    for (const auto& nodeId : graph.nodeIds()) {
        if (graph.nodeKind(nodeId) != NodeKind::Merge) continue;
        const auto outputIds = graph.streamsFrom(nodeId, PortKind::MergeOutput);
        if (outputIds.isEmpty()) continue;
        const int outputColumn = columns[outputIds.front()];
        for (const auto& inputId : graph.streamsTo(nodeId, PortKind::MergeInput)) {
            const auto allocation = allocations.constFind(inputId);
            if (allocation == allocations.cend()) continue;
            auto row = equation();
            row[columns[inputId]] = 1.0;
            const double percent = componentMass ? allocation->componentSharePercent
                                                 : allocation->dryMassSharePercent;
            row[outputColumn] = -percent / 100.0;
            matrix.append(std::move(row));
        }
    }

    if (uncertainties.isEmpty()) {
        for (auto it = knownValues.cbegin(); it != knownValues.cend(); ++it) {
            auto row = equation();
            row[columns[it.key()]] = 1.0;
            row[variableCount] = componentMass ? it->componentMass : it->dryMass;
            matrix.append(std::move(row));
        }
        return LinearSystemSolver::solve(std::move(matrix), variableCount);
    }

    QVector<std::optional<double>> observations(variableCount);
    QVector<std::optional<double>> standardDeviations(variableCount);
    for (int column = 0; column < variableCount; ++column) {
        const auto known = knownValues.constFind(streamIds[column]);
        const auto uncertainty = uncertainties.constFind(streamIds[column]);
        if (known != knownValues.cend() && uncertainty != uncertainties.cend()) {
            observations[column] = componentMass ? known->componentMass : known->dryMass;
            standardDeviations[column] = componentMass
                ? uncertainty->componentMassStdDev : uncertainty->dryMassStdDev;
        }
    }
    return WeightedLeastSquaresSolver::solve(matrix, observations, standardDeviations);
}

} // namespace afs::topology::calculation_detail
