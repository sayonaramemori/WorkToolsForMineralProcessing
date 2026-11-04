#pragma once

#include "annotations/AnnotationTypes.h"
#include <optional>

class QWidget;

namespace afs {

// Edits a value only; the caller decides whether to commit it to a document.
std::optional<AnnotationTextSettings> editAnnotationTextStyleDialog(
    QWidget* parent, AnnotationTextSettings current);

} // namespace afs
