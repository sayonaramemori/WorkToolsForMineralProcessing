#include "app/MainWindow.h"
#include "app/FlowTemplatePreviewSession.h"
#include "document/FlowsheetDocument.h"
#include "editor/CanvasProperties.h"
#include "editor/FlowsheetScene.h"
#include "ui/ObjectPropertiesDock.h"
#include "ui/CalculationIssuesDock.h"
#include "ui/FlowTemplateDock.h"
#include "ui/OperationLogDock.h"
#include "ui/TerminalProductDock.h"
#include "ui/WorkspaceSidebar.h"
#include "services/FlowTemplateStore.h"
#include <QMessageBox>
#include <QStatusBar>
#include <QTimer>
namespace afs {
void MainWindow::setupWorkspaceTools() {
    auto* scene = static_cast<FlowsheetScene*>(m_scene);
    m_propertiesDock = new ObjectPropertiesDock(this);
    m_templatesDock = new FlowTemplateDock(this);
    m_issuesDock = new CalculationIssuesDock(this);
    m_workspaceSidebar = new WorkspaceSidebar(this);
    addDockWidget(Qt::RightDockWidgetArea, m_workspaceSidebar);
    m_workspaceSidebar->addPanel("table", "表格", m_terminalDock);
    m_workspaceSidebar->addPanel("properties", "对象属性", m_propertiesDock);
    m_workspaceSidebar->addPanel("issues", "计算问题", m_issuesDock);
    m_workspaceSidebar->addPanel("templates", "模板库", m_templatesDock);
    m_workspaceSidebar->setLogPanel(m_operationLog);
    m_workspaceSidebar->showPanel("table");
    resizeDocks({m_workspaceSidebar}, {700}, Qt::Horizontal);
    connect(scene, &FlowsheetScene::geometryChanged, this, &MainWindow::schedulePropertyRefresh);
    connect(scene, &FlowsheetScene::topologyChanged, this, &MainWindow::schedulePropertyRefresh);
    connect(m_document, &FlowsheetDocument::productNameChanged, this, [this] { schedulePropertyRefresh(); refreshCalculationIssues(); });
    connect(m_document, &FlowsheetDocument::scenariosChanged, this, [this] { refreshCalculationIssues(); });
    connect(m_document, &FlowsheetDocument::calculationChanged, this, [this] { refreshCalculationIssues(); });
    connect(m_propertiesDock, &ObjectPropertiesDock::numericEdited, this, [this, scene](const QString& object, const QString& key, double value) {
        if (CanvasProperties::apply(*scene, object, key, value)) schedulePropertyRefresh();
    });
    connect(m_propertiesDock, &ObjectPropertiesDock::nameEdited, this, [this, scene](const QString& object, const QString& name) {
        const auto state = CanvasProperties::describe(*scene);
        if (state.objectKey == object && !state.streamId.isEmpty()) m_document->setProductName(state.streamId, name);
    });
    connect(m_propertiesDock, &ObjectPropertiesDock::branchSelected, this, [this, scene](const QString& object, const QString& branch) {
        CanvasProperties::selectBranch(*scene, object, branch); schedulePropertyRefresh();
    });
    connect(m_propertiesDock, &ObjectPropertiesDock::resetRequested, this, [this, scene](const QString& object) {
        CanvasProperties::resetRoutes(*scene, object); schedulePropertyRefresh();
    });
    connect(m_issuesDock, &CalculationIssuesDock::checkRequested, this, &MainWindow::calculateFlowsheet);
    connect(m_issuesDock, &CalculationIssuesDock::locateRequested, this, &MainWindow::locateCalculationIssue);
    connect(m_templatesDock, &FlowTemplateDock::saveSelectionRequested, this, &MainWindow::saveFlowTemplate);
    connect(m_templatesDock, &FlowTemplateDock::insertRequested, this, &MainWindow::insertFlowTemplate);
    connect(m_templatesDock, &FlowTemplateDock::refreshRequested, this, &MainWindow::refreshFlowTemplates);
    connect(m_templatesDock, &FlowTemplateDock::previewRequested, this, &MainWindow::previewFlowTemplate);
    connect(m_document, &FlowsheetDocument::annotationTextSettingsChanged, this, [this] {
        if (m_templatePreview) m_templatePreview->refreshAppearance(m_document->annotationTextSettings());
    });
    connect(m_templatesDock, &FlowTemplateDock::renameRequested, this, [this](const QString& id, const QString& name) {
        QString error; if (!FlowTemplateStore().rename(id, name, &error)) QMessageBox::warning(this, "模板重命名失败", error);
        refreshFlowTemplates();
    });
    connect(m_templatesDock, &FlowTemplateDock::removeRequested, this, [this](const QString& id) {
        QString error; if (!FlowTemplateStore().remove(id, &error)) QMessageBox::warning(this, "模板删除失败", error);
        refreshFlowTemplates();
    });
    refreshFlowTemplates(); schedulePropertyRefresh();
}
void MainWindow::schedulePropertyRefresh() {
    if (m_propertyRefreshPending || !m_propertiesDock) return;
    m_propertyRefreshPending = true;
    QTimer::singleShot(0, this, [this] {
        m_propertyRefreshPending = false;
        if (!m_scene || !m_document || !m_propertiesDock) return;
        const auto state = CanvasProperties::describe(*static_cast<FlowsheetScene*>(m_scene));
        m_propertiesDock->setState(state, m_document->productName(state.streamId));
    });
}
}
