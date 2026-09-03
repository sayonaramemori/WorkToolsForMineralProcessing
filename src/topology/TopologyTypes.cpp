#include "topology/TopologyTypes.h"

#include <cmath>

namespace afs::topology {

QVector<PortKind> flotationProductPorts(const FlotationNode& node) {
    QVector<PortKind> ports{PortKind::LeftProduct};
    if (node.hasMiddleProduct) ports.append(PortKind::MiddleProduct);
    ports.append(PortKind::RightProduct);
    return ports;
}

ProductMetrics FlotationPerformance::forPort(PortKind port) const {
    switch (port) {
    case PortKind::LeftProduct: return left;
    case PortKind::MiddleProduct: return middle;
    case PortKind::RightProduct: return right;
    default: return {};
    }
}

void FlotationPerformance::setForPort(PortKind port, ProductMetrics value) {
    switch (port) {
    case PortKind::LeftProduct: left = value; break;
    case PortKind::MiddleProduct: middle = value; break;
    case PortKind::RightProduct: right = value; break;
    default: break;
    }
}

std::optional<StreamValue> StreamValue::fromMassAndGrade(double dryMass, double gradePercent) {
    if (!std::isfinite(dryMass) || !std::isfinite(gradePercent)
        || dryMass < 0.0 || gradePercent < 0.0 || gradePercent > 100.0) return std::nullopt;
    return StreamValue{dryMass, dryMass * gradePercent / 100.0};
}

double StreamValue::gradePercent() const {
    return dryMass > 0.0 ? componentMass / dryMass * 100.0 : 0.0;
}

} // namespace afs::topology
