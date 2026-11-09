#pragma once

#include "topology/TopologyTypes.h"

namespace afs {

class FlowsheetDocument;
struct FlowsheetSnapshot;

class FlowsheetCalculationService final {
public:
    static topology::CalculationResult calculate(
        const FlowsheetSnapshot& snapshot, const FlowsheetDocument& document);
};

} // namespace afs
