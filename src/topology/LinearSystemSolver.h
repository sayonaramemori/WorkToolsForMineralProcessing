#pragma once

#include <QVector>
#include <optional>

namespace afs::topology {

struct LinearSystemSolution {
    QVector<std::optional<double>> values;
    bool inconsistent{false};
    int degreesOfFreedom{0};
};

// Solves an augmented dense matrix using normalized Gauss-Jordan elimination.
// It also reports variables that are unique in an otherwise underdetermined
// system. Equation assembly remains the caller's responsibility.
class LinearSystemSolver final {
public:
    LinearSystemSolver() = delete;
    static LinearSystemSolution solve(QVector<QVector<double>> matrix,
                                      int variableCount);
};

} // namespace afs::topology
