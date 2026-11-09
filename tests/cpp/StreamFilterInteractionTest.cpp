#include "adapters/CanvasTopologyBuilder.h"
#include "app/MainWindow.h"
#include "document/FlowsheetDocument.h"
#include "editor/CanvasView.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FeedJunctionItem.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"
#include "services/FlowsheetCalculationService.h"
#include "services/ProjectSerializer.h"
#include "ui/TerminalProductDock.h"
#include "ui/TerminalProductTableModel.h"

#include <QAction>
#include <QAbstractButton>
#include <QApplication>
#include <QComboBox>
#include <QDebug>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <stdexcept>

using namespace afs;

void runExcelExportInteractionChecks();

namespace {

void require(bool condition, const QString& message) {
    if (!condition) throw std::runtime_error(message.toStdString());
}

void flushEvents() { QApplication::processEvents(); }

void selectCombo(QComboBox* combo, const QString& text) {
    const int index = combo->findText(text);
    require(index >= 0, "Missing combo option: " + text);
    combo->setFocus();
    QTest::keyClick(combo, Qt::Key_Home);
    for (int step = 0; step < index; ++step) QTest::keyClick(combo, Qt::Key_Down);
    flushEvents();
    require(combo->currentText() == text, "Combo did not select: " + text);
}

void measure(FlowsheetDocument& document, const QString& id, double mass, double grade) {
    document.setDryMass(id, mass);
    document.setGradePercent(id, grade);
}

void saveFixture(FlowsheetScene& scene, FlowsheetDocument& document, const QString& path) {
    auto result = FlowsheetCalculationService::calculate(CanvasTopologyBuilder::build(scene), document);
    require(result.complete && result.fullySolved, "Fixture must have a complete balance");
    document.setCalculationResult(std::move(result));
    QString error;
    require(ProjectSerializer::save(scene, document, path, &error), error);
}

void createChain(const QString& path) {
    FlowsheetScene scene;
    auto* a = new FlotationUnitItem({"A", {-600, -150}});
    auto* b = new FlotationUnitItem({"B", {-100, 150}});
    auto* c = new FlotationUnitItem({"C", {400, 450}, 360, 150, UnitKind::ThreeProductFlotation});
    auto* d = new FlotationUnitItem({"D", {900, -150}});
    for (auto* unit : {a, b, c, d}) scene.addItem(unit);
    require(scene.connectProductDirect(a->products()[1], b->inputLine())
            && scene.connectProductDirect(b->products()[1], c->inputLine()), "Chain connections");
    FlowsheetDocument document;
    measure(document, "A:left", 10, 1);
    measure(document, "B:left", 20, 2);
    measure(document, "C:left", 30, 3);
    measure(document, "C:middle", 5, 4);
    measure(document, "C:right", 35, 5);
    measure(document, "D:left", 40, 6);
    measure(document, "D:right", 60, 1);
    saveFixture(scene, document, path);
}

void createLoop(const QString& path) {
    FlowsheetScene scene;
    auto* a = new FlotationUnitItem({"loop-a", {-300, 0}});
    auto* b = new FlotationUnitItem({"loop-b", {300, 350}});
    scene.addItem(a); scene.addItem(b);
    require(scene.connectProductDirect(a->products()[1], b->inputLine()), "Loop main connection");
    require(scene.connectRecycle(b->products()[1], a->inputLine(), "loop"), "Loop recycle");
    require(scene.mergeProducts(a->products()[0], b->products()[0], "pool"), "Terminal merge");
    FlowsheetDocument document;
    measure(document, "loop-a:left", 30, 1);
    measure(document, "loop-b:left", 70, 2);
    measure(document, "loop-b:right", 50, 3);
    saveFixture(scene, document, path);
}

struct WindowUi {
    MainWindow window;
    CanvasView* canvas{dynamic_cast<CanvasView*>(window.centralWidget())};
    FlowsheetScene* scene{qobject_cast<FlowsheetScene*>(canvas->scene())};
    FlowsheetDocument* document{window.findChild<FlowsheetDocument*>()};
    TerminalProductDock* dock{window.findChild<TerminalProductDock*>()};
    QTableView* table{dock->findChild<QTableView*>("terminalProductTable")};
    QComboBox* filter{dock->findChild<QComboBox*>("streamFilterCombo")};

    WindowUi() {
        window.resize(1600, 1100);
        window.show();
        flushEvents();
    }

