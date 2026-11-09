#include "ui/TerminalProductTableModel.h"
#include <utility>


namespace afs {

TerminalProductTableModel::TerminalProductTableModel(TerminalTableSession& session, QObject* parent)
    : QAbstractTableModel(parent), m_session(session), m_state(session.snapshot()) {
    connect(&session, &TerminalTableSession::stateChanged, this, &TerminalProductTableModel::applySnapshot);
}

void TerminalProductTableModel::applySnapshot(const TerminalTableState& state, const QString& streamId, TableChange change) {
    const bool schema = m_state.mode != state.mode || m_state.components != state.components;
    if (schema || m_state.streams != state.streams || m_state.editableStreamIds != state.editableStreamIds) {
        beginResetModel(); m_state = state; endResetModel();
        if (schema) emit schemaChanged();
        return;
    }
    m_state = state;
    const int first = change == TableChange::All ? 0
        : change == TableChange::ProductName ? ProductNameColumn : MassColumn;
    const int last = change == TableChange::ProductName ? ProductNameColumn : statusColumn();
    for (int row = 0; row < m_state.streams.size(); ++row)
        if (streamId.isEmpty() || m_state.streams[row].streamId == streamId)
            emit dataChanged(index(row, first), index(row, last));
}

int TerminalProductTableModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_state.streams.size();
}

int TerminalProductTableModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return m_state.mode == CalculationMode::DataReconciliation
        ? 5 + 2 * m_state.components.size()
        : 4 + m_state.components.size();
}

int TerminalProductTableModel::gradeColumn(const QString& componentId) const {
    for (int i = 0; i < m_state.components.size(); ++i)
        if (m_state.components[i].id == componentId)
            return (m_state.mode == CalculationMode::DataReconciliation ? 4 : 3)
                + (m_state.mode == CalculationMode::DataReconciliation ? 2 * i : i);
    return -1;
}
int TerminalProductTableModel::gradeStdDevColumn(const QString& componentId) const {
    const int grade = gradeColumn(componentId);
    return m_state.mode == CalculationMode::DataReconciliation ? grade + 1 : -1;
}
int TerminalProductTableModel::statusColumn() const {
    return columnCount() - 1;
}
bool TerminalProductTableModel::isGradeColumn(int column) const {
    for (const auto& component : m_state.components)
        if (gradeColumn(component.id) == column) return true;
    return false;
}
bool TerminalProductTableModel::isUncertaintyColumn(int column) const {
    if (m_state.mode != CalculationMode::DataReconciliation) return false;
    if (column == MassStdDevColumn) return true;
    for (const auto& component : m_state.components)
        if (gradeStdDevColumn(component.id) == column) return true;
    return false;
}
QString TerminalProductTableModel::componentIdForColumn(int column) const {
    for (const auto& component : m_state.components)
        if (gradeColumn(component.id) == column || gradeStdDevColumn(component.id) == column)
            return component.id;
    return {};
}

Qt::ItemFlags TerminalProductTableModel::flags(const QModelIndex& index) const {
    auto result = QAbstractTableModel::flags(index);
    if (!index.isValid() || index.row() >= m_state.streams.size()) return result;
    if (index.column() == ProductNameColumn
        || (m_state.editableStreamIds.contains(m_state.streams[index.row()].streamId)
            && (index.column() == MassColumn || isGradeColumn(index.column())
                || isUncertaintyColumn(index.column()))))
        result |= Qt::ItemIsEditable;
    return result;
}

bool TerminalProductTableModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (role != Qt::EditRole || !index.isValid() || index.row() >= m_state.streams.size()) return false;
    const QString id = m_state.streams[index.row()].streamId;
    if (index.column() == ProductNameColumn)
        return m_session.submit(EditTableProductName{id, value.toString()});
    MeasurementField field;
    if (index.column() == MassColumn) field = MeasurementField::DryMass;
    else if (index.column() == MassStdDevColumn && m_state.mode == CalculationMode::DataReconciliation)
        field = MeasurementField::MassStdDev;
    else if (isUncertaintyColumn(index.column())) field = MeasurementField::GradeStdDev;
    else if (isGradeColumn(index.column())) field = MeasurementField::Grade;
    else return false;
    return m_session.submit(EditTableMeasurement{id, componentIdForColumn(index.column()), field, value.toString()});
}

const StreamDescriptor* TerminalProductTableModel::streamAt(int row) const {
    return row >= 0 && row < m_state.streams.size() ? &m_state.streams[row] : nullptr;
}

int TerminalProductTableModel::rowForStreamId(const QString& streamId) const {
    for (int row = 0; row < m_state.streams.size(); ++row)
        if (m_state.streams[row].streamId == streamId) return row;
    return -1;
}

int TerminalProductTableModel::completedCount() const {
    int result = 0;
    for (const auto& stream : m_state.streams)
        if (m_state.measurements.value(stream.streamId).completeFor(m_state.componentIds())) ++result;
    return result;
}

} // namespace afs
