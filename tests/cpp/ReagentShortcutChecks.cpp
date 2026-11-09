#include "ReagentMenuTestSupport.h"
#include "graphics/items/FeedJunctionItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "services/ProjectSerializer.h"
#include <QClipboard>
#include <QLineEdit>
#include <QStatusBar>

using namespace afs;
using namespace afs::tests::reagent_menu;
namespace {
void key(Ui& ui, Qt::Key code) {
    ui.window.activateWindow(); ui.canvas->setFocus(); flush();
    QTest::keyClick(ui.canvas->viewport(), code, Qt::ControlModifier); flush();
}
void select(Ui& ui, QPointF point) {
    ui.scene->setSceneRect(ui.scene->sceneRect().united(QRectF(point - QPointF(700, 700), QSizeF(1400, 1400))));
    ui.canvas->centerOn(point); flush();
    QTest::mouseClick(ui.canvas->viewport(), Qt::LeftButton, Qt::NoModifier, ui.canvas->mapFromScene(point)); flush();
}
QString keyboardPaste(Ui& ui, QPointF point, const QString& stream) {
    select(ui, point);
    const auto before = count(*ui.document);
    key(ui, Qt::Key_V);
    const auto id = ui.selectedReagent();
    require(count(*ui.document) == before + 1 && !id.isEmpty()
        && ui.document->annotationRecord(id).ownerId == stream,
        "Keyboard paste did not bind to " + stream + ": " + ui.window.statusBar()->currentMessage());
    return id;
}
void ordinaryTransfers() {
    Ui ui; const auto source = sourceRecord();
    ui.document->setAnnotationRecord(source); ui.manager->synchronize(); flush();
    select(ui, ui.reagent(source.id)->sceneBoundingRect().center());
    QApplication::clipboard()->setText("ordinary text");
    const auto before = ProjectSerializer::serialize(*ui.scene, *ui.document);
    key(ui, Qt::Key_C);
    require(before == ProjectSerializer::serialize(*ui.scene, *ui.document)
        && QApplication::clipboard()->text() == "ordinary text", "Keyboard reagent copy changed project or system clipboard");
    ui.scene->clearSelection(); key(ui, Qt::Key_V);
    require(count(*ui.document) == 1 && !ui.unit("2"), "Paste with no target modified the canvas");
    auto edited = source; edited.text = "Changed after copy";
    ui.document->setAnnotationRecord(edited); ui.manager->synchronize();
    const auto first = keyboardPaste(ui, ui.unit()->products().back()->sourceAnchorScenePosition() + QPointF(0, 30), "1:right");
    sameContent(source, ui.document->annotationRecord(first));
    key(ui, Qt::Key_V); const auto second = ui.selectedReagent();
    require(first != second && count(*ui.document) == 3 && ui.document->annotationRecord(second).ownerId == "1:right",
        "Repeated keyboard paste failed");
    ui.undo->undo(); flush(); require(!ui.reagent(second) && ui.reagent(first), "Keyboard paste undo removed the wrong annotation");
    ui.undo->redo(); flush(); require(ui.reagent(second), "Keyboard paste redo failed");
    keyboardPaste(ui, (ui.unit()->inputLine()->startScenePosition() + ui.unit()->inputAnchorScenePosition()) / 2, "1:feed");
    // Both entry points share the same immutable reagent snapshot.
    copy(ui, source.id);
    sameContent(edited, ui.document->annotationRecord(keyboardPaste(ui,
        ui.unit()->products().back()->sourceAnchorScenePosition() + QPointF(0, 30), "1:right")));
    select(ui, ui.reagent(source.id)->sceneBoundingRect().center()); key(ui, Qt::Key_C);
    sameContent(edited, ui.document->annotationRecord(paste(ui,
        ui.unit()->products().back()->sourceAnchorScenePosition() + QPointF(0, 30), "1:right")));
    const auto reagentCount = count(*ui.document);
    QLineEdit text(&ui.window); text.setText("Input clipboard"); text.show(); text.setFocus(); text.selectAll(); flush();
    QTest::keyClick(&text, Qt::Key_C, Qt::ControlModifier);
    text.setCursorPosition(text.text().size()); QTest::keyClick(&text, Qt::Key_V, Qt::ControlModifier); flush();
    require(text.text() == "Input clipboardInput clipboard" && count(*ui.document) == reagentCount, "Canvas shortcuts intercepted input text copying");
    text.hide();
    ui.scene->clearSelection(); ui.unit()->setSelected(true); ui.reagent(source.id)->setSelected(true);
    key(ui, Qt::Key_C); key(ui, Qt::Key_V);
    require(ui.unit("2"), "Mixed group selection stopped copying units");
    select(ui, ui.reagent(source.id)->sceneBoundingRect().center()); key(ui, Qt::Key_C);
    keyboardPaste(ui, ui.unit()->products().back()->sourceAnchorScenePosition() + QPointF(0, 30), "1:right");
    require(!ui.unit("3"), "Switching from group to reagent pasted a flow group");
}
void branchTransfers() {
    Ui ui; auto* a = ui.unit();
    auto* b = new FlotationUnitItem({"b", {600, 300}});
    auto* c = new FlotationUnitItem({"c", {1000, 600}});
    auto* d = new FlotationUnitItem({"d", {300, 900}});
    for (auto* unit : {b, c, d}) ui.scene->addItem(unit);
    require(ui.scene->connectProductDirect(a->products().back(), b->inputLine()), "Cannot connect process source");
    auto* feed = ui.scene->connectRecycle(c->products().back(), b->inputLine(), "keyboard-feed");
    require(feed && ui.scene->connectRecycle(c->products().front(), b->inputLine()) == feed, "Cannot create recycle branches");
    const auto right = c->products().back()->streamId(), left = c->products().front()->streamId();
    feed->setManualRouteX(right, 1200); feed->setManualRouteX(left, -600);
    const auto source = sourceRecord(); ui.document->setAnnotationRecord(source); ui.manager->synchronize(); flush();
    select(ui, ui.reagent(source.id)->sceneBoundingRect().center()); key(ui, Qt::Key_C);
    keyboardPaste(ui, feed->sourceAnnotationAnchor(right) + QPointF(0, -50), right);
    keyboardPaste(ui, feed->sourceAnnotationAnchor(left) + QPointF(0, -50), left);
    keyboardPaste(ui, feed->processAnnotationAnchor(), a->products().back()->streamId());
    keyboardPaste(ui, feed->outputAnchor(), feed->outputStreamId());
    const auto before = count(*ui.document);
    select(ui, feed->junctionPosition()); key(ui, Qt::Key_V);
    require(count(*ui.document) == before, "Keyboard paste chose an arbitrary stream at a junction");
    auto* external = ui.scene->connectRecycle(b->products().front(), a->inputLine(), "keyboard-external");
    require(external, "Cannot create external feed");
    keyboardPaste(ui, (a->inputLine()->startScenePosition() * 3 + external->junctionPosition()) / 4, external->externalFeedStreamId());
    keyboardPaste(ui, external->outputAnchor(), external->outputStreamId());
    auto* merge = ui.scene->mergeProducts(a->products().front(), b->products().back(), "keyboard-merge");
    require(merge, "Cannot merge products");
    keyboardPaste(ui, a->products().front()->sourceAnchorScenePosition() + QPointF(0, 40), a->products().front()->streamId());
    keyboardPaste(ui, (merge->mergePosition() + merge->outputEndPosition()) / 2, merge->outputStreamId());
    require(ui.scene->connectMergeDirect(merge, d->inputLine()), "Cannot connect merged output");
    keyboardPaste(ui, QPointF(merge->outputEndPosition().x(), (merge->mergeY() + merge->outputEndPosition().y()) / 2), merge->outputStreamId());
}
}
void runReagentShortcutChecks() { ordinaryTransfers(); branchTransfers(); }
