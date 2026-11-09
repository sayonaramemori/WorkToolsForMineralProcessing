#pragma once

class QWidget;
namespace afs {
enum class CalculationMode;
namespace topology { struct CalculationResult; }

// Read-only value presentation; does not calculate or modify a document.
void showCalculationExplanationDialog(QWidget* parent,
    const topology::CalculationResult& result, CalculationMode mode);

} // namespace afs
