#include "app/TerminalTableController.h"
#include "document/FlowsheetDocument.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace afs {
TerminalTableController::TerminalTableController(FlowsheetDocument& document, QObject* parent)
    : TerminalTableSession(parent), m_document(document) {
    connect(&document, &FlowsheetDocument::measurementChanged, this, [this](const QString& id) { refresh(id, TableChange::Measurement); });
    connect(&document, &FlowsheetDocument::productNameChanged, this, [this](const QString& id) { refresh(id, TableChange::ProductName); });
    connect(&document, &FlowsheetDocument::calculationChanged, this, [this] { refresh({}, TableChange::Result); });
    connect(&document, &FlowsheetDocument::componentsChanged, this, [this] { refresh(); });
    connect(&document, &FlowsheetDocument::calculationModeChanged, this, [this] { refresh(); });
    refresh();
}

void TerminalTableController::refresh(const QString& streamId, TableChange change) {
    m_state.measurements = m_document.measurements();
    m_state.productNames = m_document.productNames();
    m_state.components = m_document.components();
    m_state.mode = m_document.calculationMode();
    if (const auto* result = m_document.calculationResult()) m_state.calculation = *result;
    else m_state.calculation.reset();
    emit stateChanged(m_state, streamId, change);
}

void TerminalTableController::setStreams(QVector<StreamDescriptor> streams,
                                        std::optional<QSet<QString>> editableStreamIds) {
    m_state.streams = std::move(streams);
    m_state.editableStreamIds.clear();
    for (const auto& stream : m_state.streams)
        if (!editableStreamIds || editableStreamIds->contains(stream.streamId))
            m_state.editableStreamIds.insert(stream.streamId);
    refresh();
}

bool TerminalTableController::hasStream(const QString& id) const {
    return std::any_of(m_state.streams.cbegin(), m_state.streams.cend(),
        [&](const auto& stream) { return stream.streamId == id; });
}

bool TerminalTableController::submit(const TableCommand& command) {
    return std::visit([this](const auto& edit) { return apply(edit); }, command);
}

bool TerminalTableController::apply(const EditTableMeasurement& edit) {
    if (!hasStream(edit.streamId) || !m_state.editableStreamIds.contains(edit.streamId)) return false;
    const bool grade = edit.field == MeasurementField::Grade || edit.field == MeasurementField::GradeStdDev;
    const bool uncertainty = edit.field == MeasurementField::MassStdDev || edit.field == MeasurementField::GradeStdDev;
    if (grade && !m_document.componentIds().contains(edit.componentId)) return false;
    if (uncertainty && m_document.calculationMode() != CalculationMode::DataReconciliation) return false;
    std::optional<double> number;
    if (!edit.text.trimmed().isEmpty()) {
        bool valid = false;
        const double value = edit.text.trimmed().toDouble(&valid);
        if (!valid || !std::isfinite(value) || value < 0 || (uncertainty && value <= 0)
            || (edit.field == MeasurementField::Grade && value > 100)) return false;
        number = value;
    }
    switch (edit.field) {
    case MeasurementField::DryMass: m_document.setDryMass(edit.streamId, number); break;
    case MeasurementField::Grade: m_document.setGradePercent(edit.streamId, edit.componentId, number); break;
    case MeasurementField::MassStdDev: m_document.setDryMassStdDev(edit.streamId, number); break;
    case MeasurementField::GradeStdDev: m_document.setGradeStdDev(edit.streamId, edit.componentId, number); break;
    default: return false;
    }
    return true;
}

bool TerminalTableController::apply(const EditTableProductName& edit) {
    if (!hasStream(edit.streamId)) return false;
    m_document.setProductName(edit.streamId, edit.name);
    return true;
}

bool TerminalTableController::apply(const EditTableComponents& edit) {
    QStringList names;
    for (const auto& name : edit.names) {
        const QString normalized = name.simplified().left(40);
        if (!normalized.isEmpty() && !names.contains(normalized)) names.append(normalized);
    }
    if (names.isEmpty()) return false;
    QVector<ComponentDefinition> components;
    const auto previous = m_document.components();
    for (int index = 0; index < names.size(); ++index)
        components.append({index < previous.size() ? previous[index].id : QString("component-%1").arg(index + 1), names[index]});
    m_document.setComponents(std::move(components));
    return true;
}

bool TerminalTableController::apply(const EditTableMode& edit) {
    if (edit.mode != CalculationMode::Strict && edit.mode != CalculationMode::DataReconciliation) return false;
    m_document.setCalculationMode(edit.mode);
    return true;
}
} // namespace afs
