#include "app/MainWindow.h"
#include "adapters/CanvasTopologyBuilder.h"
#include "document/FlowsheetDocument.h"
#include "editor/CanvasView.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FlotationUnitItem.h"
#include "services/ExcelDataExporter.h"
#include "services/FlowsheetCalculationService.h"
#include "ui/TerminalProductDock.h"
#include "ui/TerminalProductTableModel.h"

#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QTest>
#include <QTimer>
#include <stdexcept>

using namespace afs;

namespace {
void require(bool condition, const QString& message) {
    if (!condition) throw std::runtime_error(message.toStdString());
}

QAction* action(MainWindow& window, const QString& text) {
    for (auto* candidate : window.findChildren<QAction*>())
        if (candidate->text() == text) return candidate;
    throw std::runtime_error("Missing export action: " + text.toStdString());
}
} // namespace

void runExcelExportInteractionChecks() {
    qInfo() << "Table naming controls Excel eligibility and export-order dialog contains only user names";
    MainWindow window;
    window.resize(1600, 1100); window.show(); window.activateWindow();
    QApplication::processEvents();
    auto* canvas = dynamic_cast<CanvasView*>(window.centralWidget());
    auto* scene = qobject_cast<FlowsheetScene*>(canvas->scene());
    auto* document = window.findChild<FlowsheetDocument*>();
    auto* dock = window.findChild<TerminalProductDock*>();
    auto* table = dock->findChild<QTableView*>("terminalProductTable");
    auto* proxy = qobject_cast<QSortFilterProxyModel*>(table->model());
    QString unitId;
    for (auto* item : scene->items())
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item)) unitId = unit->unit().id;
    require(!unitId.isEmpty(), "Missing fixture unit");
    document->setDryMass(unitId + ":left", 30);
    document->setGradePercent(unitId + ":left", 2);
    document->setDryMass(unitId + ":right", 70);
    document->setGradePercent(unitId + ":right", 1);
    document->setCalculationResult(FlowsheetCalculationService::calculate(*scene, *document));
    const auto snapshot = CanvasTopologyBuilder::build(*scene);
    require(ExcelDataExporter::namedStreams(*document, snapshot).isEmpty(), "Unnamed products were eligible");
    for (int row = 0; row < dock->model()->rowCount(); ++row) {
        require(dock->model()->index(row, TerminalProductTableModel::ProductNameColumn).data().toString().isEmpty()
            && !dock->model()->index(row, TerminalProductTableModel::NameColumn).data().toString().isEmpty(),
            "Product name must be blank while stream identification remains visible");
    }
    const auto editName = [&](const QString& id, const QString& name) {
        QModelIndex index;
        for (int row = 0; row < proxy->rowCount(); ++row) {
            const auto candidate = proxy->index(row, TerminalProductTableModel::ProductNameColumn);
            if (dock->model()->streamAt(proxy->mapToSource(candidate).row())->streamId == id) index = candidate;
        }
        require(index.isValid(), "Missing product-name cell");
        table->scrollTo(index); table->setCurrentIndex(index); table->setFocus();
        QTest::keyClick(table, Qt::Key_F2);
        QApplication::processEvents();
        QLineEdit* editor = nullptr;
        for (auto* field : table->findChildren<QLineEdit*>()) if (field->isVisible()) editor = field;
        require(editor, "Product-name editor did not open");
        editor->setText(name); QTest::keyClick(editor, Qt::Key_Return);
        QApplication::processEvents();
        require(document->productName(id) == name.simplified(), "Product-name edit did not commit");
    };
    QSignalSpy calculations(document, &FlowsheetDocument::calculationChanged);
    editName(unitId + ":left", "精矿");
    editName(unitId + ":feed", "总入料");
    document->setExportStreamOrder({unitId + ":left", unitId + ":right", unitId + ":feed"});
    bool correctOrder = false, correctReset = false;
    QTimer::singleShot(0, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (!dialog) return;
        auto* list = dialog->findChild<QListWidget*>();
        auto* buttons = dialog->findChild<QDialogButtonBox*>();
        if (!list || !buttons) { dialog->reject(); return; }
        correctOrder = list->count() == 2 && list->item(0)->text() == "精矿" && list->item(1)->text() == "总入料";
        for (auto* button : buttons->buttons())
            if (buttons->buttonRole(button) == QDialogButtonBox::ResetRole) button->click();
        correctReset = list->count() == 2 && list->item(0)->text() == "总入料" && list->item(1)->text() == "精矿";
        dialog->accept();
    });
    action(window, "设置 Excel 行顺序")->trigger();
    require(correctOrder && correctReset && document->exportStreamOrder() == QStringList{unitId + ":feed", unitId + ":left"},
            "Export-order dialog included unnamed streams, default names or wrong reset order");
    editName(unitId + ":left", {});
    require(ExcelDataExporter::namedStreams(*document, snapshot).size() == 1,
            "Clearing a name did not remove export eligibility");
    editName(unitId + ":feed", " \t ");
    require(ExcelDataExporter::namedStreams(*document, snapshot).isEmpty() && calculations.isEmpty(),
            "Clearing names used a fallback or invalidated calculation");
    bool guidanceShown = false;
    QTimer::singleShot(0, [&] {
        if (auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
            guidanceShown = message->text().contains("产品名称"); message->accept();
        } else if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) dialog->reject();
    });
    action(window, "导出 Excel")->trigger();
    require(guidanceShown, "Empty-name export opened a save dialog instead of explaining naming requirement");
}
