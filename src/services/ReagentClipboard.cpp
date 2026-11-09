#include "services/ReagentClipboard.h"
#include "annotations/ReagentOrder.h"
#include <algorithm>
#include <limits>

namespace afs {
bool ReagentClipboard::copy(const AnnotationRecord& record) {
    if (record.id.isEmpty() || record.kind != AnnotationKind::Reagent
        || (record.text.isEmpty() && record.dosage.isEmpty() && record.note.isEmpty())) return false;
    m_record = record;
    return true;
}

std::optional<AnnotationRecord> ReagentClipboard::makePaste(const QString& targetStream,
    const QHash<QString, AnnotationRecord>& existing) const {
    if (!m_record || targetStream.isEmpty()) return std::nullopt;
    quint64 maximum = 0;
    for (auto it = existing.cbegin(); it != existing.cend(); ++it) {
        if (!isPastedReagentId(it.key())) continue;
        bool valid = false;
        const auto number = it.key().mid(ReagentPastePrefix.size()).toULongLong(&valid);
        if (valid) maximum = std::max(maximum, number);
    }
    if (maximum == std::numeric_limits<quint64>::max()) return std::nullopt;
    auto pasted = *m_record;
    pasted.id = ReagentPastePrefix + QString("%1").arg(maximum + 1, 20, 10, QLatin1Char('0'));
    pasted.ownerKind = AnnotationOwnerKind::Stream; pasted.ownerId = targetStream;
    pasted.manualOffset = {}; pasted.manuallyPlaced = false; pasted.visible = true;
    return pasted;
}
}
