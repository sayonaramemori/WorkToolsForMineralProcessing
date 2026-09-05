#pragma once

#include "topology/TopologyTypes.h"

#include <QSet>

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
    static topology::CalculationResult calculate(
        const FlowsheetScene& scene, const FlowsheetDocument& document,
        const QSet<QString>& objectScope);
    static topology::CalculationResult calculate(
        const CanvasTopologySnapshot& snapshot, const FlowsheetDocument& document,
        const QSet<QString>& objectScope);
};

} // namespace afs
