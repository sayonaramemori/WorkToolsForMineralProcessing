#include "adapters/CanvasTopologyBuilder.h"
#include "annotations/AnnotationTypes.h"
#include "document/FlowsheetDocument.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FeedJunctionItem.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"
#include "services/ProjectSerializer.h"
#include "topology/OpenCircuitCalculator.h"
#include "topology/TopologyAlgorithms.h"
#include "topology/TopologyValidator.h"

#include <QApplication>
#include <QFile>
#include <QTemporaryDir>
#include <cmath>

using namespace afs;

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;

    FlowsheetScene source;
    auto* upper = new FlotationUnitItem({"10", {-120, -80}, 420, 170});
    auto* lower = new FlotationUnitItem({"20", {460, 360}, 320, 140});
    source.addItem(upper);
    source.addItem(lower);
    if (!source.connectProduct(upper->products().at(1), lower->inputLine())) return 2;
    auto* merge = source.mergeProducts(
        upper->products().at(0), lower->products().at(1), "merge-17");
    if (!merge) return 3;
    auto* feed = source.connectMergedProduct(merge, upper->inputLine(), "feed-merge-9");
    if (!feed) return 4;
    upper->products().at(1)->setManualRouteY(245.0);
    merge->setManualMergeY(515.0);
    feed->setManualRouteX(merge->outputStreamId(), 735.0);
    feed->setManualRouteY(merge->outputStreamId(), 145.0);

    FlowsheetDocument sourceDocument;
    sourceDocument.setDryMass("upper-unused", 12.5);
    sourceDocument.setGradePercent("upper-unused", 3.25);
    sourceDocument.setComponents({{"component-1", "Cu"}, {"component-2", "Zn"}});
    sourceDocument.setGradePercent("upper-unused", "component-2", 1.75);
    sourceDocument.setProductName("10:left", "最终精矿");
    sourceDocument.setExportStreamOrder({"10:right", "10:left", "10:feed"});
    sourceDocument.setAnnotationTextSettings({14, true, QColor("#336699")});
    AnnotationRecord annotation;
    annotation.id = "result:10:left";
    annotation.ownerId = "10:left";
    annotation.manualOffset = {22, -13};
    annotation.manuallyPlaced = true;
    sourceDocument.setAnnotationRecord(annotation);
    AnnotationRecord note{"note-1", AnnotationKind::UserNote, AnnotationStyle::Note,
                          AnnotationOwnerKind::Free, "canvas", {310, 175}, true, true,
                          "自定义文字\n第二行"};
    note.noteFontFamily = "DejaVu Serif";
    note.notePointSize = 20;
    note.noteBold = true;
    note.noteBorderVisible = false;
    sourceDocument.setAnnotationRecord(note);
    AnnotationRecord reagent{"reagent-1", AnnotationKind::Reagent, AnnotationStyle::PlainText,
                             AnnotationOwnerKind::Stream, "10:left", {}, false, true};
    reagent.text = "捕收剂 A";
    reagent.dosage = "120";
    reagent.dosageUnit = "g/t";
    reagent.manualOffset = {0, 36};
    reagent.manuallyPlaced = true;
    sourceDocument.setAnnotationRecord(reagent);
    topology::CalculationResult attemptedCalculation;
    attemptedCalculation.issues.append({topology::IssueSeverity::Error,
        topology::IssueCode::Underdetermined, "test", "test"});
    sourceDocument.setCalculationResult(std::move(attemptedCalculation));
    const int alternative = sourceDocument.addScenario("方案 B", true);
    if (alternative != 1 || !sourceDocument.setCurrentScenario(alternative)) return 24;
    sourceDocument.setDryMass("upper-unused", 18.0);
    auto alternativeReagent = sourceDocument.annotationRecord("reagent-1");
    alternativeReagent.dosage = "160";
    sourceDocument.setAnnotationRecord(alternativeReagent);
    if (!sourceDocument.setCurrentScenario(0)) return 25;

    const QString path = directory.filePath("roundtrip.afs.json");
    QString error;
    if (!ProjectSerializer::save(source, sourceDocument, path, &error)) return 5;

    FlowsheetScene loaded;
    FlowsheetDocument loadedDocument;
    if (!ProjectSerializer::load(loaded, loadedDocument, path, &error)) return 6;
    const auto snapshot = CanvasTopologyBuilder::build(loaded);
    if (snapshot.graph.nodeIds().size() != 4
        || !topology::TopologyAlgorithms::sort(snapshot.graph).hasCycle) return 7;

    FlotationUnitItem* loadedUpper = nullptr;
    MergeJunctionItem* loadedMerge = nullptr;
    FeedJunctionItem* loadedFeed = nullptr;
    for (auto* item : loaded.items()) {
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item); unit && unit->unit().id == "10")
            loadedUpper = unit;
        else if (auto* itemMerge = dynamic_cast<MergeJunctionItem*>(item)) loadedMerge = itemMerge;
        else if (auto* itemFeed = dynamic_cast<FeedJunctionItem*>(item)) loadedFeed = itemFeed;
    }
    if (!loadedUpper || !loadedMerge || !loadedFeed
        || loadedMerge->id() != "merge-17" || loadedFeed->id() != "feed-merge-9") return 8;
    if (!loadedUpper->products().at(1)->manualRouteY()
        || std::abs(*loadedUpper->products().at(1)->manualRouteY() - 245.0) > 0.001
        || !loadedMerge->manualMergeY()
        || std::abs(*loadedMerge->manualMergeY() - 515.0) > 0.001
        || !loadedFeed->manualRouteXs().contains(loadedMerge->outputStreamId())
        || std::abs(loadedFeed->manualRouteXs().value(loadedMerge->outputStreamId()) - 735.0)
            > 0.001
        || !loadedFeed->manualRouteYs().contains(loadedMerge->outputStreamId())
        || std::abs(loadedFeed->manualRouteYs().value(loadedMerge->outputStreamId()) - 145.0)
            > 0.001) return 35;
    if (std::abs(loadedUpper->unit().position.x() + 120) > 0.001
        || std::abs(loadedUpper->unit().width - 420) > 0.001
        || std::abs(loadedUpper->unit().bodyHeight - 170) > 0.001) return 9;
    const auto measurement = loadedDocument.measurement("upper-unused");
    if (!measurement.dryMass || !measurement.gradePercent
        || std::abs(*measurement.dryMass - 12.5) > 0.001
        || std::abs(*measurement.gradePercent - 3.25) > 0.001) return 10;
    if (loadedDocument.components().size() != 2
        || loadedDocument.components()[0].name != "Cu"
        || loadedDocument.components()[1].name != "Zn"
        || !measurement.grade("component-2")
        || std::abs(*measurement.grade("component-2") - 1.75) > 0.001) return 29;
    const auto loadedAnnotation = loadedDocument.annotationRecord("result:10:left");
    if (!loadedAnnotation.manuallyPlaced || loadedAnnotation.manualOffset != QPointF(22, -13))
        return 11;
    const auto loadedNote = loadedDocument.annotationRecord("note-1");
    if (loadedNote.kind != AnnotationKind::UserNote
        || loadedNote.text != "自定义文字\n第二行"
        || loadedNote.manualOffset != QPointF(310, 175)
        || loadedNote.noteFontFamily != "DejaVu Serif" || loadedNote.notePointSize != 20
        || !loadedNote.noteBold || loadedNote.noteBorderVisible) return 28;
    const auto loadedReagent = loadedDocument.annotationRecord("reagent-1");
    if (loadedReagent.text != "捕收剂 A" || loadedReagent.dosage != "120"
        || loadedReagent.dosageUnit != "g/t" || !loadedReagent.manuallyPlaced
        || loadedReagent.manualOffset != QPointF(0, 36)) return 23;
    if (loadedDocument.scenarios().size() != 2
        || loadedDocument.scenarios()[1].name != "方案 B"
        || !loadedDocument.scenarios()[0].calculationResult.has_value()
        || loadedDocument.scenarios()[1].calculationResult.has_value()
        || !loadedDocument.setCurrentScenario(1)
        || std::abs(*loadedDocument.measurement("upper-unused").dryMass - 18.0) > 0.001
        || loadedDocument.annotationRecord("reagent-1").dosage != "160") return 26;
    loadedDocument.setCurrentScenario(0);
    if (loadedDocument.productName("10:left") != "最终精矿") return 21;
    if (loadedDocument.exportStreamOrder()
        != QStringList({"10:right", "10:left", "10:feed"})) return 30;
    const auto loadedTextStyle = loadedDocument.annotationTextSettings();
    if (loadedTextStyle.pointSize != 14 || !loadedTextStyle.bold
        || loadedTextStyle.color != QColor("#336699")) return 22;

    const int itemCount = loaded.items().size();
    const QString invalidPath = directory.filePath("invalid.afs.json");
    QFile invalid(invalidPath);
    if (!invalid.open(QIODevice::WriteOnly) || invalid.write("{broken") < 0) return 12;
    invalid.close();
    if (ProjectSerializer::load(loaded, loadedDocument, invalidPath, &error)) return 13;
    if (loaded.items().size() != itemCount) return 14;

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

    FlowsheetScene splitterSource;
    FlotationUnit splitterUnit{"splitter", {120, 80}};
    splitterUnit.kind = UnitKind::BinarySplitter;
    splitterUnit.leftSplitPercent = 37.5;
    splitterSource.addItem(new FlotationUnitItem(splitterUnit));
    FlowsheetDocument splitterDocument;
    const QString splitterPath = directory.filePath("splitter.afs.json");
    if (!ProjectSerializer::save(splitterSource, splitterDocument, splitterPath, &error)) return 39;
    FlowsheetScene splitterLoaded;
    FlowsheetDocument splitterLoadedDocument;
    if (!ProjectSerializer::load(
            splitterLoaded, splitterLoadedDocument, splitterPath, &error)) return 40;
    FlotationUnitItem* loadedSplitter = nullptr;
    for (auto* item : splitterLoaded.items())
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item);
            unit && unit->unit().id == "splitter") loadedSplitter = unit;
    if (!loadedSplitter || loadedSplitter->unit().kind != UnitKind::BinarySplitter
        || std::abs(loadedSplitter->unit().leftSplitPercent - 37.5) > 0.001) return 41;
    const auto splitterSnapshot = CanvasTopologyBuilder::build(splitterLoaded);
    if (splitterSnapshot.requiredMeasurements.size() != 1
        || splitterSnapshot.requiredMeasurements.front().streamId != "splitter:left") return 42;

    FlowsheetScene threeProductSource;
    FlotationUnit threeProductUnit{"three", {240, 160}};
    threeProductUnit.kind = UnitKind::ThreeProductFlotation;
    threeProductSource.addItem(new FlotationUnitItem(threeProductUnit));
    FlowsheetDocument threeProductDocument;
    const QString threeProductPath = directory.filePath("three-product.afs.json");
    if (!ProjectSerializer::save(
            threeProductSource, threeProductDocument, threeProductPath, &error)) return 43;
    FlowsheetScene threeProductLoaded;
    FlowsheetDocument threeProductLoadedDocument;
    if (!ProjectSerializer::load(threeProductLoaded, threeProductLoadedDocument,
                                 threeProductPath, &error)) return 44;
    FlotationUnitItem* loadedThreeProduct = nullptr;
    for (auto* item : threeProductLoaded.items())
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item);
            unit && unit->unit().id == "three") loadedThreeProduct = unit;
    if (!loadedThreeProduct
        || loadedThreeProduct->unit().kind != UnitKind::ThreeProductFlotation
        || loadedThreeProduct->products().size() != 3
        || loadedThreeProduct->products()[1]->streamId() != "three:middle") return 45;
    const auto threeProductSnapshot = CanvasTopologyBuilder::build(threeProductLoaded);
    if (threeProductSnapshot.graph.streamsFrom(
            "three", topology::PortKind::MiddleProduct).size() != 1
        || threeProductSnapshot.requiredMeasurements.size() != 3) return 46;
    return 0;
}
