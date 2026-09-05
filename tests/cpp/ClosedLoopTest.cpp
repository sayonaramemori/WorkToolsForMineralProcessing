#include "adapters/CanvasTopologyBuilder.h"
#include "editor/CanvasActions.h"
#include "editor/FlowsheetScene.h"
#include "graphics/FlotationGeometry.h"
#include "graphics/items/FeedJunctionItem.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"
#include "topology/OpenCircuitCalculator.h"
#include "topology/TopologyAlgorithms.h"
#include "topology/TopologyValidator.h"

#include <QApplication>
#include <cmath>

using namespace afs;

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    FlowsheetScene scene;
    auto* first = new FlotationUnitItem({"first", {0, 0}});
    auto* second = new FlotationUnitItem({"second", {500, 400}});
    scene.addItem(first);
    scene.addItem(second);

    if (!scene.connectProduct(first->products().at(1), second->inputLine())) return 1;
    auto* recycle = second->products().at(0);
    if (!scene.connectProduct(recycle, first->inputLine())) return 2;
    auto* junction = recycle->feedJunction();
    if (!junction || first->inputLine()->feedJunction() != junction) return 3;
    if (recycle->isVisible() || first->inputLine()->isVisible()) return 4;
    if (std::abs(junction->junctionPosition().x() - first->scenePos().x()) > 0.001
        || std::abs(junction->junctionPosition().y()
                    - (first->scenePos().y() - FlotationGeometry::InputHeight / 2.0)) > 0.001)
        return 5;

    const auto snapshot = CanvasTopologyBuilder::build(scene);
    if (snapshot.graph.nodeIds().size() != 3 || snapshot.graph.externalFeedStreams().size() != 1
        || snapshot.graph.terminalProductStreams().size() != 2) return 6;
    if (!topology::TopologyAlgorithms::sort(snapshot.graph).hasCycle) return 7;
    const auto issues = topology::TopologyValidator::validate(snapshot.graph);
    if (topology::TopologyValidator::hasErrors(issues)) return 8;

    if (snapshot.requiredMeasurements.size() != 3) return 9;
    QHash<topology::StreamId, topology::StreamValue> closedValues;
    closedValues.insert("first:left", *topology::StreamValue::fromMassAndGrade(10, 5));
    closedValues.insert("second:right", *topology::StreamValue::fromMassAndGrade(40, 1));
    closedValues.insert("second:left", *topology::StreamValue::fromMassAndGrade(50, 2));
    const auto result = topology::OpenCircuitCalculator::calculate(snapshot.graph, closedValues);
    if (!result.complete || !result.values.contains(junction->externalFeedStreamId())) return 10;
    const auto externalFeed = result.values[junction->externalFeedStreamId()];
    if (std::abs(externalFeed.dryMass - 50.0) > 0.001
        || std::abs(externalFeed.gradePercent() - 1.8) > 0.001) return 11;

    if (!scene.disconnectRecycle(junction)) return 12;
    if (!recycle->isVisible() || !first->inputLine()->isVisible()
        || recycle->feedJunction() || first->inputLine()->feedJunction()) return 13;

    FlowsheetScene mergedScene;
    auto* upper = new FlotationUnitItem({"upper", {0, 0}});
    auto* lower = new FlotationUnitItem({"lower", {500, 400}});
    mergedScene.addItem(upper);
    mergedScene.addItem(lower);
    if (!mergedScene.connectProduct(upper->products().at(1), lower->inputLine())) return 14;
    auto* productMerge = mergedScene.mergeProducts(
        upper->products().at(0), lower->products().at(1));
    if (!productMerge) return 15;
    auto* mergedFeed = mergedScene.connectMergedProduct(productMerge, upper->inputLine());
    if (!mergedFeed || productMerge->feedJunction() != mergedFeed
        || mergedFeed->recycleMerge() != productMerge) return 16;

    const auto mergedSnapshot = CanvasTopologyBuilder::build(mergedScene);
    const auto* mergedOutput = mergedSnapshot.graph.stream(productMerge->outputStreamId());
    if (!mergedOutput || !mergedOutput->target
        || mergedOutput->target->nodeId != mergedFeed->id()) return 17;
    if (mergedSnapshot.graph.terminalProductStreams().contains(productMerge->outputStreamId()))
        return 18;
    if (!topology::TopologyAlgorithms::sort(mergedSnapshot.graph).hasCycle
        || topology::TopologyValidator::hasErrors(
            topology::TopologyValidator::validate(mergedSnapshot.graph))) return 19;

    if (mergedSnapshot.requiredMeasurements.size() != 3) return 20;
    QHash<topology::StreamId, topology::StreamValue> mergedValues;
    mergedValues.insert("upper:left", *topology::StreamValue::fromMassAndGrade(10, 5));
    mergedValues.insert("lower:right", *topology::StreamValue::fromMassAndGrade(40, 2));
    mergedValues.insert("lower:left", *topology::StreamValue::fromMassAndGrade(50, 1));
    const auto mergedResult = topology::OpenCircuitCalculator::calculate(
        mergedSnapshot.graph, mergedValues);
    if (!mergedResult.complete
        || std::abs(mergedResult.values[mergedFeed->externalFeedStreamId()].dryMass - 50.0) > 0.001)
        return 21;

    if (!mergedScene.disconnectRecycle(mergedFeed) || !productMerge->isAvailable()
        || !upper->inputLine()->isVisible()) return 22;
    const auto restoredSnapshot = CanvasTopologyBuilder::build(mergedScene);
    if (!restoredSnapshot.graph.terminalProductStreams().contains(productMerge->outputStreamId()))
        return 23;
    auto* firstMergedProduct = productMerge->firstProduct();
    auto* secondMergedProduct = productMerge->secondProduct();
    auto* secondFeed = mergedScene.connectMergedProduct(productMerge, upper->inputLine());
    if (!secondFeed) return 24;
    productMerge->setSelected(true);
    secondFeed->setSelected(true);
    if (CanvasActions::disconnectSelection(mergedScene) != 2) return 25;
    if (!firstMergedProduct->isAvailable() || !secondMergedProduct->isAvailable()
        || !upper->inputLine()->isVisible()) return 26;

    FlowsheetScene occupiedFeedScene;
    auto* stageA = new FlotationUnitItem({"A", {0, 0}});
    auto* stageB = new FlotationUnitItem({"B", {400, 350}});
    auto* stageC = new FlotationUnitItem({"C", {800, 700}});
    occupiedFeedScene.addItem(stageA);
    occupiedFeedScene.addItem(stageB);
    occupiedFeedScene.addItem(stageC);
    auto* normalFeed = stageA->products().at(1);
    if (!occupiedFeedScene.connectProduct(normalFeed, stageB->inputLine())
        || !occupiedFeedScene.connectProduct(stageB->products().at(1), stageC->inputLine()))
        return 27;
    const QPointF occupiedInputMidpoint = stageB->mapToScene(
        QPointF(0, -FlotationGeometry::InputHeight / 2.0));
    if (stageB->inputLine()->isVisible()
        || occupiedFeedScene.inputAtDropPosition(occupiedInputMidpoint) != stageB->inputLine())
        return 28;
    auto* downstreamRecycle = stageC->products().at(0);
    if (!occupiedFeedScene.connectProduct(downstreamRecycle, stageB->inputLine())) return 29;
    auto* occupiedJunction = downstreamRecycle->feedJunction();
    if (!occupiedJunction || occupiedJunction->processProduct() != normalFeed
        || occupiedJunction->hasExternalFeed() || normalFeed->isVisible()
        || downstreamRecycle->isVisible()) return 30;
    auto* secondRecycle = stageC->products().at(1);
    if (occupiedFeedScene.inputAtDropPosition(occupiedInputMidpoint) != stageB->inputLine()
        || !occupiedFeedScene.connectProduct(secondRecycle, stageB->inputLine())
        || secondRecycle->feedJunction() != occupiedJunction
        || occupiedJunction->recycleProducts().size() != 2) return 31;
    const auto occupiedSnapshot = CanvasTopologyBuilder::build(occupiedFeedScene);
    if (occupiedSnapshot.graph.externalFeedStreams().size() != 1
        || !topology::TopologyAlgorithms::sort(occupiedSnapshot.graph).hasCycle
        || topology::TopologyValidator::hasErrors(
            topology::TopologyValidator::validate(occupiedSnapshot.graph))) return 32;
    const CanvasStreamDescriptor* processDescriptor = nullptr;
    const CanvasStreamDescriptor* recycleDescriptor = nullptr;
    for (const auto& descriptor : occupiedSnapshot.reportStreams) {
        if (descriptor.streamId == normalFeed->streamId()) processDescriptor = &descriptor;
        if (descriptor.streamId == downstreamRecycle->streamId()) recycleDescriptor = &descriptor;
    }
    if (!processDescriptor || !processDescriptor->feed || processDescriptor->recycle
        || !processDescriptor->displayName.contains("主入料")
        || !recycleDescriptor || recycleDescriptor->feed || !recycleDescriptor->recycle
        || !recycleDescriptor->displayName.contains("回流")) return 38;
    if (!occupiedFeedScene.disconnectRecycle(occupiedJunction)
        || normalFeed->targetUnit() != stageB || normalFeed->feedJunction()
        || stageB->inputLine()->sourceProduct() != normalFeed
        || downstreamRecycle->feedJunction() || secondRecycle->feedJunction()) return 33;

    // Two independent recycle junctions in the same main flow must coexist.
    FlowsheetScene multiLoopScene;
    auto* loopA = new FlotationUnitItem({"LA", {0, 0}});
    auto* loopB = new FlotationUnitItem({"LB", {400, 350}});
    auto* loopC = new FlotationUnitItem({"LC", {800, 700}});
    multiLoopScene.addItem(loopA); multiLoopScene.addItem(loopB); multiLoopScene.addItem(loopC);
    if (!multiLoopScene.connectProduct(loopA->products().at(1), loopB->inputLine())
        || !multiLoopScene.connectProduct(loopB->products().at(1), loopC->inputLine())) return 34;
    auto* firstLoop = multiLoopScene.connectRecycle(
        loopB->products().at(0), loopA->inputLine(), "loop-feed-a");
    auto* secondLoop = multiLoopScene.connectRecycle(
        loopC->products().at(0), loopB->inputLine(), "loop-feed-b");
    if (!firstLoop || !secondLoop || firstLoop == secondLoop
        || secondLoop->processProduct() != loopA->products().at(1)) return 35;
    const auto multiLoopSnapshot = CanvasTopologyBuilder::build(multiLoopScene);
    if (multiLoopSnapshot.graph.externalFeedStreams().size() != 1
        || multiLoopSnapshot.requiredMeasurements.size() != 4
        || !topology::TopologyAlgorithms::sort(multiLoopSnapshot.graph).hasCycle
        || topology::TopologyValidator::hasErrors(
            topology::TopologyValidator::validate(multiLoopSnapshot.graph))) return 36;
    QHash<topology::StreamId, topology::StreamValue> multiLoopValues;
    multiLoopValues.insert("LA:left", *topology::StreamValue::fromMassAndGrade(10, 5));
    multiLoopValues.insert("LB:left", *topology::StreamValue::fromMassAndGrade(20, 3));
    multiLoopValues.insert("LC:left", *topology::StreamValue::fromMassAndGrade(30, 2));
    multiLoopValues.insert("LC:right", *topology::StreamValue::fromMassAndGrade(40, 1));
    const auto multiLoopResult = topology::OpenCircuitCalculator::calculate(
        multiLoopSnapshot.graph, multiLoopValues);
    if (!multiLoopResult.complete || !multiLoopResult.fullySolved
        || std::abs(multiLoopResult.values[firstLoop->externalFeedStreamId()].dryMass - 50.0)
            > 0.001) return 37;
    return 0;
}
