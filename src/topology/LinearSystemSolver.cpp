#include "topology/LinearSystemSolver.h"

#include <QSet>
#include <algorithm>
#include <cmath>

namespace afs::topology {

LinearSystemSolution LinearSystemSolver::solve(
    QVector<QVector<double>> matrix, int variableCount) {
    LinearSystemSolution result{QVector<std::optional<double>>(variableCount)};
    if (variableCount < 0) {
        result.inconsistent = true;
        return result;
    }

    for (auto& row : matrix) {
        if (row.size() != variableCount + 1) {
            result.inconsistent = true;
            return result;
        }
        double scale = 0.0;
        for (int column = 0; column < variableCount; ++column)
            scale = std::max(scale, std::abs(row[column]));
        if (scale <= 0.0) continue;
        for (double& value : row) value /= scale;
    }

    constexpr double coefficientTolerance = 1e-11;
    double rightHandSideScale = 1.0;
    for (const auto& row : matrix)
        rightHandSideScale = std::max(rightHandSideScale,
                                      std::abs(row[variableCount]));

    QVector<int> pivotColumns;
    int pivotRow = 0;
    for (int column = 0; column < variableCount && pivotRow < matrix.size(); ++column) {
        int best = pivotRow;
        for (int row = pivotRow + 1; row < matrix.size(); ++row)
            if (std::abs(matrix[row][column]) > std::abs(matrix[best][column])) best = row;
        if (std::abs(matrix[best][column]) <= coefficientTolerance) continue;
        if (best != pivotRow) matrix.swapItemsAt(best, pivotRow);
        const double pivot = matrix[pivotRow][column];
        for (int index = column; index <= variableCount; ++index)
            matrix[pivotRow][index] /= pivot;
        for (int row = 0; row < matrix.size(); ++row) {
            if (row == pivotRow
                || std::abs(matrix[row][column]) <= coefficientTolerance) continue;
            const double factor = matrix[row][column];
            for (int index = column; index <= variableCount; ++index)
                matrix[row][index] -= factor * matrix[pivotRow][index];
        }
        pivotColumns.append(column);
        ++pivotRow;
    }

    result.degreesOfFreedom = variableCount - pivotColumns.size();
    for (const auto& row : matrix) {
        bool zero = true;
        for (int column = 0; column < variableCount; ++column)
            zero = zero && std::abs(row[column]) <= coefficientTolerance;
        if (zero && std::abs(row[variableCount]) > 1e-9 * rightHandSideScale) {
            result.inconsistent = true;
            return result;
        }
    }

    const QSet<int> pivots(pivotColumns.cbegin(), pivotColumns.cend());
    for (int row = 0; row < pivotColumns.size(); ++row) {
        bool dependsOnFreeVariable = false;
        for (int column = 0; column < variableCount; ++column) {
            if (!pivots.contains(column)
                && std::abs(matrix[row][column]) > coefficientTolerance) {
                dependsOnFreeVariable = true;
                break;
            }
        }
        if (dependsOnFreeVariable) continue;
        double value = matrix[row][variableCount];
        if (std::abs(value) <= coefficientTolerance) value = 0.0;
        result.values[pivotColumns[row]] = value;
    }
    return result;
}

} // namespace afs::topology
