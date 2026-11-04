#pragma once

#include "services/FlowGroupSnapshot.h"
#include <functional>

namespace afs {
class FlowsheetDocument;
class FlowsheetScene;

// An in-process flowsheet fragment.  It deliberately stores only topology
// wholly owned by the copied units, so paste can never reconnect a clone to
// an unrelated part of the source diagram. Selected free-text annotations are
// copied as independent document records and use the same translation as units.
// Reagents on copied streams are snapshots of the current scenario and retain
// their relative offsets when their owners are remapped into the pasted group.
class FlowGroupClipboard final {
public:
    using CopyResult = flow_group::CopyResult;
    using PasteResult = flow_group::PasteResult;

    [[nodiscard]] bool hasData() const { return m_snapshot.hasData(); }
    [[nodiscard]] CopyResult copy(const FlowsheetScene& scene, const FlowsheetDocument& document);
    [[nodiscard]] PasteResult paste(FlowsheetScene& scene, FlowsheetDocument& document,
                                    const std::function<QString()>& nextUnitId);

private:
    flow_group::Snapshot m_snapshot;
    int m_pasteCount{0};
};
} // namespace afs
