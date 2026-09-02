#include "topology/TopologyTypes.h"

#include <cmath>

namespace afs::topology {

std::optional<StreamValue> StreamValue::fromMassAndGrade(double dryMass, double gradePercent) {
    if (!std::isfinite(dryMass) || !std::isfinite(gradePercent)
        || dryMass < 0.0 || gradePercent < 0.0 || gradePercent > 100.0) return std::nullopt;
    return StreamValue{dryMass, dryMass * gradePercent / 100.0};
}

double StreamValue::gradePercent() const {
    return dryMass > 0.0 ? componentMass / dryMass * 100.0 : 0.0;
}

} // namespace afs::topology
