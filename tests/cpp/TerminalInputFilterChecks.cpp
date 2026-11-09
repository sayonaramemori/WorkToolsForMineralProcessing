#include "TerminalInputChecks.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/InputLineItem.h"
#include "services/FlowsheetCalculationService.h"
#include "ui/TerminalProductDock.h"
#include "ui/TerminalProductTableModel.h"
#include <QApplication>
#include <QComboBox>
#include <QKeyEvent>
#include <QTableView>

namespace afs::tests {
int runTerminalFilterChecks(TerminalInputContext& context) {
    auto& calculationScene = context.calculationScene;
    auto* calculationUnit = context.calculationUnit;
    const auto& calculationSnapshot = context.calculationSnapshot;
    auto& document = context.inputDocument;
    const auto& connected = context.connected;
    FlowsheetDocument filterDocument;
    filterDocument.setDryMass("calculation:feed", 100.0);
    filterDocument.setGradePercent("calculation:feed", 2.0);
    const auto insufficient = FlowsheetCalculationService::calculate(
        calculationScene, filterDocument);
    if (insufficient.dryMassDegreesOfFreedom != 1
        || insufficient.componentMassDegreesOfFreedom != 1) return 54;
    TerminalProductDock filterDock(filterDocument);
    filterDock.setSnapshot(calculationSnapshot);
    auto* filterCombo = filterDock.findChild<QComboBox*>("streamFilterCombo");
    auto* filterTable = filterDock.findChild<QTableView*>("terminalProductTable");
    if (!filterCombo || !filterTable || filterTable->model()->rowCount() != 3) return 55;
    filterCombo->setCurrentText("已填写");
    if (filterTable->model()->rowCount() != 1) return 56;
    filterCombo->setCurrentText("终端产品");
    if (filterTable->model()->rowCount() != 2) return 57;
    filterCombo->setCurrentText("浮选入料");
    if (filterTable->model()->rowCount() != 1) return 58;
    // The selected-unit filter is a one-hop canvas focus: it retains the
    // selected unit's input/output streams and directly attached neighbours,
    // but does not expand to the full connected circuit.
    TerminalProductDock connectionFilterDock(document);
    connectionFilterDock.setSnapshot(connected);
    auto* connectionFilterCombo = connectionFilterDock.findChild<QComboBox*>("streamFilterCombo");
    auto* connectionFilterTable = connectionFilterDock.findChild<QTableView*>("terminalProductTable");
    if (!connectionFilterCombo || !connectionFilterTable) return 82;
    connectionFilterCombo->setCurrentText("选中流程关联");
    connectionFilterDock.setSelectedUnitIds({"first"});
    if (connectionFilterTable->model()->rowCount() != 3) return 83;
    connectionFilterDock.setSelectedUnitIds({"second"});
    if (connectionFilterTable->model()->rowCount() != 3) return 84;
    filterTable->setCurrentIndex(filterTable->model()->index(
        0, TerminalProductTableModel::MassColumn));
    QKeyEvent deleteMeasurement(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
    QApplication::sendEvent(filterTable, &deleteMeasurement);
    if (filterDocument.measurement("calculation:feed").dryMass) return 67;
    filterTable->setCurrentIndex(filterTable->model()->index(
        0, TerminalProductTableModel::GradeColumn));
    filterDock.selectGraphicsItem(calculationUnit->inputLine());
    if (filterTable->currentIndex().column() != TerminalProductTableModel::GradeColumn)
        return 68;
    QKeyEvent deleteGrade(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
    QApplication::sendEvent(filterTable, &deleteGrade);
    if (filterDocument.measurement("calculation:feed").grade(DefaultComponentId)) return 69;

    TerminalProductDock unfilteredDock(filterDocument);
    unfilteredDock.setSnapshot(connected);
    auto* allTable = unfilteredDock.findChild<QTableView*>("terminalProductTable");
    auto* mode = unfilteredDock.findChild<QComboBox*>("streamFilterCombo");
    if (!allTable || !mode
        || allTable->model()->rowCount() != 5) return 61;
    mode->setCurrentText("选中流程关联");
    unfilteredDock.setSelectedUnitIds({"first"});
    if (allTable->model()->rowCount() != 3) return 62;
    if (!unfilteredDock.focusMeasurement("second:right", DefaultComponentId, true, false)
        || allTable->model()->rowCount() != 5 || !allTable->currentIndex().isValid()
        || allTable->currentIndex().column() != unfilteredDock.model()->gradeColumn(DefaultComponentId)) return 63;

    return 0;
}
}
