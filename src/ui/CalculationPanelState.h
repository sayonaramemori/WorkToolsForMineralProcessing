#pragma once
#include <QString>

namespace afs {
struct CalculationPanelState {
    QString message{QStringLiteral("请先在画布中建立流程")};
    bool canCalculate{false};
    bool calculated{false};
};
} // namespace afs