    void load(const QString& path) {
        QTimer::singleShot(0, [] {
            if (auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
                dialog->button(QMessageBox::Discard)->click();
        });
        require(window.openProject(path), "Project did not load: " + path);
        flushEvents();
    }

    QSortFilterProxyModel* proxy() const {
        return qobject_cast<QSortFilterProxyModel*>(table->model());
    }

    QModelIndex indexFor(const QString& streamId, int column = 0) const {
        for (int row = 0; row < proxy()->rowCount(); ++row) {
            const auto source = proxy()->mapToSource(proxy()->index(row, column));
            if (dock->model()->streamAt(source.row())->streamId == streamId)
                return proxy()->index(row, column);
        }
        return {};
    }

    QString currentStream() const {
        const auto index = proxy()->mapToSource(table->currentIndex());
        const auto* stream = dock->model()->streamAt(index.row());
        return stream ? stream->streamId : QString{};
    }

    void expectStreams(QSet<QString> expected) const {
        QSet<QString> actual;
        for (int row = 0; row < proxy()->rowCount(); ++row) {
            const auto source = proxy()->mapToSource(proxy()->index(row, 0));
            actual.insert(dock->model()->streamAt(source.row())->streamId);
        }
        require(actual == expected, "Unexpected visible streams: "
            + QStringList(actual.begin(), actual.end()).join(", ")
            + "; expected: " + QStringList(expected.begin(), expected.end()).join(", "));
        const auto* summary = dock->findChild<QLabel*>("terminalProgress");
        require(summary->text().startsWith(QString("显示 %1/%2")
            .arg(expected.size()).arg(dock->model()->rowCount())), "Stale filter summary");
    }

    void clickUnit(const QString& id, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        FlotationUnitItem* unit = nullptr;
        for (auto* item : scene->items())
            if (auto* candidate = dynamic_cast<FlotationUnitItem*>(item);
                candidate && candidate->unit().id == id) unit = candidate;
        require(unit, "Missing unit: " + id);
        const bool wasSelected = unit->isSelected();
        canvas->centerOn(unit);
        flushEvents();
        QTest::mouseClick(canvas->viewport(), Qt::LeftButton, modifiers,
                          canvas->mapFromScene(unit->mapToScene(QPointF(60, 12))));
        flushEvents();
        require(unit->isSelected() == (modifiers.testFlag(Qt::ControlModifier) ? !wasSelected : true),
                "Canvas unit click was not delivered");
    }

    void clearCanvasSelection() {
        const QPointF empty(-1600, -1200);
        canvas->centerOn(empty); flushEvents();
        QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::NoModifier,
                          canvas->mapFromScene(empty));
        flushEvents();
        require(scene->selectedItems().isEmpty(), "Blank canvas click did not clear selection");
    }

    void clickStream(const QString& id, int column = TerminalProductTableModel::MassColumn) {
        const auto index = indexFor(id, column);
        require(index.isValid(), "Missing visible row: " + id);
        table->scrollTo(index); flushEvents();
        QTest::mouseClick(table->viewport(), Qt::LeftButton, Qt::NoModifier,
                          table->visualRect(index).center());
        flushEvents();
        require(currentStream() == id, "Table click changed the selected stream: " + id);
        require(!table->selectionModel()->selectedRows().isEmpty(), "Table navigation cleared row selection");
    }

    void clickProduct(const QString& id) {
        ProductLineItem* product = nullptr;
        for (auto* item : scene->items())
            if (auto* candidate = dynamic_cast<ProductLineItem*>(item);
                candidate && candidate->streamId() == id) product = candidate;
        require(product, "Missing product: " + id);
        const QPointF point = product->mapToScene(product->path().pointAtPercent(0.35));
        canvas->centerOn(point); flushEvents();
        QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::NoModifier,
                          canvas->mapFromScene(point));
        flushEvents();
        require(product->isSelected(), "Canvas product click was not delivered");
    }


};

