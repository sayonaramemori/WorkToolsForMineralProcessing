#pragma once
#include "topology/TopologyTypes.h"
namespace afs {
class FlowsheetDocument;
struct CanvasTopologySnapshot;
struct CalculationDiagnostic {
    QString objectId, streamId, componentId, title, message;
    bool error{false}, gradeField{false};
};
class CalculationDiagnostics final {
public:
    static QVector<CalculationDiagnostic> build(const CanvasTopologySnapshot& snapshot,
                                               const FlowsheetDocument& document,
                                               const topology::CalculationResult& result);
};
}
