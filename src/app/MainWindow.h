#pragma once

#include "services/FlowGroupClipboard.h"

#include <QMainWindow>
#include <QString>

class QAction;
class QGraphicsScene;
class QComboBox;
class QCloseEvent;
class QLabel;
class QMenu;

namespace afs {

class CanvasView;
class FlowsheetDocument;
class TerminalProductDock;
class AnnotationManager;
class OperationLogDock;
class ProjectUndoManager;
enum class UnitKind;

class MainWindow final : public QMainWindow {
public:
    MainWindow();
    ~MainWindow() override;
    // Enables opening a project from the process command line as well as from
    // the Project menu. Keeping both paths on the same loader makes startup
    // validation and normal interactive imports behave identically.
    bool openProject(const QString& path);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    QGraphicsScene* m_scene;
    CanvasView* m_view;
    QAction* m_themeAction{nullptr};
    QAction* m_annotationsAction{nullptr};
    FlowsheetDocument* m_document{nullptr};
    TerminalProductDock* m_terminalDock{nullptr};
    AnnotationManager* m_annotationManager{nullptr};
    bool m_darkTheme{false};
    bool m_projectDirty{false};
    bool m_loadingProject{false};
    int m_nextUnitId{1};
    QString m_projectPath;
    QComboBox* m_scenarioCombo{nullptr};
    QLabel* m_selectionStatusLabel{nullptr};
    OperationLogDock* m_operationLog{nullptr};
    ProjectUndoManager* m_undoManager{nullptr};
    FlowGroupClipboard m_flowClipboard;
    QMenu* m_recentProjectsMenu{nullptr};

    void addFlotationUnit();
    void addMagneticSeparator(UnitKind kind);
    void addGravitySeparator(UnitKind kind);
    void addSizingSeparator(UnitKind kind);
    void addStoragePool(UnitKind kind);
    void addThreeProductUnit();
    void addThreeProductScreening();
    void addThreeProductDemediumScreen();
    void addBinarySplitter();
    void newProject();
    bool deleteSelectedUnits();
    void copySelectedFlowGroup();
    void pasteFlowGroup();
    bool confirmSaveBeforeDestructiveAction();
    bool saveProject();
    bool saveProjectAs();
    bool saveProjectTo(const QString& path);
    void importProject();
    bool loadProjectFromPath(const QString& path);
    void refreshRecentProjectsMenu();
    void updateNextUnitId();
    [[nodiscard]] QString takeNextUnitId();
    void resizeSelectedUnits(double delta);
    void disconnectSelectedLines();
    void exportScene();
    void exportExcelData();
    void editExcelExportOrder();
    void applyTheme(bool dark, bool saveSetting = true);
    void refreshTerminalProducts();
    void refreshProductNames();
    void editAnnotationTextStyle();
    void syncCanvasSelectionToTable();
    void refreshSelectionStatus();
    void calculateFlowsheet();
    void refreshSelectedResult();
    void refreshScenarioUi();
    void addScenario(bool copyCurrent);
    void renameScenario();
    void deleteScenario();
    void compareScenarios();
    void setProjectDirty(bool dirty = true);
    void appendOperationLog(const QString& message);
};

} // namespace afs
