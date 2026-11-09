#include "app/MainWindow.h"
#include "app/CalculationController.h"
#include "app/TerminalTableController.h"
#include "adapters/CanvasTopologyBuilder.h"
#include "document/FlowsheetDocument.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"
#include "ui/TerminalProductDock.h"
#include <QSet>
#include <QStatusBar>
#include <algorithm>

namespace afs {

void MainWindow::refreshTerminalProducts() {
    // Import emits topologyChanged so views can rebuild their snapshots, but
    // the imported scenarios have just been recalculated and must be retained.
    if (!m_loadingProject) m_document->invalidateAllCalculations();
    auto snapshot = CanvasTopologyBuilder::build(*static_cast<FlowsheetScene*>(m_scene));
    m_tableController->setStreams(snapshot.reportStreams);
    m_calculationController->setSnapshot(std::move(snapshot));
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
    schedulePropertyRefresh();
    const auto selected = m_scene->selectedItems();
    if (!m_selectingGraphicsFromTable) {
        QSet<QString> selectedUnitIds;
        for (auto* item : selected) {
            if (const auto* unit = dynamic_cast<FlotationUnitItem*>(item))
                selectedUnitIds.insert(unit->unit().id);
        }
        m_terminalDock->setSelectedUnitIds(std::move(selectedUnitIds));
        const auto index = CanvasTopologyBuilder::buildItemIndex(*static_cast<FlowsheetScene*>(m_scene));
        m_terminalDock->selectStreams(index.streamsForItem(selected.isEmpty() ? nullptr : selected.first()));
    }
    refreshSelectionStatus();
}

void MainWindow::calculateFlowsheet() {
    auto result = m_calculationController->calculate();
    const bool complete = result.complete;
    const int issueCount = result.issues.size();
    const bool hasBalanceConflict = std::any_of(result.issues.cbegin(), result.issues.cend(),
        [](const topology::TopologyIssue& issue) {
            return issue.code == topology::IssueCode::InconsistentBalance
                && issue.severity == topology::IssueSeverity::Error;
        });
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
    refreshCalculationIssues(hasBalanceConflict);
}

} // namespace afs
