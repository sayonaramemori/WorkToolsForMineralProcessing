#include "ui/TerminalProductDock.h"
#include "ui/TerminalProductTableModel.h"
#include "ui/TerminalProductViewSupport.h"
#include <QAction>
#include <QItemSelectionModel>
#include <QMenu>
#include <QPushButton>
#include <QTableView>
#include <utility>

namespace afs {

void TerminalProductDock::setSnapshot(CanvasTopologySnapshot snapshot) {
    m_snapshot = std::move(snapshot);
    QSet<QString> editableIds;
    for (const auto& stream : m_snapshot.reportStreams) editableIds.insert(stream.streamId);
    m_model->setStreams(m_snapshot.reportStreams, std::move(editableIds));
    m_filterModel->setSelectedUnitIds(m_selectedUnitIds);
    rebuildInterestMenu();
}

void TerminalProductDock::setSelectedUnitIds(QSet<QString> unitIds) {
    if (m_selectedUnitIds == unitIds) return;
    m_selectedUnitIds = std::move(unitIds);
    m_filterModel->setSelectedUnitIds(m_selectedUnitIds);
    updateSummary();
}

void TerminalProductDock::rebuildInterestMenu() {
    m_interestMenu->clear();
    QSet<QString> validSelection;
    for (const auto& object : m_snapshot.interestObjects) {
        auto* action = m_interestMenu->addAction(object.displayName);
        action->setCheckable(true);
        action->setData(object.id);
        if (m_interestedOwners.contains(object.id)) {
            action->setChecked(true);
            validSelection.insert(object.id);
        }
        connect(action, &QAction::toggled, this, [this] {
            QSet<QString> selected;
            int count = 0;
            for (auto* candidate : m_interestMenu->actions()) {
                if (!candidate->isChecked()) continue;
                selected.insert(candidate->data().toString());
                ++count;
            }
            m_interestedOwners = selected;
            m_filterModel->setInterestedOwners(std::move(selected));
            m_interestButton->setText(count == 0
                ? "关注对象：全部" : QString("关注对象：%1 个").arg(count));
            updateSummary();
        });
    }
    if (m_snapshot.interestObjects.isEmpty()) {
        auto* empty = m_interestMenu->addAction("暂无可选对象");
        empty->setEnabled(false);
    }
    m_interestedOwners = std::move(validSelection);
    m_filterModel->setInterestedOwners(m_interestedOwners);
    m_interestButton->setText(m_interestedOwners.isEmpty()
        ? "关注对象：全部" : QString("关注对象：%1 个").arg(m_interestedOwners.size()));
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

} // namespace afs
