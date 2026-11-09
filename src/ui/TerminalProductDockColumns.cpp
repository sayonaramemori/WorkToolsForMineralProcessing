#include "ui/TerminalProductDock.h"
#include "document/FlowsheetDocument.h"
#include "ui/TerminalProductTableModel.h"
#include "ui/TerminalProductViewSupport.h"
#include <QHeaderView>
#include <QScopedValueRollback>
#include <QTableView>
#include <QTimer>
#include <numeric>

namespace afs {

void TerminalProductDock::configureColumns() {
    const QScopedValueRollback<bool> applying(m_applyingColumnWidths, true);
    m_columnsManuallyResized = false;
    auto* header = m_table->horizontalHeader();
    // Keep every column interactive. Initial fitting stops after a manual
    // resize so window/page changes cannot overwrite the user's widths.
    header->setMinimumSectionSize(48);
    header->setStretchLastSection(false);
    for (int column = 0; column < m_model->columnCount(); ++column)
        header->setSectionResizeMode(column, QHeaderView::Interactive);
    m_table->setColumnWidth(TerminalProductTableModel::NameColumn, 210);
    m_table->setColumnWidth(TerminalProductTableModel::ProductNameColumn, 110);
    m_table->setColumnWidth(TerminalProductTableModel::MassColumn, 82);
    m_table->setItemDelegateForColumn(TerminalProductTableModel::MassColumn,
                                      new NumberDelegate(1.0e12, m_table));
    if (m_document.calculationMode() == CalculationMode::DataReconciliation) {
        m_table->setColumnWidth(TerminalProductTableModel::MassStdDevColumn, 72);
        m_table->setItemDelegateForColumn(TerminalProductTableModel::MassStdDevColumn,
                                          new NumberDelegate(1.0e12, m_table));
    }
    for (const auto& component : m_document.components()) {
        const int grade = m_model->gradeColumn(component.id);
        m_table->setColumnWidth(grade, 88);
        m_table->setItemDelegateForColumn(grade, new NumberDelegate(100.0, m_table));
        const int sigma = m_model->gradeStdDevColumn(component.id);
        if (sigma >= 0) {
            m_table->setColumnWidth(sigma, 72);
            m_table->setItemDelegateForColumn(sigma, new NumberDelegate(100.0, m_table));
        }
    }
    m_table->setColumnWidth(m_model->statusColumn(), 82);
    m_table->setItemDelegateForColumn(m_model->statusColumn(), new StatusDelegate(m_table));
    m_defaultColumnWidths.clear();
    for (int column = 0; column < m_model->columnCount(); ++column)
        m_defaultColumnWidths.append(header->sectionSize(column));
    scheduleColumnFit();
}

void TerminalProductDock::scheduleColumnFit() {
    if (m_columnFitPending || m_columnsManuallyResized) return;
    m_columnFitPending = true;
    QTimer::singleShot(0, this, [this] {
        m_columnFitPending = false;
        fitColumnsToViewport();
    });
}

void TerminalProductDock::fitColumnsToViewport() {
    auto* header = m_table->horizontalHeader();
    if (m_columnsManuallyResized || !m_table->isVisible()
        || m_defaultColumnWidths.size() != header->count()) return;
    const int total = std::accumulate(m_defaultColumnWidths.cbegin(), m_defaultColumnWidths.cend(), 0);
    if (!total) return;
    const int available = m_table->viewport()->width();
    const QScopedValueRollback<bool> applying(m_applyingColumnWidths, true);
    int accumulated = 0, used = 0;
    for (int column = 0; column < header->count(); ++column) {
        accumulated += m_defaultColumnWidths[column];
        // Preserve readable defaults when many components require scrolling.
        // Cumulative rounding fills the viewport without leaving a spare strip.
        const int edge = available >= total ? int(qint64(accumulated) * available / total) : accumulated;
        header->resizeSection(column, edge - used);
        used = edge;
    }
}

} // namespace afs
