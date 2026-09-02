#pragma once

#include <QDialog>

namespace afs {

class FlowsheetDocument;
struct CanvasTopologySnapshot;

class ScenarioComparisonDialog final : public QDialog {
public:
    ScenarioComparisonDialog(const FlowsheetDocument& document,
                             const CanvasTopologySnapshot& snapshot,
                             QWidget* parent = nullptr);
};

} // namespace afs
