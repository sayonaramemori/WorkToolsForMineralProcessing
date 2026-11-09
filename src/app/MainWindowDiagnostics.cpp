#include "app/MainWindow.h"
#include "adapters/CanvasTopologyBuilder.h"
#include "document/FlowsheetDocument.h"
#include "editor/CanvasView.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/ProductLineItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/FeedJunctionItem.h"
#include "services/CalculationDiagnostics.h"
#include "ui/CalculationIssuesDock.h"
#include "ui/TerminalProductDock.h"
#include "ui/WorkspaceSidebar.h"
#include <QScopedValueRollback>
#include <QStatusBar>
namespace afs {
void MainWindow::refreshCalculationIssues(bool reveal) {
    if (!m_issuesDock) return;
    const auto* result = m_document->calculationResult();
    if (!result) { m_issuesDock->setEntries({}, "输入或流程已改变，点击检查流程重新查看问题。"); return; }
    const auto entries = CalculationDiagnostics::build(CanvasTopologyBuilder::build(*static_cast<FlowsheetScene*>(m_scene)), *m_document, *result);
    m_issuesDock->setEntries(entries, entries.isEmpty() ? "未发现计算问题。" : QString("%1 · %2 项问题；单击定位画布，双击切到表格输入。").arg(m_document->currentScenarioName()).arg(entries.size()));
    if (reveal && !entries.isEmpty()) m_workspaceSidebar->showPanel("issues");
}
void MainWindow::locateCalculationIssue(const QString& object, const QString& streamId,
                                      const QString& component, bool grade, bool edit) {
    const auto snapshot = CanvasTopologyBuilder::build(*static_cast<FlowsheetScene*>(m_scene));
    QGraphicsItem* item = nullptr; FeedJunctionItem* branchOwner = nullptr; QPointF anchor;
    for (const auto& stream : snapshot.reportStreams) if (stream.streamId == streamId) { item = stream.graphicsItem; break; }
    for (auto* candidate : m_scene->items()) if (auto* unit = dynamic_cast<FlotationUnitItem*>(candidate); unit && unit->unit().id == object) item = unit;
    if (!item) { statusBar()->showMessage("这是全局约束问题，可检查列表中相关实测值或流程连接。", 5000); return; }
    anchor = item->sceneBoundingRect().center();
    if (auto* product = dynamic_cast<ProductLineItem*>(item)) {
        anchor = product->sourceAnchorScenePosition();
        if (auto* feed = product->feedJunction()) { item = feed; branchOwner = feed; anchor = feed->sourceAnnotationAnchor(streamId); }
        else if (auto* merge = product->mergeJunction()) { item = merge; }
    }
    if (auto* input = dynamic_cast<InputLineItem*>(item); input && !input->isVisible()) { item = input->unit(); anchor = item->scenePos(); }
    {
        const QScopedValueRollback<bool> selecting(m_selectingGraphicsFromTable, true);
        m_scene->clearSelection(); item->setSelected(true);
        if (branchOwner) branchOwner->selectSourceStream(streamId);
        m_view->centerOn(anchor);
    }
    if (edit) m_workspaceSidebar->showPanel("table");
    m_terminalDock->focusMeasurement(streamId, component, grade, edit);
    schedulePropertyRefresh();
}
}
