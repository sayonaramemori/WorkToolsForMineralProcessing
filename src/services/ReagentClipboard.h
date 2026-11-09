#pragma once
#include "annotations/AnnotationTypes.h"
#include <QHash>
#include <optional>

namespace afs {
// A value snapshot separate from flow-group storage and the system clipboard.
class ReagentClipboard final {
public:
    bool copy(const AnnotationRecord& record);
    std::optional<AnnotationRecord> makePaste(const QString& targetStream,
        const QHash<QString, AnnotationRecord>& existing) const;
private:
    std::optional<AnnotationRecord> m_record;
};
}
