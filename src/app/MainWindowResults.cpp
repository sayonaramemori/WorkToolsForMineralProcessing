#include "app/MainWindow.h"
#include "adapters/CanvasTopologyBuilder.h"
#include "document/FlowsheetDocument.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FeedJunctionItem.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"
#include "services/FlowsheetCalculationService.h"
#include "ui/TerminalProductDock.h"
#include <QSet>
#include <QStatusBar>

namespace afs {

void MainWindow::refreshTerminalProducts() {
    // Import emits topologyChanged so views can rebuild their snapshots, but
    // the imported scenarios have just been recalculated and must be retained.
    if (!m_loadingProject) m_document->invalidateAllCalculations();
    m_terminalDock->setSnapshot(CanvasTopologyBuilder::build(*static_cast<FlowsheetScene*>(m_scene)));
    refreshProductNames();
    syncCanvasSelectionToTable();
}

void MainWindow::refreshProductNames() {
    for (auto* item : m_scene->items()) {
        if (auto* product = dynamic_cast<ProductLineItem*>(item)) {
            product->setTextSettings(m_document->annotationTextSettings());
            product->setProductName(m_document->productName(product->streamId()));
        } else if (auto* merge = dynamic_cast<MergeJunctionItem*>(item)) {
            merge->setTextSettings(m_document->annotationTextSettings());
            merge->setProductName(m_document->productName(merge->outputStreamId()));
        }
    }
}

void MainWindow::syncCanvasSelectionToTable() {
    const auto selected = m_scene->selectedItems();
    if (!m_selectingGraphicsFromTable) {
        QSet<QString> selectedUnitIds;
        for (auto* item : selected) {
            if (const auto* unit = dynamic_cast<FlotationUnitItem*>(item))
                selectedUnitIds.insert(unit->unit().id);
        }
        m_terminalDock->setSelectedUnitIds(std::move(selectedUnitIds));
        m_terminalDock->selectGraphicsItem(selected.isEmpty() ? nullptr : selected.first());
    }
    refreshSelectedResult();
    refreshSelectionStatus();
}

void MainWindow::calculateFlowsheet() {
    auto result = FlowsheetCalculationService::calculate(
        *static_cast<FlowsheetScene*>(m_scene), *m_document);
    const bool complete = result.complete;
    const int issueCount = result.issues.size();
    m_document->setCalculationResult(std::move(result));
    if (!complete) {
        const auto* storedResult = m_document->calculationResult();
        if (storedResult) for (const auto& issue : storedResult->issues) {
            appendOperationLog(tr("%1：%2").arg(
                issue.severity == topology::IssueSeverity::Error ? tr("错误") : tr("提示"),
                issue.message));
        }
    }
    statusBar()->showMessage(complete ? tr("平衡计算成功")
        : tr("计算未完成，共发现 %1 个问题").arg(issueCount), 5000);
}

void MainWindow::refreshSelectedResult() {
    const auto selected = m_scene->selectedItems();
    if (selected.isEmpty()) {
        m_terminalDock->clearResultDetails();
        return;
    }
    auto* item = selected.first();
    if (auto* product = dynamic_cast<ProductLineItem*>(item)) {
        m_terminalDock->showStreamResult(product->streamId());
    } else if (auto* merge = dynamic_cast<MergeJunctionItem*>(item)) {
        m_terminalDock->showStreamResult(merge->outputStreamId());
    } else if (auto* feedJunction = dynamic_cast<FeedJunctionItem*>(item)) {
        const auto streamId = m_terminalDock->selectedStreamIdForGraphicsItem(feedJunction);
        m_terminalDock->showStreamResult(streamId.isEmpty() ? feedJunction->outputStreamId() : streamId);
    } else if (auto* unit = dynamic_cast<FlotationUnitItem*>(item)) {
        if (hasCenteredVerticalPorts(unit->unit().kind)) {
            m_terminalDock->showStreamResult(unit->products().front()->streamId());
            return;
        }
        const auto snapshot = CanvasTopologyBuilder::build(*static_cast<FlowsheetScene*>(m_scene));
        m_terminalDock->showUnitResult(unit->unit().id, snapshot.graph);
    } else {
        m_terminalDock->clearResultDetails();
    }
}

} // namespace afs
