#pragma once

#include "topology/LinearSystemSolver.h"

#include <QVector>
#include <optional>

namespace afs::topology {

// Solves min sum((x_i-y_i)/sigma_i)^2 subject to A*x=b.
// A balance row contains variableCount coefficients followed by b.
class WeightedLeastSquaresSolver final {
public:
    WeightedLeastSquaresSolver() = delete;
    static LinearSystemSolution solve(
        const QVector<QVector<double>>& balanceMatrix,
        const QVector<std::optional<double>>& observations,
        const QVector<std::optional<double>>& standardDeviations);
};

} // namespace afs::topology
