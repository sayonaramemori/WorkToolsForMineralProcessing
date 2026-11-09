#pragma once
#include "document/FlowsheetSnapshot.h"
#include "ui/CalculationPanelState.h"
#include <QObject>
#include <optional>

class QTimer;
namespace afs {
class FlowsheetDocument;

class CalculationController final : public QObject {
    Q_OBJECT
public:
    explicit CalculationController(FlowsheetDocument& document, QObject* parent = nullptr);
    void setSnapshot(FlowsheetSnapshot snapshot);
    // Preview is cached until calculation inputs change, and never saved.
    const topology::CalculationResult& displayResult();
    topology::CalculationResult calculate() const;

signals:
    void stateChanged(const afs::CalculationPanelState& state);

private:
    FlowsheetDocument& m_document;
    FlowsheetSnapshot m_snapshot;
    std::optional<topology::CalculationResult> m_preview;
    QTimer* m_refreshTimer;
    void invalidatePreview();
    void publishState();
};
} // namespace afs
