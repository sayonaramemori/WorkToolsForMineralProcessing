#pragma once
#include "document/FlowsheetSnapshot.h"
#include "adapters/CanvasItemIndex.h"

namespace afs {
class FlowsheetScene;
class CanvasTopologyBuilder final {
public:
    [[nodiscard]] static FlowsheetSnapshot build(const FlowsheetScene& scene);
    [[nodiscard]] static CanvasItemIndex buildItemIndex(const FlowsheetScene& scene);
};
} // namespace afs
