#include "services/FlowGroupClipboard.h"
#include "services/FlowGroupSnapshotOperations.h"

namespace afs {

FlowGroupClipboard::CopyResult FlowGroupClipboard::copy(
    const FlowsheetScene& scene, const FlowsheetDocument& document) {
    m_pasteCount = 0;
    return flow_group::capture(scene, document, m_snapshot);
}

FlowGroupClipboard::PasteResult FlowGroupClipboard::paste(
    FlowsheetScene& scene, FlowsheetDocument& document,
    const std::function<QString()>& nextUnitId) {
    if (!hasData()) return {};
    // Keep the increment separate from QPointF arguments: argument evaluation
    // order must not produce mismatched horizontal and vertical offsets.
    const int pasteIndex = ++m_pasteCount;
    const QPointF offset(40.0 * pasteIndex, 40.0 * pasteIndex);
    return flow_group::paste(m_snapshot, offset, scene, document, nextUnitId);
}

} // namespace afs
