#pragma once

#include "services/FlowGroupSnapshot.h"
#include <functional>

namespace afs {
class FlowsheetDocument;
class FlowsheetScene;
}

namespace afs::flow_group {

CopyResult capture(const FlowsheetScene& scene, const FlowsheetDocument& document, Snapshot& snapshot);
PasteResult paste(const Snapshot& snapshot, const QPointF& offset,
                  FlowsheetScene& scene, FlowsheetDocument& document,
                  const std::function<QString()>& nextUnitId);

} // namespace afs::flow_group
