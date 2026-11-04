#include "ProjectIoChecks.h"
#include "document/FlowsheetDocument.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/ProductLineItem.h"
#include "services/ProjectSerializer.h"
#include "adapters/CanvasTopologyBuilder.h"
#include "graphics/items/FeedJunctionItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "topology/OpenCircuitCalculator.h"
#include "topology/TopologyAlgorithms.h"
#include "topology/TopologyValidator.h"
#include <QTemporaryDir>

namespace afs::tests {

int checkProjectConnections(const QTemporaryDir& directory) {
    QString error;
    FlowsheetScene downstreamSource;
    auto* branchA = new FlotationUnitItem({"A", {0, 0}});
    auto* branchB = new FlotationUnitItem({"B", {400, 0}});
    auto* nextStage = new FlotationUnitItem({"C", {800, 600}});
    downstreamSource.addItem(branchA);
    downstreamSource.addItem(branchB);
    downstreamSource.addItem(nextStage);
    if (!downstreamSource.connectProductDirect(
            branchA->products().at(0), branchB->inputLine())) return 15;
    auto* downstreamMerge = downstreamSource.mergeProducts(
        branchA->products().at(1), branchB->products().at(0), "merge-23");
    if (!downstreamMerge || !downstreamSource.connectMerge(
            downstreamMerge, nextStage->inputLine())) return 27;
    FlowsheetDocument downstreamDocument;
    const QString downstreamPath = directory.filePath("merged-downstream.afs.json");
    if (!ProjectSerializer::save(
            downstreamSource, downstreamDocument, downstreamPath, &error)) return 16;
    FlowsheetScene downstreamLoaded;
    FlowsheetDocument downstreamLoadedDocument;
    if (!ProjectSerializer::load(
            downstreamLoaded, downstreamLoadedDocument, downstreamPath, &error)) return 17;
    const auto downstreamSnapshot = CanvasTopologyBuilder::build(downstreamLoaded);
    const auto* output = downstreamSnapshot.graph.stream("merge-23:output");
    if (!output || !output->target || output->target->nodeId != "C"
        || downstreamSnapshot.graph.terminalProductStreams().contains("merge-23:output"))
        return 18;
    if (downstreamSnapshot.terminalProducts.size() != 3
        || downstreamSnapshot.productStreams.size() != 7
        || downstreamSnapshot.reportStreams.size() != 8
        || downstreamSnapshot.requiredMeasurements.size() != 4
        || downstreamSnapshot.requiredMeasurements.back().streamId != "A:right"
        || !downstreamSnapshot.requiredMeasurements.back().displayName.contains("合流支路"))
        return 19;
    QHash<topology::StreamId, topology::StreamValue> known;
    known.insert("A:right", *topology::StreamValue::fromMassAndGrade(30, 2));
    known.insert("B:right", *topology::StreamValue::fromMassAndGrade(20, 3));
    known.insert("C:left", *topology::StreamValue::fromMassAndGrade(60, 2));
    known.insert("C:right", *topology::StreamValue::fromMassAndGrade(40, 2));
    const auto downstreamResult = topology::OpenCircuitCalculator::calculate(
        downstreamSnapshot.graph, known);
    if (!downstreamResult.complete || !downstreamResult.values.contains("B:left")) return 20;

    FlowsheetScene occupiedSource;
    auto* occupiedA = new FlotationUnitItem({"OA", {0, 0}});
    auto* occupiedB = new FlotationUnitItem({"OB", {400, 350}});
    auto* occupiedC = new FlotationUnitItem({"OC", {800, 700}});
    occupiedSource.addItem(occupiedA); occupiedSource.addItem(occupiedB);
    occupiedSource.addItem(occupiedC);
    if (!occupiedSource.connectProductDirect(
            occupiedA->products().at(1), occupiedB->inputLine())
        || !occupiedSource.connectProductDirect(
            occupiedB->products().at(1), occupiedC->inputLine())
        || !occupiedSource.connectRecycle(
            occupiedC->products().at(0), occupiedB->inputLine(), "occupied-feed")
        || !occupiedSource.connectRecycle(
            occupiedC->products().at(1), occupiedB->inputLine())) return 31;
    FlowsheetDocument occupiedDocument;
    const QString occupiedPath = directory.filePath("occupied-feed.afs.json");
    if (!ProjectSerializer::save(
            occupiedSource, occupiedDocument, occupiedPath, &error)) return 32;
    FlowsheetScene occupiedLoaded;
    FlowsheetDocument occupiedLoadedDocument;
    if (!ProjectSerializer::load(
            occupiedLoaded, occupiedLoadedDocument, occupiedPath, &error)) return 33;
    FeedJunctionItem* loadedOccupiedFeed = nullptr;
    for (auto* item : occupiedLoaded.items())
        if (auto* junction = dynamic_cast<FeedJunctionItem*>(item);
            junction && junction->id() == "occupied-feed") loadedOccupiedFeed = junction;
    if (!loadedOccupiedFeed || loadedOccupiedFeed->hasExternalFeed()
        || !loadedOccupiedFeed->processProduct()
        || loadedOccupiedFeed->processProduct()->streamId() != "OA:right"
        || loadedOccupiedFeed->recycleProducts().size() != 2
        || loadedOccupiedFeed->recycleProducts()[0]->streamId() != "OC:left"
        || loadedOccupiedFeed->recycleProducts()[1]->streamId() != "OC:right") return 34;

    FlowsheetScene multiLoopSource;
    auto* multiA = new FlotationUnitItem({"MA", {0, 0}});
    auto* multiB = new FlotationUnitItem({"MB", {400, 350}});
    auto* multiC = new FlotationUnitItem({"MC", {800, 700}});
    multiLoopSource.addItem(multiA); multiLoopSource.addItem(multiB);
    multiLoopSource.addItem(multiC);
    if (!multiLoopSource.connectProduct(multiA->products().at(1), multiB->inputLine())
        || !multiLoopSource.connectProduct(multiB->products().at(1), multiC->inputLine())
        || !multiLoopSource.connectRecycle(
            multiB->products().at(0), multiA->inputLine(), "multi-loop-a")
        || !multiLoopSource.connectRecycle(
            multiC->products().at(0), multiB->inputLine(), "multi-loop-b")) return 35;
    const QString multiLoopPath = directory.filePath("multi-loop.afs.json");
    FlowsheetDocument multiLoopDocument;
    if (!ProjectSerializer::save(
            multiLoopSource, multiLoopDocument, multiLoopPath, &error)) return 36;
    FlowsheetScene multiLoopLoaded;
    FlowsheetDocument multiLoopLoadedDocument;
    if (!ProjectSerializer::load(
            multiLoopLoaded, multiLoopLoadedDocument, multiLoopPath, &error)) return 37;
    QSet<QString> loadedFeedIds;
    for (auto* item : multiLoopLoaded.items())
        if (auto* feed = dynamic_cast<FeedJunctionItem*>(item)) loadedFeedIds.insert(feed->id());
    const auto multiLoopSnapshot = CanvasTopologyBuilder::build(multiLoopLoaded);
    if (loadedFeedIds != QSet<QString>{"multi-loop-a", "multi-loop-b"}
        || multiLoopSnapshot.graph.externalFeedStreams().size() != 1
        || !topology::TopologyAlgorithms::sort(multiLoopSnapshot.graph).hasCycle
        || topology::TopologyValidator::hasErrors(
            topology::TopologyValidator::validate(multiLoopSnapshot.graph))) return 38;
    return 0;
}

} // namespace afs::tests
