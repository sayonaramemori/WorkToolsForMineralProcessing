#pragma once

#include "topology/TopologyTypes.h"

namespace afs {

class FlowsheetDocument;
class FlowsheetScene;
struct CanvasTopologySnapshot;

class FlowsheetCalculationService final {
public:
    static topology::CalculationResult calculate(
        const FlowsheetScene& scene, const FlowsheetDocument& document);
    static topology::CalculationResult calculate(
        const CanvasTopologySnapshot& snapshot, const FlowsheetDocument& document);
};

} // namespace afs
