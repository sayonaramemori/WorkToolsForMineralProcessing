#include "WorkspaceToolsTestSupport.h"
#include "services/ProjectSerializer.h"
#include "ui/TerminalProductTableModel.h"
#include <QDialog>
#include <QTextBrowser>
#include <QTimer>
#include <QHeaderView>
#include <QPushButton>
#include <QSignalSpy>
#include <QTableView>
#include <cmath>

using namespace afs;
using namespace afs::tests;

void runTerminalTableLayoutChecks() {
    ToolsUi ui;
    auto* table = ui.window.findChild<QTableView*>("terminalProductTable");
    auto* header = table->horizontalHeader();
    auto* sidebar = ui.window.findChild<WorkspaceSidebar*>();
    require(!ui.window.findChild<QPushButton*>("interestObjectButton"), "Interest selector was not removed");
    const auto checkFilled = [&] {
        require(!ui.window.findChild<QWidget*>("resultDetails")
            && !ui.window.findChild<QWidget*>("resultDetailTable"), "Removed result table was recreated");
        auto* panel = ui.window.findChild<QWidget*>("terminalProductPanel");
        require(std::abs(panel->contentsRect().bottom() - table->geometry().bottom() - 14) <= 2,
                "Main table does not fill the space released by the result table");
        require(std::abs(header->length() - table->viewport()->width()) <= 1,
            QString("Table does not fill its viewport: columns %1, available %2").arg(header->length()).arg(table->viewport()->width()));
        for (int column = 0; column < header->count(); ++column)
            require(header->sectionResizeMode(column) == QHeaderView::Interactive, "A filled column cannot be resized manually");
    };
    checkFilled();
    const auto before = ProjectSerializer::serialize(*ui.scene, *ui.document);
    QSignalSpy projectChanges(ui.document, &FlowsheetDocument::projectChanged);
    QSignalSpy calculations(ui.document, &FlowsheetDocument::calculationChanged);
    bool explanationShown = false;
    QTimer::singleShot(0, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (!dialog) return;
        auto* text = dialog->findChild<QTextBrowser*>("calculationExplanationText");
        explanationShown = dialog->objectName() == "calculationExplanationDialog" && text
            && text->toPlainText().contains("严格模式") && text->toPlainText().contains("自由度");
        dialog->reject();
    });
    auto* explanation = ui.window.findChild<QPushButton*>("calculationExplanationButton");
    require(explanation && explanation->isVisible(), "Calculation explanation button is missing");
    QTest::mouseClick(explanation, Qt::LeftButton); flush();
    require(explanationShown, "Calculation explanation did not open a separate read-only dialog");
    checkFilled();
    for (const auto size : {QSize(1440, 900), QSize(1800, 1100)}) {
        ui.window.resize(size); flush(); checkFilled();
    }
    // Use the actual header grip to verify that automatic fitting yields to
    // the user's widths rather than stretching the last column permanently.
    const int initial = header->sectionSize(0);
    const QPoint grip(header->sectionViewportPosition(0) + initial - 1, header->viewport()->height() / 2);
    QTest::mouseMove(header->viewport(), grip);
    QTest::mousePress(header->viewport(), Qt::LeftButton, Qt::NoModifier, grip);
    QTest::mouseMove(header->viewport(), grip + QPoint(45, 0));
    QTest::mouseRelease(header->viewport(), Qt::LeftButton, Qt::NoModifier, grip + QPoint(45, 0)); flush();
    require(header->sectionSize(0) > initial + 20, "Column grip did not resize the column");
    QVector<int> manual;
    for (int column = 0; column < header->count(); ++column) manual.append(header->sectionSize(column));
    ui.window.resize(1600, 1000); flush();
    sidebar->showPanel("properties"); flush(); sidebar->showPanel("table"); flush();
    for (int column = 0; column < header->count(); ++column)
        require(header->sectionSize(column) == manual[column], "Resize or panel switching overwrote manual column widths");
    require(ProjectSerializer::serialize(*ui.scene, *ui.document) == before && projectChanges.isEmpty() && calculations.isEmpty(),
            "Table fitting or resizing modified project data or calculation");

    ui.document->setComponents({{"component-1", "Cu"}, {"component-2", "Zn"}, {"component-3", "Pb"}}); flush();
    ui.window.resizeDocks({sidebar}, {1150}, Qt::Horizontal); flush(); checkFilled();
    ui.document->setCalculationMode(CalculationMode::DataReconciliation); flush(); checkFilled();
    ui.window.resizeDocks({sidebar}, {700}, Qt::Horizontal); flush();
    require(header->length() > table->viewport()->width(), "Many component columns were squeezed until unreadable");
    for (int column = 0; column < header->count(); ++column)
        require(header->sectionSize(column) >= 72, "A component or standard deviation column became unreadable");

    ToolsUi startup;
    preview(startup.window, "table-filled-startup");
}