void testSelectionAndNavigation(WindowUi& ui) {
    qInfo() << "Canvas selection, table navigation and multiple selection";
    const QByteArray before = ProjectSerializer::serialize(*ui.scene, *ui.document);
    QSignalSpy calculations(ui.document, &FlowsheetDocument::calculationChanged);
    ui.expectStreams({"A:feed", "D:feed", "A:left", "A:right", "B:left", "B:right",
                      "C:left", "C:middle", "C:right", "D:left", "D:right"});
    selectCombo(ui.filter, "选中流程关联");
    ui.expectStreams({});
    ui.clickUnit("A");
    ui.expectStreams({"A:feed", "A:left", "A:right"});
    ui.clickUnit("B");
    ui.expectStreams({"A:right", "B:left", "B:right"});
    ui.clickStream("A:right");
    ui.expectStreams({"A:right", "B:left", "B:right"});
    require(ui.scene->selectedItems().size() == 1
        && dynamic_cast<ProductLineItem*>(ui.scene->selectedItems().first()), "Table did not select product");
    require(ui.dock->selectedStreamId() == "A:right",
            "Table navigation did not retain the selected stream");
    ui.clickStream("B:left", ui.dock->model()->gradeColumn(DefaultComponentId));
    ui.expectStreams({"A:right", "B:left", "B:right"});
    require(ui.table->currentIndex().column() == ui.dock->model()->gradeColumn(DefaultComponentId),
            "Canvas synchronization changed the editing column");
    ui.clickUnit("C");
    ui.expectStreams({"B:right", "C:left", "C:middle", "C:right"});
    ui.clickUnit("B", Qt::ControlModifier);
    ui.expectStreams({"A:right", "B:left", "B:right", "C:left", "C:middle", "C:right"});
    ui.clickUnit("B", Qt::ControlModifier);
    ui.expectStreams({"B:right", "C:left", "C:middle", "C:right"});
    ui.clickUnit("D");
    ui.expectStreams({"D:feed", "D:left", "D:right"});
    ui.clearCanvasSelection();
    ui.expectStreams({});
    selectCombo(ui.filter, "全部物流");
    ui.clickProduct("C:middle");
    require(ui.currentStream() == "C:middle"
        && !ui.table->selectionModel()->selectedRows().isEmpty(), "Canvas did not select the matching table row");
    selectCombo(ui.filter, "选中流程关联"); ui.expectStreams({});
    require(calculations.isEmpty(), "Display filtering invalidated calculation results");
    require(ProjectSerializer::serialize(*ui.scene, *ui.document) == before,
            "Display filtering changed persistent project data");
}

void testCategoriesAndEditing(WindowUi& ui) {
    qInfo() << "Category filters, selected-unit scope and edits through the proxy";
    selectCombo(ui.filter, "浮选入料");
    ui.expectStreams({"A:feed", "A:right", "B:right", "D:feed"});
    selectCombo(ui.filter, "回流"); ui.expectStreams({});
    selectCombo(ui.filter, "已填写");
    ui.expectStreams({"A:left", "B:left", "C:left", "C:middle", "C:right", "D:left", "D:right"});
    selectCombo(ui.filter, "待补充"); ui.expectStreams({});
    selectCombo(ui.filter, "终端产品");
    ui.expectStreams({"A:left", "B:left", "C:left", "C:middle", "C:right", "D:left", "D:right"});
    selectCombo(ui.filter, "选中流程关联");
    ui.clickUnit("A"); ui.expectStreams({"A:feed", "A:left", "A:right"});
    selectCombo(ui.filter, "全部物流");
    ui.clickStream("A:feed");
    QTest::keyClick(ui.table, Qt::Key_F2); flushEvents();
    auto* editor = ui.table->findChild<QLineEdit*>();
    require(editor && editor->isVisible(), "Numeric cell editor did not open");
    QTest::keyClick(editor, Qt::Key_A, Qt::ControlModifier);
    QTest::keyClicks(editor, "100"); QTest::keyClick(editor, Qt::Key_Return); flushEvents();
    require(ui.document->measurement("A:feed").dryMass == 100.0
        && !ui.document->measurement("D:feed").dryMass, "Proxy edit wrote the wrong stream");
    selectCombo(ui.filter, "待补充");
    ui.expectStreams({"A:feed", "A:right", "B:right", "D:feed"});
    selectCombo(ui.filter, "已填写");
    ui.expectStreams({"A:feed", "A:left", "B:left", "C:left", "C:middle", "C:right", "D:left", "D:right"});
    ui.clickStream("A:feed");
    QTest::keyClick(ui.table, Qt::Key_Delete); flushEvents();
    require(!ui.document->measurement("A:feed").dryMass, "Delete did not clear proxy cell");
    ui.expectStreams({"A:left", "B:left", "C:left", "C:middle", "C:right", "D:left", "D:right"});
    selectCombo(ui.filter, "选中流程关联");
    ui.clickUnit("B"); ui.expectStreams({"A:right", "B:left", "B:right"});
    auto* calculate = ui.dock->findChild<QPushButton*>("calculateButton");
    QTest::mouseClick(calculate, Qt::LeftButton); flushEvents();
    require(ui.document->calculationResult() && ui.document->calculationResult()->fullySolved,
            "Calculation used the display-filtered graph");
    require(ui.document->calculationResult()->values.contains("D:feed"), "Hidden circuit was not calculated");
    selectCombo(ui.filter, "待补充"); ui.expectStreams({});
}

