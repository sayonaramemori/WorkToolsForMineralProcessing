#pragma once
#include "annotations/AnnotationTypes.h"

namespace afs {
inline const QString ReagentPastePrefix = QStringLiteral("reagent-paste-");

inline bool isPastedReagentId(const QString& id) {
    if (!id.startsWith(ReagentPastePrefix)) return false;
    const auto suffix = id.mid(ReagentPastePrefix.size());
    if (suffix.size() != 20) return false;
    for (const auto character : suffix)
        if (character < QLatin1Char('0') || character > QLatin1Char('9')) return false;
    return true;
}

// Keep existing reagent order, then append single-reagent copies in paste order.
// The same ordering is used when capturing a reusable group/template.
inline bool reagentDisplayLess(const AnnotationRecord& a, const AnnotationRecord& b) {
    if (a.ownerId != b.ownerId) return a.ownerId < b.ownerId;
    const bool pastedA = isPastedReagentId(a.id), pastedB = isPastedReagentId(b.id);
    if (pastedA != pastedB) return !pastedA;
    return a.id < b.id;
}
}
