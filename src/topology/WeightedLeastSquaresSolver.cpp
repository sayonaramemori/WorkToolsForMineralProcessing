#include "topology/WeightedLeastSquaresSolver.h"

namespace afs::topology {

LinearSystemSolution WeightedLeastSquaresSolver::solve(
    const QVector<QVector<double>>& balanceMatrix,
    const QVector<std::optional<double>>& observations,
    const QVector<std::optional<double>>& standardDeviations) {
    const int variableCount = observations.size();
    if (standardDeviations.size() != variableCount) {
        LinearSystemSolution invalid;
        invalid.inconsistent = true;
        return invalid;
    }
    const int constraintCount = balanceMatrix.size();
    const int kktVariables = variableCount + constraintCount;
    QVector<QVector<double>> matrix;
    matrix.reserve(kktVariables);

    for (int column = 0; column < variableCount; ++column) {
        QVector<double> row(kktVariables + 1, 0.0);
        if (observations[column] && standardDeviations[column]
            && *standardDeviations[column] > 0.0) {
            const double weight = 1.0
                / (*standardDeviations[column] * *standardDeviations[column]);
            row[column] = weight;
            row[kktVariables] = weight * *observations[column];
        }
        for (int constraint = 0; constraint < constraintCount; ++constraint) {
            if (balanceMatrix[constraint].size() != variableCount + 1) {
                LinearSystemSolution invalid;
                invalid.inconsistent = true;
                return invalid;
            }
            row[variableCount + constraint] = balanceMatrix[constraint][column];
        }
        matrix.append(std::move(row));
    }
    for (const auto& balance : balanceMatrix) {
        QVector<double> row(kktVariables + 1, 0.0);
        for (int column = 0; column < variableCount; ++column)
            row[column] = balance[column];
        row[kktVariables] = balance[variableCount];
        matrix.append(std::move(row));
    }

    auto result = LinearSystemSolver::solve(std::move(matrix), kktVariables);
    result.values.resize(variableCount);
    result.degreesOfFreedom = 0;
    for (const auto& value : result.values)
        if (!value) ++result.degreesOfFreedom;
    return result;
}

} // namespace afs::topology
