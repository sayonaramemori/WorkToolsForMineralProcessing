#include "ui/TerminalProductDock.h"
#include "ui/TerminalProductTableModel.h"
#include "ui/TerminalProductViewSupport.h"
#include <QComboBox>
#include <QItemSelectionModel>
#include <QTableView>
#include <utility>

namespace afs {

void TerminalProductDock::setSnapshot(CanvasTopologySnapshot snapshot) {
    m_snapshot = std::move(snapshot);
    QSet<QString> editableIds;
    for (const auto& stream : m_snapshot.reportStreams) editableIds.insert(stream.streamId);
    m_model->setStreams(m_snapshot.reportStreams, std::move(editableIds));
    m_filterModel->setSelectedUnitIds(m_selectedUnitIds);
    updateSummary();
}

void TerminalProductDock::setSelectedUnitIds(QSet<QString> unitIds) {
    if (m_selectedUnitIds == unitIds) return;
    m_selectedUnitIds = std::move(unitIds);
    m_filterModel->setSelectedUnitIds(m_selectedUnitIds);
    updateSummary();
}

void TerminalProductDock::selectGraphicsItem(const QGraphicsItem* item) {
    const bool keepCurrentRow = !selectedStreamIdForGraphicsItem(item).isEmpty();
    const int row = keepCurrentRow ? -1 : m_model->rowForGraphicsItem(item);
    if (!keepCurrentRow && row < 0) { m_table->clearSelection(); return; }
    const QModelIndex target = keepCurrentRow ? m_table->currentIndex()
        : m_filterModel->mapFromSource(m_model->index(row, TerminalProductTableModel::MassColumn));
    if (!target.isValid()) { m_table->clearSelection(); return; }
    m_table->selectionModel()->select(target, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    m_table->setCurrentIndex(target);
    m_table->scrollTo(target);
}

QString TerminalProductDock::selectedStreamIdForGraphicsItem(const QGraphicsItem* item) const {
    const QModelIndex current = m_filterModel->mapToSource(m_table->currentIndex());
    const auto* currentStream = current.isValid() ? m_model->streamAt(current.row()) : nullptr;
    // Fresh feed and total feed can share a junction item. Retain the exact
    // row and editor column the user clicked rather than selecting its first row.
    return currentStream && currentStream->graphicsItem == item ? currentStream->streamId : QString{};
}

bool TerminalProductDock::focusMeasurement(const QString& streamId, const QString& componentId, bool grade, bool edit) {
    int row = -1;
    for (int i = 0; i < m_model->rowCount(); ++i)
        if (m_model->streamAt(i)->streamId == streamId) { row = i; break; }
    if (row < 0) return false;
    // A diagnostic target must be reachable even outside the user's filter.
    m_filterCombo->setCurrentIndex(m_filterCombo->findData(StreamFilterProxyModel::All));
    int column = grade ? m_model->gradeColumn(componentId) : TerminalProductTableModel::MassColumn;
    if (column < 0) column = TerminalProductTableModel::MassColumn;
    const auto target = m_filterModel->mapFromSource(m_model->index(row, column));
    if (!target.isValid()) return false;
    m_table->setCurrentIndex(target); m_table->selectionModel()->select(target, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    m_table->scrollTo(target);
    if (m_table->isVisible()) m_table->setFocus();
    if (edit) m_table->edit(target);
    return true;
}

} // namespace afs
