#pragma once

#include <QDialog>
#include <QVector>

namespace afs {

class FlowsheetDocument;
struct StreamDescriptor;

class ScenarioComparisonDialog final : public QDialog {
public:
    ScenarioComparisonDialog(const FlowsheetDocument& document,
                             const QVector<StreamDescriptor>& streams,
                             QWidget* parent = nullptr);
};

} // namespace afs
