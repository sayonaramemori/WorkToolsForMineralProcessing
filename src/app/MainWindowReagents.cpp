#include "app/MainWindow.h"
#include "adapters/CanvasTopologyBuilder.h"
#include "annotations/AnnotationManager.h"
#include "document/FlowsheetDocument.h"
#include "editor/CanvasView.h"
#include "editor/FlowsheetScene.h"
#include "services/ProjectUndoManager.h"
#include <QStatusBar>

namespace afs {
void MainWindow::setupReagentClipboard() {
    connect(m_annotationManager, &AnnotationManager::reagentCopyRequested, this, &MainWindow::copyReagentAnnotation);
    connect(m_annotationManager, &AnnotationManager::reagentPasteRequested, this, &MainWindow::pasteReagentAnnotation);
}

void MainWindow::copyReagentAnnotation(const QString& id) {
    if (!m_reagentClipboard.copy(m_document->annotationRecord(id))) {
        statusBar()->showMessage(tr("该药剂标注已不存在，请重新选择。"), 4000); return;
    }
    m_canvasClipboardKind = CanvasClipboardKind::Reagent;
    m_annotationManager->setReagentPasteAvailable(true);
    statusBar()->showMessage(tr("已复制药剂标注，选中目标物流线后按 Ctrl+V，或右键粘贴。"), 4000);
}

void MainWindow::pasteReagentAnnotation(const QString& streamId) {
    const auto snapshot = CanvasTopologyBuilder::build(*static_cast<FlowsheetScene*>(m_scene));
    if (!snapshot.graph.stream(streamId)) {
        statusBar()->showMessage(tr("目标物流已不存在，请重新选择线路。"), 4000); return;
    }
    const auto record = m_reagentClipboard.makePaste(streamId, m_document->annotationRecords());
    if (!record) { statusBar()->showMessage(tr("请先复制一条药剂标注。"), 4000); return; }
    if (m_undoManager) m_undoManager->commitPending();
    m_document->setAnnotationRecord(*record);
    m_annotationManager->revealPastedReagent(record->id);
    if (m_undoManager) { m_undoManager->scheduleCheckpoint(tr("粘贴药剂标注")); m_undoManager->commitPending(); }
    for (auto* item : m_scene->selectedItems()) m_view->ensureVisible(item, 24, 24);
    statusBar()->showMessage(tr("已将药剂标注粘贴到 %1").arg(streamId), 4000);
}
}
