#include "app/MainWindow.h"
#include "editor/CanvasView.h"
#include "services/ProjectUndoManager.h"
#include "ui/WorkspaceSidebar.h"
#include <QAction>
#include <QKeySequence>
#include <QMenu>
#include <QShortcut>

namespace afs {
MainWindow::UndoActions MainWindow::populateFlowMenu(QMenu* flowMenu) {
    auto* undoAction = flowMenu->addAction(tr("撤销"));
    undoAction->setShortcut(QKeySequence::Undo);
    connect(undoAction, &QAction::triggered, this, [this] {
        if (m_undoManager) m_undoManager->undo();
    });
    auto* redoAction = flowMenu->addAction(tr("重做"));
    redoAction->setShortcuts({QKeySequence::Redo, QKeySequence("Ctrl+Shift+Z")});
    connect(redoAction, &QAction::triggered, this, [this] {
        if (m_undoManager) m_undoManager->redo();
    });
    flowMenu->addSeparator();
    auto* deleteUnitAction = flowMenu->addAction(tr("删除选中单元"));
    connect(deleteUnitAction, &QAction::triggered, this, [this] { deleteSelectedUnits(); });
    auto* copyFlowAction = flowMenu->addAction(tr("复制选中对象 (Ctrl+C)"));
    connect(copyFlowAction, &QAction::triggered, this, &MainWindow::copyCanvasSelection);
    auto* pasteFlowAction = flowMenu->addAction(tr("粘贴对象 (Ctrl+V)"));
    connect(pasteFlowAction, &QAction::triggered, this, &MainWindow::pasteCanvasSelection);
    flowMenu->addSeparator();
    auto* saveTemplate = flowMenu->addAction(tr("保存选中流程为模板"));
    saveTemplate->setObjectName("saveFlowTemplateAction");
    connect(saveTemplate, &QAction::triggered, this, &MainWindow::saveFlowTemplate);
    auto* templates = flowMenu->addAction(tr("流程模板库"));
    connect(templates, &QAction::triggered, this, [this] { refreshFlowTemplates(); m_workspaceSidebar->showPanel("templates"); });
    auto* check = flowMenu->addAction(tr("检查流程 / 计算问题")); check->setObjectName("checkFlowsheetAction");
    connect(check, &QAction::triggered, this, [this] {
        calculateFlowsheet();
        m_workspaceSidebar->showPanel("issues");
    });
    flowMenu->addSeparator();
    populateEquipmentMenu(flowMenu);
    return {undoAction, redoAction};
}

void MainWindow::setupCanvasShortcuts() {
    auto* widenShortcut = new QShortcut(QKeySequence("Ctrl++"), this);
    connect(widenShortcut, &QShortcut::activated, this, [this] { resizeSelectedUnits(40.0); });
    // 部分键盘把加号报告为 Ctrl+=，同时注册以保证一致操作。
    auto* widenEqualShortcut = new QShortcut(QKeySequence("Ctrl+="), this);
    connect(widenEqualShortcut, &QShortcut::activated, this, [this] { resizeSelectedUnits(40.0); });
    auto* narrowShortcut = new QShortcut(QKeySequence("Ctrl+-"), this);
    connect(narrowShortcut, &QShortcut::activated, this, [this] { resizeSelectedUnits(-40.0); });
    auto* breakShortcut = new QShortcut(QKeySequence("Ctrl+B"), this);
    connect(breakShortcut, &QShortcut::activated, this, [this] { disconnectSelectedLines(); });
    // Keep the ordinary text/table clipboard available.  These shortcuts are
    // active only while the canvas (or one of its viewport children) has focus.
    auto* copyFlowShortcut = new QShortcut(QKeySequence::Copy, m_view);
    copyFlowShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(copyFlowShortcut, &QShortcut::activated, this, &MainWindow::copyCanvasSelection);
    auto* pasteFlowShortcut = new QShortcut(QKeySequence::Paste, m_view);
    pasteFlowShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(pasteFlowShortcut, &QShortcut::activated, this, &MainWindow::pasteCanvasSelection);

}
} // namespace afs
