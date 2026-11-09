#include "ui/TerminalProductTableModel.h"
#include "document/FlowsheetDocument.h"
#include <utility>


namespace afs {

TerminalProductTableModel::TerminalProductTableModel(FlowsheetDocument& document, QObject* parent)
    : QAbstractTableModel(parent), m_document(document) {
    m_notifiedColumnCount = columnCount();
    connect(&document, &FlowsheetDocument::measurementChanged, this, [this](const QString& id) {
        for (int row = 0; row < m_streams.size(); ++row) {
            if (id.isEmpty() || m_streams[row].streamId == id)
                emit dataChanged(index(row, MassColumn), index(row, statusColumn()));
        }
    });
    connect(&document, &FlowsheetDocument::productNameChanged, this, [this](const QString& id) {
        for (int row = 0; row < m_streams.size(); ++row)
            if (id.isEmpty() || m_streams[row].streamId == id)
                emit dataChanged(index(row, ProductNameColumn), index(row, ProductNameColumn));
    });
    connect(&document, &FlowsheetDocument::calculationChanged, this, [this] {
        // Components/mode can change immediately before their reset signal.
        // Do not send the proxy a range with not-yet-announced columns.
        if (!m_streams.isEmpty() && m_notifiedColumnCount == columnCount())
            emit dataChanged(index(0, MassColumn), index(m_streams.size() - 1, statusColumn()));
    });
    connect(&document, &FlowsheetDocument::componentsChanged, this, [this] {
        beginResetModel(); m_notifiedColumnCount = columnCount(); endResetModel();
    });
    connect(&document, &FlowsheetDocument::calculationModeChanged, this, [this] {
        beginResetModel(); m_notifiedColumnCount = columnCount(); endResetModel();
    });
}

int TerminalProductTableModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_streams.size();
}

int TerminalProductTableModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return m_document.calculationMode() == CalculationMode::DataReconciliation
        ? 5 + 2 * m_document.components().size()
        : 4 + m_document.components().size();
}

int TerminalProductTableModel::gradeColumn(const QString& componentId) const {
    for (int i = 0; i < m_document.components().size(); ++i)
        if (m_document.components()[i].id == componentId)
            return (m_document.calculationMode() == CalculationMode::DataReconciliation ? 4 : 3)
                + (m_document.calculationMode() == CalculationMode::DataReconciliation ? 2 * i : i);
    return -1;
}
int TerminalProductTableModel::gradeStdDevColumn(const QString& componentId) const {
    const int grade = gradeColumn(componentId);
    return m_document.calculationMode() == CalculationMode::DataReconciliation ? grade + 1 : -1;
}
int TerminalProductTableModel::statusColumn() const {
    return columnCount() - 1;
}
bool TerminalProductTableModel::isGradeColumn(int column) const {
    for (const auto& component : m_document.components())
        if (gradeColumn(component.id) == column) return true;
    return false;
}
bool TerminalProductTableModel::isUncertaintyColumn(int column) const {
    if (m_document.calculationMode() != CalculationMode::DataReconciliation) return false;
    if (column == MassStdDevColumn) return true;
    for (const auto& component : m_document.components())
        if (gradeStdDevColumn(component.id) == column) return true;
    return false;
}
QString TerminalProductTableModel::componentIdForColumn(int column) const {
    for (const auto& component : m_document.components())
        if (gradeColumn(component.id) == column || gradeStdDevColumn(component.id) == column)
            return component.id;
    return {};
}

Qt::ItemFlags TerminalProductTableModel::flags(const QModelIndex& index) const {
    auto result = QAbstractTableModel::flags(index);
    if (index.column() == ProductNameColumn
        || (m_editableStreamIds.contains(m_streams[index.row()].streamId)
            && (index.column() == MassColumn || isGradeColumn(index.column())
                || isUncertaintyColumn(index.column()))))
        result |= Qt::ItemIsEditable;
    return result;
}

bool TerminalProductTableModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (role != Qt::EditRole || !index.isValid() || index.row() >= m_streams.size()) return false;
    if (index.column() == ProductNameColumn) {
        m_document.setProductName(m_streams[index.row()].streamId, value.toString());
        return true;
    }
    if (index.column() != MassColumn && !isGradeColumn(index.column())
        && !isUncertaintyColumn(index.column())) return false;
    if (!m_editableStreamIds.contains(m_streams[index.row()].streamId)) return false;
    const QString text = value.toString().trimmed();
    std::optional<double> number;
    if (!text.isEmpty()) {
        bool ok = false;
        const double parsed = text.toDouble(&ok);
        const bool valid = ok && parsed >= 0.0
            && (!isGradeColumn(index.column()) || parsed <= 100.0);
        if (!valid) return false;
        number = parsed;
    }
    const auto& id = m_streams[index.row()].streamId;
    if (index.column() == MassColumn) m_document.setDryMass(id, number);
    else if (index.column() == MassStdDevColumn
             && m_document.calculationMode() == CalculationMode::DataReconciliation) {
        if (number && *number <= 0.0) return false;
        m_document.setDryMassStdDev(id, number);
    }
    else if (isUncertaintyColumn(index.column())) {
        if (number && *number <= 0.0) return false;
        m_document.setGradeStdDev(id, componentIdForColumn(index.column()), number);
    }
    else if (isGradeColumn(index.column()))
        m_document.setGradePercent(id, componentIdForColumn(index.column()), number);
    return true;
}

void TerminalProductTableModel::setStreams(QVector<CanvasStreamDescriptor> streams) {
    QSet<QString> ids;
    for (const auto& stream : streams) ids.insert(stream.streamId);
    setStreams(std::move(streams), std::move(ids));
}

void TerminalProductTableModel::setStreams(QVector<CanvasStreamDescriptor> streams,
                                           QSet<QString> editableStreamIds) {
    beginResetModel();
    m_streams = std::move(streams);
    m_editableStreamIds = std::move(editableStreamIds);
    m_notifiedColumnCount = columnCount();
    endResetModel();
}

const CanvasStreamDescriptor* TerminalProductTableModel::streamAt(int row) const {
    return row >= 0 && row < m_streams.size() ? &m_streams[row] : nullptr;
}

int TerminalProductTableModel::rowForGraphicsItem(const QGraphicsItem* item) const {
    for (int row = 0; row < m_streams.size(); ++row)
        if (m_streams[row].graphicsItem == item) return row;
    return -1;
}

int TerminalProductTableModel::completedCount() const {
    int result = 0;
    for (const auto& stream : m_streams)
        if (m_document.measurement(stream.streamId).completeFor(m_document.componentIds())) ++result;
    return result;
}

} // namespace afs
