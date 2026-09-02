#pragma once

#include "topology/TopologyTypes.h"

namespace afs {

class FlowsheetDocument;
class FlowsheetScene;

class FlowsheetCalculationService final {
public:
    static topology::CalculationResult calculate(
        const FlowsheetScene& scene, const FlowsheetDocument& document);
};

} // namespace afs
