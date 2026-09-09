#pragma once

#include <QMainWindow>
#include <QString>

class QAction;
class QGraphicsScene;
class QComboBox;
class QCloseEvent;
class QLabel;

namespace afs {

class CanvasView;
class FlowsheetDocument;
class TerminalProductDock;
class AnnotationManager;
class OperationLogDock;
class ProjectUndoManager;

class MainWindow final : public QMainWindow {
public:
    MainWindow();

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

    void addFlotationUnit();
    void addThreeProductUnit();
    void addBinarySplitter();
    void newProject();
    bool deleteSelectedUnits();
    bool confirmSaveBeforeDestructiveAction();
    bool saveProject();
    bool saveProjectAs();
    bool saveProjectTo(const QString& path);
    void importProject();
    void updateNextUnitId();
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
