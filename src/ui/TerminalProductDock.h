#pragma once

#include <QDockWidget>

class QTableView;
class QGraphicsItem;
class QLabel;
class QWidget;
class QPushButton;

namespace afs {

class FlowsheetDocument;
class TerminalProductTableModel;
class ResultDetailsView;
struct CanvasTopologySnapshot;
namespace topology { class TopologyGraph; }

class TerminalProductDock final : public QDockWidget {
    Q_OBJECT
public:
    TerminalProductDock(FlowsheetDocument& document, QWidget* parent = nullptr);
    void setSnapshot(CanvasTopologySnapshot snapshot);
    void selectGraphicsItem(const QGraphicsItem* item);
    void refreshAppearance();
    void showStreamResult(const QString& streamId);
    void showUnitResult(const QString& unitId, const topology::TopologyGraph& graph);
    void clearResultDetails();
    void setScenarioName(const QString& name);
    [[nodiscard]] TerminalProductTableModel* model() const { return m_model; }

signals:
    void graphicsItemRequested(QGraphicsItem* item);
    void calculationRequested();

private:
    QTableView* m_table;
    TerminalProductTableModel* m_model;
    QLabel* m_progressLabel;
    QWidget* m_panel;
    FlowsheetDocument& m_document;
    QPushButton* m_calculateButton;
    QLabel* m_calculationStatus;
    ResultDetailsView* m_resultDetails;

    void updateSummary();
    void applyPanelStyle();
    void updateCalculationState();
    void configureColumns();
    void editComponents();
};

} // namespace afs
