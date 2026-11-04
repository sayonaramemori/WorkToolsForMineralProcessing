#pragma once

#include "core/FlotationUnit.h"

namespace afs::project_serialization {

// Stable current-format type keys. Display names and device drawing do not
// belong to the project format and remain in their presentation modules.
QString unitKindKey(UnitKind kind);
std::optional<UnitKind> unitKindFromKey(const QString& key);

} // namespace afs::project_serialization
