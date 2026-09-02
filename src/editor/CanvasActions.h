#pragma once

namespace afs {

class FlowsheetScene;

struct ResizeSelectionResult {
    int resizedUnits{0};
    int resizedConnections{0};
    double lastUnitWidth{0.0};
};

class CanvasActions final {
public:
    CanvasActions() = delete;
    static ResizeSelectionResult resizeSelection(FlowsheetScene& scene, double delta);
    static int disconnectSelection(FlowsheetScene& scene);
};

} // namespace afs
