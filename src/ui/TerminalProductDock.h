#pragma once

#include "document/StreamDescriptor.h"
#include "ui/CalculationPanelState.h"

#include <QDockWidget>
#include <QSet>
#include <QVector>

class QTableView;
class QLabel;
class QWidget;
class QPushButton;
class QComboBox;

namespace afs {

class TerminalTableSession;
class TerminalProductTableModel;
class StreamFilterProxyModel;

class TerminalProductDock final : public QDockWidget {
    Q_OBJECT
public:
    TerminalProductDock(TerminalTableSession& session, QWidget* parent = nullptr);
    void setCalculationState(const CalculationPanelState& state);
    void setSelectedUnitIds(QSet<QString> unitIds);
    void selectStreams(const QStringList& streamIds);
    [[nodiscard]] QString selectedStreamId() const;
    void refreshAppearance();
    void setScenarioName(const QString& name);
    bool focusMeasurement(const QString& streamId, const QString& componentId, bool grade, bool edit);
    [[nodiscard]] TerminalProductTableModel* model() const { return m_model; }

signals:
    void streamRequested(const QString& streamId);
    void calculationExplanationRequested();
    void calculationRequested();

private:
    QTableView* m_table;
    TerminalProductTableModel* m_model;
    StreamFilterProxyModel* m_filterModel;
    QComboBox* m_filterCombo;
    QComboBox* m_modeCombo;
    QLabel* m_progressLabel;
    QWidget* m_panel;
    TerminalTableSession& m_session;
    QPushButton* m_calculateButton;
    QPushButton* m_explanationButton;
    QLabel* m_calculationStatus;
    QSet<QString> m_selectedUnitIds;
    QVector<int> m_defaultColumnWidths;
    bool m_columnsManuallyResized{false};
    bool m_applyingColumnWidths{false};
    bool m_columnFitPending{false};

    void setupPanel();
    void updateSummary();
    void applyPanelStyle();
    void configureColumns();
    void editComponents();
    void scheduleColumnFit();
    void fitColumnsToViewport();
    bool eventFilter(QObject* watched, QEvent* event) override;
};

} // namespace afs
