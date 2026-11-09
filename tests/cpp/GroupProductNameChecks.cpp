#include "app/MainWindow.h"
#include "document/FlowsheetDocument.h"
#include "editor/CanvasView.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"
#include "services/FlowGroupSnapshotOperations.h"
#include "services/FlowTemplateStore.h"
#include "services/ProjectSerializer.h"
#include "services/ProjectUndoManager.h"
#include "ui/TerminalProductDock.h"
#include "ui/TerminalProductTableModel.h"
#include <QApplication>
#include <QGraphicsSimpleTextItem>
#include <QTest>
#include <stdexcept>

using namespace afs;
namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
MergeJunctionItem* mergeByStream(FlowsheetScene& scene, const QString& streamId) {
    for (auto* item : scene.items())
        if (auto* merge = dynamic_cast<MergeJunctionItem*>(item); merge && merge->outputStreamId() == streamId)
            return merge;
    return nullptr;
}
bool visibleName(MergeJunctionItem* merge, const QString& name) {
    if (merge) for (auto* child : merge->childItems())
        if (auto* text = dynamic_cast<QGraphicsSimpleTextItem*>(child); text && text->isVisible() && text->text() == name)
            return true;
    return false;
}
}
void runGroupProductNameChecks(const QString& directory) {
    MainWindow window; window.resize(1600, 1000); window.show(); window.activateWindow();
    QApplication::processEvents();
    auto* scene = window.findChild<FlowsheetScene*>();
    auto* document = window.findChild<FlowsheetDocument*>();
    auto* dock = window.findChild<TerminalProductDock*>();
    auto* undo = window.findChild<ProjectUndoManager*>();
    auto* canvas = dynamic_cast<CanvasView*>(window.centralWidget());
    FlotationUnitItem* first = nullptr;
    for (auto* item : scene->items()) if (auto* unit = dynamic_cast<FlotationUnitItem*>(item)) first = unit;
    require(first, "Initial unit is missing");
    auto* second = new FlotationUnitItem({"name-branch", {500, 200}}); scene->addItem(second);
    auto* original = scene->mergeProducts(first->products().front(), second->products().front(), "named-merge");
    require(original, "Named product merge could not be created");
    const QString sourceStream = original->outputStreamId();
    const int row = dock->model()->rowForStreamId(sourceStream);
    require(row >= 0 && dock->model()->setData(dock->model()->index(row, TerminalProductTableModel::ProductNameColumn),
        "合流精矿", Qt::EditRole), "Merged output could not be named in the table");
    require(visibleName(original, "合流精矿"), "Original merge name was not drawn");
    auto* third = new FlotationUnitItem({"extra-branch", {900, 250}}); scene->addItem(third);
    require(scene->addProductToMerge(third->products().front(), original)
        && original->outputStreamId() == sourceStream && visibleName(original, "合流精矿"),
        "Extending a named merge changed its stream ID or name");
    const QString firstId = first->unit().id;
    scene->clearSelection(); first->setSelected(true); second->setSelected(true); third->setSelected(true);
    canvas->setFocus(); QTest::keyClick(canvas->viewport(), Qt::Key_C, Qt::ControlModifier);
    QApplication::processEvents();
    document->setProductName(sourceStream, "复制后的源名称");
    undo->initialize();
    canvas->setFocus(); QTest::keyClick(canvas->viewport(), Qt::Key_V, Qt::ControlModifier);
    QApplication::processEvents();
    QString cloneStream;
    for (auto* item : scene->items())
        if (auto* merge = dynamic_cast<MergeJunctionItem*>(item); merge && merge->outputStreamId() != sourceStream)
            cloneStream = merge->outputStreamId();
    require(!cloneStream.isEmpty() && mergeByStream(*scene, cloneStream)->products().size() == 3
        && document->productName(cloneStream) == "合流精矿"
        && document->productName(sourceStream) == "复制后的源名称"
        && visibleName(mergeByStream(*scene, cloneStream), "合流精矿"),
        "Ctrl+C/V lost merged product name, its label, or clipboard snapshot isolation");
    const int cloneRow = dock->model()->rowForStreamId(cloneStream);
    require(cloneRow >= 0 && dock->model()->data(dock->model()->index(cloneRow,
        TerminalProductTableModel::ProductNameColumn), Qt::DisplayRole).toString() == "合流精矿",
        "Table does not display the cloned merged product name");
    undo->undo(); QApplication::processEvents();
    require(!mergeByStream(*scene, cloneStream) && document->productName(cloneStream).isEmpty(),
        "Undo retained the cloned merge or its name");
    undo->redo(); QApplication::processEvents();
    require(document->productName(cloneStream) == "合流精矿"
        && visibleName(mergeByStream(*scene, cloneStream), "合流精矿"), "Redo lost the merged product name");
    QString error;
    const QString path = directory + "/named-merge-copy.afs.json";
    require(ProjectSerializer::save(*scene, *document, path, &error), "Named clone could not be saved");
    FlowsheetScene restored; FlowsheetDocument restoredDocument;
    require(ProjectSerializer::load(restored, restoredDocument, path, &error)
        && restoredDocument.productName(cloneStream) == "合流精矿"
        && mergeByStream(restored, cloneStream)->products().size() == 3, "Project round trip lost merged product name");
    // Templates share capture/paste, including normalization to fresh merge IDs.
    scene->clearSelection();
    for (auto* item : scene->items())
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item);
            unit && (unit->unit().id == firstId || unit->unit().id == "name-branch" || unit->unit().id == "extra-branch")) unit->setSelected(true);
    flow_group::Snapshot snapshot;
    require(flow_group::capture(*scene, *document, snapshot).merges == 1, "Named template capture failed");
    FlowTemplateStore store(directory + "/named-merge-templates");
    const QString id = store.save("Named merged product", snapshot, &error);
    const auto loaded = store.load(id, &error);
    require(!id.isEmpty() && loaded && loaded->merges.size() == 1 && loaded->merges.front().sources.size() == 3
        && loaded->productNames.value(loaded->merges.front().id + ":output") == "复制后的源名称",
        "Template normalization lost merged product name");
}
