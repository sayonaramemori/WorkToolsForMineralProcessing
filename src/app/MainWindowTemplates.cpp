#include "ui/WorkspaceSidebar.h"
#include "app/MainWindow.h"
#include "app/FlowTemplatePreviewSession.h"
#include "document/FlowsheetDocument.h"
#include "editor/CanvasView.h"
#include "editor/FlowsheetScene.h"
#include "services/FlowTemplateStore.h"
#include "services/FlowGroupSnapshotOperations.h"
#include "ui/FlowTemplateDock.h"
#include <QMessageBox>
#include <QLineF>
#include <QStatusBar>
namespace afs {
void MainWindow::refreshFlowTemplates() {
    QString warning; const auto templates = FlowTemplateStore().list(&warning);
    m_templatesDock->setTemplates(templates, warning);
}
void MainWindow::previewFlowTemplate(const QString& id) {
    // Detach before replacing a session so the view never references a scene
    // whose annotations/document are already being destroyed.
    m_templatesDock->setPreview(nullptr);
    m_templatePreview.reset();
    if (id.isEmpty()) return;
    QString error; const auto snapshot = FlowTemplateStore().load(id, &error);
    if (!snapshot) { m_templatesDock->setPreview(nullptr, "无法预览此模板：" + error); return; }
    m_templatePreview = std::make_unique<FlowTemplatePreviewSession>(*snapshot, m_document->annotationTextSettings());
    m_templatesDock->setPreview(m_templatePreview->scene());
}
void MainWindow::saveFlowTemplate() {
    flow_group::Snapshot snapshot;
    const auto captured = flow_group::capture(*static_cast<FlowsheetScene*>(m_scene), *m_document, snapshot);
    if (!captured.units) { statusBar()->showMessage("请先选中要保存为模板的设备，可同时选择文字。", 4000); return; }
    const auto name = m_templatesDock->askTemplateName(); if (name.isEmpty()) return;
    QString error; const auto id = FlowTemplateStore().save(name, snapshot, &error);
    if (id.isEmpty()) { QMessageBox::warning(this, "保存模板失败", error); return; }
    refreshFlowTemplates(); m_workspaceSidebar->showPanel("templates");
    statusBar()->showMessage(QString("已保存模板“%1”，包含 %2 个设备").arg(name).arg(captured.units), 4000);
}
void MainWindow::insertFlowTemplate(const QString& id) {
    QString error; const auto snapshot = FlowTemplateStore().load(id, &error);
    if (!snapshot) { QMessageBox::warning(this, "插入模板失败", error); return; }
    QPointF center; for (const auto& unit : snapshot->units) center += unit.position;
    center /= snapshot->units.size();
    const auto viewportCenter = m_view->mapToScene(m_view->viewport()->rect().center());
    if (id == m_lastTemplateId && QLineF(viewportCenter, m_lastTemplateCenter).length() < 1)
        ++m_templateInsertCount;
    else m_templateInsertCount = 1;
    m_lastTemplateId = id; m_lastTemplateCenter = viewportCenter;
    const auto offset = viewportCenter - center + QPointF(40, 40) * m_templateInsertCount;
    const auto pasted = flow_group::paste(*snapshot, offset, *static_cast<FlowsheetScene*>(m_scene), *m_document, [this] { return takeNextUnitId(); });
    synchronizePastedAnnotations(pasted);
    statusBar()->showMessage(QString("已插入模板：%1 个设备、%2 个文字标注、%3 个药剂标注").arg(pasted.units).arg(pasted.noteIds.size()).arg(pasted.reagents), 4000);
}
}
