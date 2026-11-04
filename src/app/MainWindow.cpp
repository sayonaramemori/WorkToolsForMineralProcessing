#include "app/MainWindow.h"
#include "adapters/CanvasTopologyBuilder.h"
#include "annotations/AnnotationManager.h"
#include "document/FlowsheetDocument.h"
#include "editor/CanvasView.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FlotationUnitItem.h"
#include "services/ProjectUndoManager.h"
#include "services/ThemeService.h"
#include "ui/TerminalProductDock.h"
#include "ui/OperationLogDock.h"

#include <QAction>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QScopedValueRollback>
#include <QStatusBar>
#include <QUndoStack>

namespace afs {

MainWindow::MainWindow() {
    m_scene = new FlowsheetScene(this);
    m_scene->setSceneRect(-900, -600, 1800, 1200);
    m_view = new CanvasView(m_scene, this);
    m_view->setRenderHints(QPainter::Antialiasing);
    m_view->setDragMode(QGraphicsView::RubberBandDrag);
    m_view->setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
    setCentralWidget(m_view);

    m_selectionStatusLabel = new QLabel(tr("当前未选中浮选单元"), this);
    m_selectionStatusLabel->setMinimumWidth(260);
    m_selectionStatusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    statusBar()->addPermanentWidget(m_selectionStatusLabel, 1);

    m_document = new FlowsheetDocument(this);
    m_annotationManager = new AnnotationManager(
        *static_cast<FlowsheetScene*>(m_scene), *m_document, this);
    m_view->setVerticalNudgeHandler([this](qreal delta) {
        return m_annotationManager->nudgeSelectedReagents(delta);
    });
    m_view->setDeleteHandler([this] { return deleteSelectedUnits(); });
    m_terminalDock = new TerminalProductDock(*m_document, this);
    addDockWidget(Qt::RightDockWidgetArea, m_terminalDock);

    m_operationLog = new OperationLogDock(this);
    addDockWidget(Qt::BottomDockWidgetArea, m_operationLog);
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
    resizeDocks({m_operationLog}, {130}, Qt::Vertical);
    connect(statusBar(), &QStatusBar::messageChanged, this,
            [this](const QString& message) {
                if (!message.isEmpty()) appendOperationLog(message);
            });
    connect(static_cast<FlowsheetScene*>(m_scene), &FlowsheetScene::topologyChanged,
            this, [this] { refreshTerminalProducts(); });
    connect(static_cast<FlowsheetScene*>(m_scene), &FlowsheetScene::topologyChanged,
            this, [this] { setProjectDirty(); });
    connect(static_cast<FlowsheetScene*>(m_scene), &FlowsheetScene::geometryChanged,
            this, [this] { setProjectDirty(); });
    connect(m_document, &FlowsheetDocument::projectChanged,
            this, [this] { setProjectDirty(); });
    connect(m_scene, &QGraphicsScene::selectionChanged,
            this, [this] { syncCanvasSelectionToTable(); });
    connect(m_terminalDock, &TerminalProductDock::graphicsItemRequested, this,
            [this](QGraphicsItem* item) {
                if (!item) return;
                // Selecting a table row navigates within its existing filter.
                // Do not invalidate the proxy while it handles the click.
                const QScopedValueRollback<bool> selecting(m_selectingGraphicsFromTable, true);
                m_scene->clearSelection();
                item->setSelected(true);
                m_view->centerOn(item);
            });
    connect(m_terminalDock, &TerminalProductDock::calculationRequested,
            this, [this] { calculateFlowsheet(); });
    connect(m_document, &FlowsheetDocument::calculationChanged,
            this, [this] { refreshSelectedResult(); });
    connect(m_document, &FlowsheetDocument::productNameChanged,
            this, [this](const QString&) { refreshProductNames(); });
    connect(m_document, &FlowsheetDocument::annotationTextSettingsChanged,
            this, [this] { refreshProductNames(); });
    connect(m_document, &FlowsheetDocument::scenariosChanged,
            this, [this] { refreshScenarioUi(); });
    connect(m_document, &FlowsheetDocument::currentScenarioChanged, this, [this] {
        refreshScenarioUi();
        m_terminalDock->setSnapshot(CanvasTopologyBuilder::build(
            *static_cast<FlowsheetScene*>(m_scene)));
        m_scene->clearSelection();
        statusBar()->showMessage(tr("已切换至：%1").arg(m_document->currentScenarioName()), 3000);
    });

    const auto undoActions = createToolbar();

    addFlotationUnit();
    refreshScenarioUi();
    refreshTerminalProducts();
    applyTheme(ThemeService::loadDarkPreference(), false);
    setWindowTitle(tr("浮选单元编辑器"));
    setProjectDirty(true);
    resize(1200, 760);
    m_undoManager = new ProjectUndoManager(
        *static_cast<FlowsheetScene*>(m_scene), *m_document,
        [this] {
            m_loadingProject = true;
            m_annotationManager->clearGraphicsItems();
            m_scene->clearSelection();
        },
        [this] {
            m_loadingProject = false;
            updateNextUnitId();
            static_cast<FlowsheetScene*>(m_scene)->refreshAppearance();
            refreshSelectionStatus();
            setProjectDirty(true);
        }, this);
    connect(static_cast<FlowsheetScene*>(m_scene), &FlowsheetScene::topologyChanged,
            m_undoManager, [this] { m_undoManager->scheduleCheckpoint("编辑流程拓扑"); });
    connect(static_cast<FlowsheetScene*>(m_scene), &FlowsheetScene::geometryChanged,
            m_undoManager, [this] { m_undoManager->scheduleCheckpoint("调整流程图"); });
    connect(m_document, &FlowsheetDocument::projectChanged,
            m_undoManager, [this] { m_undoManager->scheduleCheckpoint("编辑项目数据"); });
    connect(m_undoManager, &ProjectUndoManager::restoreFailed, this,
            [this](const QString& error) {
                QMessageBox::warning(this, tr("撤销失败"), error);
            });
    undoActions.undo->setEnabled(false);
    undoActions.redo->setEnabled(false);
    connect(m_undoManager->stack(), &QUndoStack::canUndoChanged,
            undoActions.undo, &QAction::setEnabled);
    connect(m_undoManager->stack(), &QUndoStack::canRedoChanged,
            undoActions.redo, &QAction::setEnabled);
    m_undoManager->initialize();
    statusBar()->showMessage(
        tr("单击选择，拖动图元；滚轮缩放画布，按住鼠标中键拖动画布"));
}

MainWindow::~MainWindow() {
    // Removing selected annotations emits selectionChanged. Stop scene
    // callbacks before any of the panels they access are destroyed.
    if (m_scene) m_scene->blockSignals(true);
    // Do not leave QObject to choose the destruction order here. The dock
    // models, annotation manager and undo stack all retain non-owning
    // references to the document and/or scene. In an imported project those
    // references can still carry queued model state while QMainWindow tears
    // down its child widgets. Dispose of each dependency while its owners are
    // unquestionably alive, then detach the scene from the view.
    if (m_undoManager) {
        m_undoManager->shutdown();
        delete m_undoManager;
        m_undoManager = nullptr;
    }

    if (m_terminalDock) {
        removeDockWidget(m_terminalDock);
        delete m_terminalDock;
        m_terminalDock = nullptr;
    }
    if (m_operationLog) {
        removeDockWidget(m_operationLog);
        delete m_operationLog;
        m_operationLog = nullptr;
    }

    if (m_annotationManager) {
        if (m_scene) m_scene->removeEventFilter(m_annotationManager);
        m_annotationManager->clearGraphicsItems();
        delete m_annotationManager;
        m_annotationManager = nullptr;
    }
    if (m_scene) {
        m_scene->clear();
        if (m_view) m_view->setScene(nullptr);
        delete m_scene;
        m_scene = nullptr;
    }
    if (m_document) {
        m_document->disconnect();
        delete m_document;
        m_document = nullptr;
    }
}

} // namespace afs