void testLoopAndProjectSwitch(WindowUi& ui, const QString& loopPath, const QString& chainPath) {
    qInfo() << "Recycle and product merge filters, shared graphics rows and project replacement";
    ui.load(loopPath);
    selectCombo(ui.filter, "选中流程关联");
    ui.clickUnit("loop-a");
    ui.expectStreams({"loop:external-feed", "loop:output", "loop-a:left", "loop-a:right"});
    ui.clickStream("loop:output");
    ui.expectStreams({"loop:external-feed", "loop:output", "loop-a:left", "loop-a:right"});
    require(ui.dock->selectedStreamId() == "loop:output",
            "Shared junction item selected the wrong stream row");
    ui.clickStream("loop:external-feed");
    require(ui.dock->selectedStreamId() == "loop:external-feed",
            "Fresh feed row lost its selected stream");
    selectCombo(ui.filter, "回流"); ui.expectStreams({"loop-b:right"});
    selectCombo(ui.filter, "浮选入料");
    ui.expectStreams({"loop:external-feed", "loop:output", "loop-a:right"});
    selectCombo(ui.filter, "回流"); ui.expectStreams({"loop-b:right"});
    selectCombo(ui.filter, "全部物流");
    ui.expectStreams({"loop:external-feed", "loop:output", "loop-a:left", "loop-a:right", "loop-b:left", "loop-b:right", "pool:output"});
    selectCombo(ui.filter, "终端产品"); ui.expectStreams({"pool:output"});
    selectCombo(ui.filter, "选中流程关联"); ui.clickUnit("loop-a");
    ui.expectStreams({"loop:external-feed", "loop:output", "loop-a:left", "loop-a:right"});
    ui.load(chainPath);
    ui.expectStreams({});
    ui.clickUnit("C"); ui.expectStreams({"B:right", "C:left", "C:middle", "C:right"});
}

void testScenarioSwitch(WindowUi& ui) {
    qInfo() << "Scenario replacement and live measurement-status filtering";
    require(ui.document->addScenario("空白方案", false) == 1, "Could not add empty scenario");
    auto* scenarios = ui.window.findChild<QComboBox*>("scenarioCombo");
    require(scenarios, "Missing scenario selector");
    ui.expectStreams({"B:right", "C:left", "C:middle", "C:right"});
    selectCombo(scenarios, "空白方案");
    ui.expectStreams({});
    selectCombo(ui.filter, "已填写"); ui.expectStreams({});
    selectCombo(ui.filter, "待补充");
    ui.expectStreams({"A:feed", "D:feed", "A:left", "A:right", "B:left", "B:right",
                      "C:left", "C:middle", "C:right", "D:left", "D:right"});
    selectCombo(scenarios, "基准方案");
    ui.expectStreams({});
    require(ui.document->calculationResult() && ui.document->calculationResult()->fullySolved,
            "Scenario switch lost stored calculation results");
}

void testTopologyChange(WindowUi& ui) {
    qInfo() << "Deleting and undoing a focused unit rebuilds filter state";
    selectCombo(ui.filter, "选中流程关联");
    ui.clickUnit("D");
    ui.expectStreams({"D:feed", "D:left", "D:right"});
    QTimer::singleShot(0, [] {
        if (auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
            dialog->button(QMessageBox::Yes)->click();
    });
    QTest::keyClick(ui.canvas->viewport(), Qt::Key_Delete); flushEvents();
    ui.expectStreams({});
    selectCombo(ui.filter, "全部物流");
    ui.expectStreams({"A:feed", "A:left", "A:right", "B:left", "B:right", "C:left", "C:middle", "C:right"});
    QAction* undo = nullptr;
    for (auto* action : ui.window.findChildren<QAction*>())
        if (action->text() == "撤销") undo = action;
    require(undo, "Missing undo action");
    require(QTest::qWaitFor([undo] { return undo->isEnabled(); }, 1000), "Undo checkpoint was not committed");
    auto* menu = qobject_cast<QMenu*>(undo->parent());
    require(menu, "Missing flow edit menu");
    menu->popup(ui.canvas->mapToGlobal(QPoint(10, 10))); flushEvents();
    QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier, menu->actionGeometry(undo).center());
    flushEvents();
    ui.expectStreams({"A:feed", "D:feed", "A:left", "A:right", "B:left", "B:right",
                      "C:left", "C:middle", "C:right", "D:left", "D:right"});
    selectCombo(ui.filter, "选中流程关联");
    ui.clickUnit("D"); ui.expectStreams({"D:feed", "D:left", "D:right"});
}

} // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("AutoFlotationSheetTests");
    QCoreApplication::setApplicationName("StreamFilterInteraction");
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
    try {
        const QString chainPath = directory.filePath("chain.afs.json");
        const QString loopPath = directory.filePath("loop.afs.json");
        createChain(chainPath); createLoop(loopPath);
        WindowUi ui;
        ui.load(chainPath);
        testSelectionAndNavigation(ui);
        testCategoriesAndEditing(ui);
        testLoopAndProjectSwitch(ui, loopPath, chainPath);
        testScenarioSwitch(ui);
        testTopologyChange(ui);
        runExcelExportInteractionChecks();
        qInfo() << "All stream filter interaction checks passed";
    } catch (const std::exception& error) {
        qCritical().noquote() << error.what();
        return 1;
    }
    return 0;
}
