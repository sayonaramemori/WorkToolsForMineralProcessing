#pragma once

#include "adapters/CanvasTopologyBuilder.h"

#include <QDockWidget>
#include <QSet>
#include <QVector>

class QTableView;
class QGraphicsItem;
class QLabel;
class QWidget;
class QPushButton;
class QComboBox;

namespace afs {

class FlowsheetDocument;
class TerminalProductTableModel;
class StreamFilterProxyModel;

class TerminalProductDock final : public QDockWidget {
    Q_OBJECT
public:
    TerminalProductDock(FlowsheetDocument& document, QWidget* parent = nullptr);
    void setSnapshot(CanvasTopologySnapshot snapshot);
    void setSelectedUnitIds(QSet<QString> unitIds);
    void selectGraphicsItem(const QGraphicsItem* item);
    [[nodiscard]] QString selectedStreamIdForGraphicsItem(const QGraphicsItem* item) const;
    void refreshAppearance();
    void setScenarioName(const QString& name);
    bool focusMeasurement(const QString& streamId, const QString& componentId, bool grade, bool edit);
    [[nodiscard]] TerminalProductTableModel* model() const { return m_model; }

signals:
    void graphicsItemRequested(QGraphicsItem* item);
    void calculationRequested();

private:
    QTableView* m_table;
    TerminalProductTableModel* m_model;
    StreamFilterProxyModel* m_filterModel;
    QComboBox* m_filterCombo;
    QComboBox* m_modeCombo;
    QLabel* m_progressLabel;
    QWidget* m_panel;
    FlowsheetDocument& m_document;
    QPushButton* m_calculateButton;
    QPushButton* m_explanationButton;
    QLabel* m_calculationStatus;
    CanvasTopologySnapshot m_snapshot;
    QSet<QString> m_selectedUnitIds;
    QVector<int> m_defaultColumnWidths;
    bool m_columnsManuallyResized{false};
    bool m_applyingColumnWidths{false};
    bool m_columnFitPending{false};

    void setupPanel();
    void updateSummary();
    void applyPanelStyle();
    void updateCalculationState();
    void showCalculationExplanation();
    void configureColumns();
    void editComponents();
    void scheduleColumnFit();
    void fitColumnsToViewport();
    bool eventFilter(QObject* watched, QEvent* event) override;
};

} // namespace afs
