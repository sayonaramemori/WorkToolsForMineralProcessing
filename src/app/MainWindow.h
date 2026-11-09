#pragma once

#include "services/FlowGroupClipboard.h"
#include "services/ReagentClipboard.h"

#include <QMainWindow>
#include <QString>
#include <memory>

class QAction;
class QGraphicsScene;
class QComboBox;
class QCloseEvent;
class QLabel;
class QMenu;

namespace afs {

class CanvasView;
class CalculationController;
class TerminalTableController;
class FlowsheetDocument;
class TerminalProductDock;
class AnnotationManager;
class OperationLogDock;
class ProjectUndoManager;
class ObjectPropertiesDock;
class CalculationIssuesDock;
class FlowTemplateDock;
class WorkspaceSidebar;
class FlowTemplatePreviewSession;
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
    struct UndoActions { QAction* undo; QAction* redo; };
    UndoActions createToolbar();
    UndoActions populateFlowMenu(QMenu* menu);
    void populateEquipmentMenu(QMenu* menu);
    void populateDisplayMenu(QMenu* menu, QWidget* controlParent);
    void setupCanvasShortcuts();

    QGraphicsScene* m_scene;
    CanvasView* m_view;
    QAction* m_themeAction{nullptr};
    QAction* m_annotationsAction{nullptr};
    FlowsheetDocument* m_document{nullptr};
    TerminalProductDock* m_terminalDock{nullptr};
    CalculationController* m_calculationController{nullptr};
    TerminalTableController* m_tableController{nullptr};
    AnnotationManager* m_annotationManager{nullptr};
    bool m_darkTheme{false};
    bool m_projectDirty{false};
    bool m_loadingProject{false};
    bool m_selectingGraphicsFromTable{false};
    int m_nextUnitId{1};
    QString m_projectPath;
    QComboBox* m_scenarioCombo{nullptr};
    QLabel* m_selectionStatusLabel{nullptr};
    OperationLogDock* m_operationLog{nullptr};
    ProjectUndoManager* m_undoManager{nullptr};
    FlowGroupClipboard m_flowClipboard;
    ReagentClipboard m_reagentClipboard;
    enum class CanvasClipboardKind { Empty, FlowGroup, Reagent };
    CanvasClipboardKind m_canvasClipboardKind{CanvasClipboardKind::Empty};
    QMenu* m_recentProjectsMenu{nullptr};
    ObjectPropertiesDock* m_propertiesDock{nullptr};
    CalculationIssuesDock* m_issuesDock{nullptr};
    FlowTemplateDock* m_templatesDock{nullptr};
    std::unique_ptr<FlowTemplatePreviewSession> m_templatePreview;
    WorkspaceSidebar* m_workspaceSidebar{nullptr};
    bool m_propertyRefreshPending{false};
    QString m_lastTemplateId;
    QPointF m_lastTemplateCenter;
    int m_templateInsertCount{0};

    void setupWorkspaceTools();
    void setupReagentClipboard();
    void copyReagentAnnotation(const QString& id);
    void pasteReagentAnnotation(const QString& streamId);
    void copyCanvasSelection();
    void pasteCanvasSelection();
    void schedulePropertyRefresh();
    void refreshCalculationIssues(bool reveal = false);
    void locateCalculationIssue(const QString& objectId, const QString& streamId,
                                const QString& componentId, bool grade, bool edit);
    void refreshFlowTemplates();
    void previewFlowTemplate(const QString& id);
    void saveFlowTemplate();
    void insertFlowTemplate(const QString& id);
    void synchronizePastedAnnotations(const FlowGroupClipboard::PasteResult& result);

    void addFlotationUnit();
    void addBallMill();
    void addThickener();
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
    void refreshScenarioUi();
    void addScenario(bool copyCurrent);
    void renameScenario();
    void deleteScenario();
    void compareScenarios();
    void setProjectDirty(bool dirty = true);
    void appendOperationLog(const QString& message);
};

} // namespace afs
