#include "app/MainWindow.h"
#include "adapters/CanvasTopologyBuilder.h"
#include "annotations/AnnotationManager.h"
#include "core/FlotationUnit.h"
#include "document/FlowsheetDocument.h"
#include "editor/CanvasView.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FlotationUnitItem.h"
#include "services/ProjectSerializer.h"
#include "services/ProjectUndoManager.h"
#include "services/RecentProjectService.h"

#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsScene>
#include <QMessageBox>
#include <QMenu>
#include <QSet>
#include <QStatusBar>

namespace afs {

bool MainWindow::confirmSaveBeforeDestructiveAction() {
    if (!m_projectDirty) return true;
    const auto choice = QMessageBox::warning(this, tr("保存项目"),
        tr("当前项目有未保存的更改，是否先保存？"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (choice == QMessageBox::Cancel) return false;
    return choice != QMessageBox::Save || saveProject();
}

void MainWindow::newProject() {
    if (!confirmSaveBeforeDestructiveAction()) return;
    m_loadingProject = true; m_annotationManager->clearGraphicsItems(); m_scene->clear();
    m_document->replaceProjectData({}, {});
    m_document->setComponents({{DefaultComponentId, DefaultComponentName}});
    m_nextUnitId = 1;
    const QPointF center = m_view->mapToScene(m_view->viewport()->rect().center());
    m_scene->addItem(new FlotationUnitItem({takeNextUnitId(), center}));
    static_cast<FlowsheetScene*>(m_scene)->notifyTopologyChanged();
    m_loadingProject = false; m_projectPath.clear(); m_scene->clearSelection();
    static_cast<FlowsheetScene*>(m_scene)->refreshAppearance();
    if (m_undoManager) m_undoManager->clearHistory();
    setProjectDirty(true); statusBar()->showMessage(tr("已新建空白项目"), 4000);
}

bool MainWindow::deleteSelectedUnits() {
    QList<FlotationUnitItem*> selectedUnits;
    for (auto* item : m_scene->selectedItems())
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item)) selectedUnits.append(unit);
    if (selectedUnits.isEmpty()) return false;
    const QString prompt = selectedUnits.size() == 1
        ? tr("确定删除选中的单元“%1”及其相关连接吗？")
              .arg(selectedUnits.front()->unit().id)
        : tr("确定删除选中的 %1 个单元及其相关连接吗？").arg(selectedUnits.size());
    if (QMessageBox::question(this, tr("删除浮选单元"), prompt,
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return true;
    m_annotationManager->clearGraphicsItems();
    int removed = 0;
    auto* flowsheet = static_cast<FlowsheetScene*>(m_scene);
    for (auto* unit : selectedUnits) if (flowsheet->removeUnit(unit)) ++removed;
    const auto streamIds = CanvasTopologyBuilder::build(*flowsheet).graph.streamIds();
    m_document->removeUnknownStreams(QSet<QString>(streamIds.cbegin(), streamIds.cend()));
    m_annotationManager->synchronize(); updateNextUnitId();
    statusBar()->showMessage(tr("已删除 %1 个浮选单元").arg(removed), 4000);
    return true;
}

bool MainWindow::saveProject() {
    return m_projectPath.isEmpty() ? saveProjectAs() : saveProjectTo(m_projectPath);
}

bool MainWindow::saveProjectAs() {
    QString suggestedPath = m_projectPath;
    if (suggestedPath.isEmpty()) suggestedPath = QStringLiteral("flotation-project.afs.json");
    QString path = QFileDialog::getSaveFileName(this, tr("项目另存为"), suggestedPath,
        tr("AutoFlotationSheet 项目 (*.afs.json);;JSON 文件 (*.json)"));
    if (path.isEmpty()) return false;
    if (!path.endsWith(".json", Qt::CaseInsensitive)) path += ".afs.json";
    return saveProjectTo(path);
}

bool MainWindow::saveProjectTo(const QString& path) {
    QString error;
    if (!ProjectSerializer::save(*static_cast<FlowsheetScene*>(m_scene), *m_document,
                                 path, &error)) {
        QMessageBox::warning(this, tr("保存项目失败"), error); return false;
    }
    m_projectPath = path; setProjectDirty(false);
    RecentProjectService::addProject(path);
    refreshRecentProjectsMenu();
    if (m_undoManager) m_undoManager->markClean();
    statusBar()->showMessage(tr("项目已保存：%1").arg(path), 8000); return true;
}

void MainWindow::importProject() {
    const QString path = QFileDialog::getOpenFileName(this, tr("导入浮选项目"), {},
        tr("AutoFlotationSheet 项目 (*.afs.json *.json);;所有文件 (*)"));
    if (path.isEmpty()) return;
    loadProjectFromPath(path);
}

bool MainWindow::openProject(const QString& path) {
    return loadProjectFromPath(path);
}

bool MainWindow::loadProjectFromPath(const QString& path) {
    if (QFileInfo(path).absoluteFilePath() == QFileInfo(m_projectPath).absoluteFilePath()) {
        statusBar()->showMessage(tr("当前已打开此项目：%1").arg(path), 4000);
        return true;
    }
    if (!QFileInfo::exists(path)) {
        RecentProjectService::removeProject(path);
        refreshRecentProjectsMenu();
        QMessageBox::warning(this, tr("打开项目失败"), tr("项目文件不存在：\n%1").arg(path));
        return false;
    }
    if (!confirmSaveBeforeDestructiveAction()) return false;
    m_annotationManager->clearGraphicsItems();
    QString error; m_loadingProject = true;
    const bool loaded = ProjectSerializer::load(*static_cast<FlowsheetScene*>(m_scene),
                                                *m_document, path, &error);
    m_loadingProject = false;
    if (!loaded) {
        m_annotationManager->synchronize();
        QMessageBox::warning(this, tr("导入项目失败"), error); return false;
    }
    m_projectPath = path;
    RecentProjectService::addProject(path);
    refreshRecentProjectsMenu();
    if (m_undoManager) { m_undoManager->clearHistory(); m_undoManager->markClean(); }
    setProjectDirty(false); updateNextUnitId(); m_scene->clearSelection();
    static_cast<FlowsheetScene*>(m_scene)->refreshAppearance();
    const QRectF bounds = m_scene->itemsBoundingRect().adjusted(-60, -60, 60, 60);
    if (!bounds.isEmpty()) m_view->fitInView(bounds, Qt::KeepAspectRatio);
    statusBar()->showMessage(tr("项目已导入：%1").arg(path), 8000);
    return true;
}

void MainWindow::refreshRecentProjectsMenu() {
    if (!m_recentProjectsMenu) return;
    m_recentProjectsMenu->clear();
    const auto projects = RecentProjectService::projects();
    if (projects.isEmpty()) {
        auto* emptyAction = m_recentProjectsMenu->addAction(tr("暂无最近项目"));
        emptyAction->setEnabled(false);
        return;
    }

    int index = 1;
    bool hasMissingFile = false;
    for (const auto& path : projects) {
        const QFileInfo info(path);
        const bool exists = info.exists();
        hasMissingFile |= !exists;
        QString label = tr("&%1  %2").arg(index++).arg(info.fileName());
        if (!exists) label += tr("（文件不存在）");
        auto* action = m_recentProjectsMenu->addAction(label);
        action->setToolTip(path);
        action->setStatusTip(path);
        action->setCheckable(!m_projectPath.isEmpty());
        action->setChecked(info.absoluteFilePath() == QFileInfo(m_projectPath).absoluteFilePath());
        connect(action, &QAction::triggered, this, [this, path] { loadProjectFromPath(path); });
    }
    m_recentProjectsMenu->addSeparator();
    if (hasMissingFile) {
        auto* removeMissing = m_recentProjectsMenu->addAction(tr("清理失效记录"));
        connect(removeMissing, &QAction::triggered, this, [this, projects] {
            for (const auto& path : projects)
                if (!QFileInfo::exists(path)) RecentProjectService::removeProject(path);
            refreshRecentProjectsMenu();
            statusBar()->showMessage(tr("已清理不存在的最近项目记录"), 4000);
        });
    }
    auto* clearAction = m_recentProjectsMenu->addAction(tr("清空最近项目"));
    connect(clearAction, &QAction::triggered, this, [this] {
        RecentProjectService::clear();
        refreshRecentProjectsMenu();
        statusBar()->showMessage(tr("已清空最近项目记录"), 4000);
    });
}

void MainWindow::setProjectDirty(bool dirty) {
    m_projectDirty = dirty;
    const QString name = m_projectPath.isEmpty() ? tr("未命名项目")
                                                  : QFileInfo(m_projectPath).fileName();
    setWindowTitle(tr("浮选单元编辑器 · %1%2")
        .arg(name, m_projectDirty ? QStringLiteral(" *") : QString()));
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (!m_projectDirty) { event->accept(); return; }
    const auto choice = QMessageBox::warning(this, tr("保存项目"),
        tr("当前项目有未保存的更改，是否在退出前保存？"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (choice == QMessageBox::Cancel) event->ignore();
    else if (choice == QMessageBox::Save) saveProject() ? event->accept() : event->ignore();
    else event->accept();
}

} // namespace afs
