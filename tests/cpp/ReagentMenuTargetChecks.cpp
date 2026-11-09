#include "ReagentMenuTestSupport.h"
#include "annotations/AnnotationContextTarget.h"
#include "graphics/items/FeedJunctionItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include <QStatusBar>

using namespace afs;
using namespace afs::tests::reagent_menu;

namespace {
void routeResetMenus() {
    Ui ui; auto* first = ui.unit();
    auto* second = new FlotationUnitItem({"next", {600, 300}}); ui.scene->addItem(second);
    auto* product = first->products().back();
    require(ui.scene->connectProductDirect(product, second->inputLine()), "Cannot connect route reset fixture");
    product->setManualRouteY(240);
    require(menu(ui, product->sourceAnchorScenePosition() + QPointF(0, 25), "resetAnnotationRouteAction").enabled
        && !product->manualRouteY(), "Unified product menu did not restore automatic routing");
    auto* merge = ui.scene->mergeProducts(first->products().front(), second->products().front(), "reset-merge");
    require(merge, "Cannot merge route reset fixture"); merge->setManualMergeY(600);
    require(menu(ui, first->products().front()->sourceAnchorScenePosition() + QPointF(0, 40), "resetAnnotationRouteAction").enabled
        && !merge->manualMergeY(), "Unified merge menu did not restore automatic routing");
    auto* feed = ui.scene->connectRecycle(second->products().back(), first->inputLine(), "reset-feed");
    require(feed, "Cannot create recycle reset fixture");
    const auto stream = second->products().back()->streamId();
    feed->setManualRouteX(stream, 1200); feed->setManualRouteY(stream, -200);
    require(menu(ui, feed->sourceAnnotationAnchor(stream) + QPointF(0, -50), "resetAnnotationRouteAction").enabled
        && feed->manualRouteXs().isEmpty() && feed->manualRouteYs().isEmpty(),
        "Unified feed menu did not restore all source routes");
}
}

void runReagentMenuTargetChecks() {
    routeResetMenus();
    Ui ui; auto* a = ui.unit();
    auto* b = new FlotationUnitItem({"b", {600, 300}});
    auto* c = new FlotationUnitItem({"c", {1000, 600}});
    auto* d = new FlotationUnitItem({"d", {300, 900}});
    for (auto* item : {b, c, d}) ui.scene->addItem(item);
    require(ui.scene->connectProductDirect(a->products().back(), b->inputLine()), "Cannot connect process feed");
    auto* feed = ui.scene->connectRecycle(c->products().back(), b->inputLine(), "multi-feed");
    require(feed && ui.scene->connectRecycle(c->products().front(), b->inputLine()) == feed, "Cannot create multiple recycle branches");
    const auto rightId = c->products().back()->streamId(), leftId = c->products().front()->streamId();
    feed->setManualRouteX(rightId, 1200); feed->setManualRouteX(leftId, -600);
    const auto source = sourceRecord(); ui.document->setAnnotationRecord(source); ui.manager->synchronize(); flush(); copy(ui, source.id);
    feed->selectSourceStream(leftId);
    paste(ui, feed->sourceAnnotationAnchor(rightId) + QPointF(0, -50), rightId);
    for (const auto& record : ui.document->annotationRecords())
        require(record.kind != AnnotationKind::Reagent || record.ownerId != leftId,
                "Context pasted into the previously selected recycle branch");
    paste(ui, feed->sourceAnnotationAnchor(leftId) + QPointF(0, -50), leftId);
    paste(ui, feed->processAnnotationAnchor(), a->products().back()->streamId());
    paste(ui, feed->outputAnchor(), feed->outputStreamId());
    const auto beforeAmbiguous = count(*ui.document);
    const auto ambiguous = menu(ui, feed->junctionPosition(), "pasteReagentAnnotationAction", false);
    require(ambiguous.found && !ambiguous.enabled && count(*ui.document) == beforeAmbiguous,
            "Ambiguous shared junction silently selected a stream");
    auto* external = ui.scene->connectRecycle(b->products().front(), a->inputLine(), "external-feed");
    require(external && external->hasExternalFeed(), "Missing external-feed fixture");
    const auto freshPoint = (a->inputLine()->startScenePosition() * 3 + external->junctionPosition()) / 4;
    paste(ui, freshPoint, external->externalFeedStreamId());
    paste(ui, external->outputAnchor(), external->outputStreamId());
    auto* merge = ui.scene->mergeProducts(a->products().front(), b->products().back(), "product-merge");
    require(merge, "Cannot create product merge");
    const auto branchStart = a->products().front()->sourceAnchorScenePosition();
    const auto emptySpace = (branchStart + QPointF(branchStart.x(), merge->mergeY()) + merge->mergePosition()) / 3;
    require(!merge->shape().contains(merge->mapFromScene(emptySpace)), "Merge hit area fills empty space between branches");
    paste(ui, (merge->mergePosition() + merge->outputEndPosition()) / 2, merge->outputStreamId());
    const auto mergeBranchPoint = a->products().front()->sourceAnchorScenePosition() + QPointF(0, 40);
    const auto branchTarget = annotationContextTarget(*ui.scene, mergeBranchPoint);
    require(branchTarget.streamId == a->products().front()->streamId(),
        QString("Wrong merge branch hit at %1,%2: stream %3 reagent %4; merge query %5")
            .arg(mergeBranchPoint.x()).arg(mergeBranchPoint.y()).arg(branchTarget.streamId, branchTarget.reagentId,
                merge->streamAtScenePosition(mergeBranchPoint)));
    paste(ui, mergeBranchPoint, a->products().front()->streamId());
    require(ui.scene->connectMergeDirect(merge, d->inputLine()), "Cannot connect merged output");
    paste(ui, QPointF(merge->outputEndPosition().x(),
                     (merge->mergeY() + merge->outputEndPosition().y()) / 2), merge->outputStreamId());
    const auto beforeStale = count(*ui.document);
    const auto stale = menu(ui, d->products().back()->sourceAnchorScenePosition() + QPointF(0, 30),
        "pasteReagentAnnotationAction", true, [&] { ui.scene->removeUnit(d); });
    require(stale.enabled && count(*ui.document) == beforeStale && ui.window.statusBar()->currentMessage().contains("不存在"),
            "A removed context target still accepted a pasted reagent");
}
