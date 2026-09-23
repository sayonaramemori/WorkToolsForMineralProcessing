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
#include "services/ProjectUndoManager.h"
#include "services/FlowGroupClipboard.h"
#include "services/FlowsheetCalculationService.h"
#include "topology/OpenCircuitCalculator.h"
#include "topology/TopologyAlgorithms.h"
#include "topology/TopologyValidator.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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
    upper->products().at(0)->setTerminalLength(333.0);

    FlowsheetDocument sourceDocument;
    sourceDocument.setDryMass("upper-unused", 12.5);
    sourceDocument.setGradePercent("upper-unused", 3.25);
    sourceDocument.setComponents({{"component-1", "Cu"}, {"component-2", "Zn"}});
    sourceDocument.setGradePercent("upper-unused", "component-2", 1.75);
    sourceDocument.setProductName("10:left", "最终精矿");
    sourceDocument.setExportStreamOrder({"10:right", "10:left", "10:feed"});
    sourceDocument.setAnnotationTextSettings(
        {14, true, QColor("#336699"), MassUnit::Custom, "dry short ton"});
    auto annotationSettings = sourceDocument.annotationTextSettings();
    annotationSettings.showProductName = true;
    sourceDocument.setAnnotationTextSettings(annotationSettings);
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
    note.noteColor = QColor("#a04050");
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
    sourceDocument.setCalculationMode(CalculationMode::DataReconciliation);
    sourceDocument.setDryMassStdDev("10:left", 2.5);
    sourceDocument.setGradeStdDev("10:left", DefaultComponentId, 0.08);
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
    if (loadedDocument.calculationMode() != CalculationMode::DataReconciliation
        || loadedDocument.measurement("10:left").dryMassStdDev != 2.5
        || loadedDocument.measurement("10:left").gradeStdDev(DefaultComponentId) != 0.08)
        return 49;

    // Legacy projects can use numeric unit IDs, nested position coordinates
    // and omit dimensions that were implicit in early canvas prototypes.
    const QString legacyPath = directory.filePath("legacy-geometry.afs.json");
    const QJsonObject legacyProject{{"format", "AutoFlotationSheet"}, {"version", 1},
        {"units", QJsonArray{QJsonObject{{"id", 7},
            {"position", QJsonObject{{"x", "120"}, {"y", "-40"}}}}}},
        {"connections", QJsonArray{}}, {"productMerges", QJsonArray{}},
        {"feedJunctions", QJsonArray{}}, {"measurements", QJsonArray{}},
        {"annotations", QJsonArray{}}};
    QFile legacyFile(legacyPath);
    if (!legacyFile.open(QIODevice::WriteOnly)
        || legacyFile.write(QJsonDocument(legacyProject).toJson()) < 0) return 68;
    legacyFile.close();
    FlowsheetScene legacyLoaded;
    FlowsheetDocument legacyDocument;
    if (!ProjectSerializer::load(legacyLoaded, legacyDocument, legacyPath, &error)) return 69;
    FlotationUnitItem* legacyUnit = nullptr;
    for (auto* item : legacyLoaded.items())
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item); unit) legacyUnit = unit;
    if (!legacyUnit || legacyUnit->unit().id != "7"
        || legacyUnit->unit().position != QPointF(120, -40)
        || legacyUnit->unit().width != 360.0 || legacyUnit->unit().bodyHeight != 150.0) return 70;

    // Allows an explicitly supplied real-world project to be checked by the
    // complete reader/restorer pipeline without making a developer-local path
    // part of the normal test fixture.
    const QString externalProject = qEnvironmentVariable("AFS_PROJECT_IO_EXTERNAL_PATH");
    if (!externalProject.isEmpty()) {
        FlowsheetScene externalScene;
        FlowsheetDocument externalDocument;
        if (!ProjectSerializer::load(externalScene, externalDocument, externalProject, &error)) {
            qWarning().noquote() << error;
            return 71;
        }
        if (externalScene.items().isEmpty()) return 72;
        const QString externalRoundTrip = directory.filePath("external-roundtrip.afs.json");
        if (!ProjectSerializer::save(externalScene, externalDocument, externalRoundTrip, &error)) {
            qWarning().noquote() << error;
            return 73;
        }
        FlowsheetScene roundTrippedScene;
        FlowsheetDocument roundTrippedDocument;
        if (!ProjectSerializer::load(
                roundTrippedScene, roundTrippedDocument, externalRoundTrip, &error)) {
            qWarning().noquote() << error;
            return 74;
        }
    }
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
            > 0.001
        || !loadedUpper->products().at(0)->terminalLengthOverride()
        || std::abs(loadedUpper->products().at(0)->terminalLength() - 333.0) > 0.001)
        return 35;
    if (std::abs(loadedUpper->unit().position.x() + 120) > 0.001
        || std::abs(loadedUpper->unit().width - 420) > 0.001
        || std::abs(loadedUpper->unit().bodyHeight - 170) > 0.001) return 9;
    const auto measurement = loadedDocument.measurement("upper-unused");
    if (!measurement.dryMass || !measurement.grade(DefaultComponentId)
        || std::abs(*measurement.dryMass - 12.5) > 0.001
        || std::abs(*measurement.grade(DefaultComponentId) - 3.25) > 0.001) return 10;
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
        || !loadedNote.noteBold || loadedNote.noteBorderVisible
        || loadedNote.noteColor != QColor("#a04050")) return 28;
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
        || loadedTextStyle.color != QColor("#336699")
        || loadedTextStyle.massUnit != MassUnit::Custom
        || loadedTextStyle.customMassUnit != "dry short ton"
        || !loadedTextStyle.showProductName) return 22;

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

    FlowsheetScene magneticSource;
    FlotationUnit magneticUnit{"magnetic", {180, 120}};
    magneticUnit.kind = UnitKind::MagneticSeparation;
    magneticSource.addItem(new FlotationUnitItem(magneticUnit));
    FlowsheetDocument magneticDocument;
    const QString magneticPath = directory.filePath("magnetic.afs.json");
    if (!ProjectSerializer::save(magneticSource, magneticDocument, magneticPath, &error)) return 50;
    FlowsheetScene magneticLoaded;
    FlowsheetDocument magneticLoadedDocument;
    if (!ProjectSerializer::load(magneticLoaded, magneticLoadedDocument, magneticPath, &error)) return 51;
    FlotationUnitItem* loadedMagnetic = nullptr;
    for (auto* item : magneticLoaded.items())
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item);
            unit && unit->unit().id == "magnetic") loadedMagnetic = unit;
    const auto magneticSnapshot = CanvasTopologyBuilder::build(magneticLoaded);
    if (!loadedMagnetic || loadedMagnetic->unit().kind != UnitKind::MagneticSeparation
        || loadedMagnetic->products().size() != 2
        || magneticSnapshot.interestObjects.isEmpty()
        || !magneticSnapshot.interestObjects.front().displayName.contains("磁选机")) return 52;

    FlowsheetScene strongMagneticSource;
    FlotationUnit strongMagneticUnit{"strong-magnetic", {220, 150}};
    strongMagneticUnit.kind = UnitKind::StrongMagneticSeparation;
    strongMagneticSource.addItem(new FlotationUnitItem(strongMagneticUnit));
    const QString strongMagneticPath = directory.filePath("strong-magnetic.afs.json");
    if (!ProjectSerializer::save(strongMagneticSource, magneticDocument,
                                 strongMagneticPath, &error)) return 53;
    FlowsheetScene strongMagneticLoaded;
    FlowsheetDocument strongMagneticLoadedDocument;
    if (!ProjectSerializer::load(strongMagneticLoaded, strongMagneticLoadedDocument,
                                 strongMagneticPath, &error)) return 54;
    const auto strongMagneticSnapshot = CanvasTopologyBuilder::build(strongMagneticLoaded);
    FlotationUnitItem* loadedStrongMagnetic = nullptr;
    for (auto* item : strongMagneticLoaded.items())
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item);
            unit && unit->unit().id == "strong-magnetic") loadedStrongMagnetic = unit;
    if (!loadedStrongMagnetic
        || loadedStrongMagnetic->unit().kind != UnitKind::StrongMagneticSeparation
        || strongMagneticSnapshot.interestObjects.isEmpty()
        || !strongMagneticSnapshot.interestObjects.front().displayName.contains("强磁选机")) return 55;

    FlowsheetScene poolSource;
    FlotationUnit tailingsPool{"tailings-pool", {0, 0}};
    tailingsPool.kind = UnitKind::TailingsPool;
    tailingsPool.poolLabel = QStringLiteral("尾矿浓缩池");
    FlotationUnit concentratePool{"concentrate-pool", {360, 0}};
    concentratePool.kind = UnitKind::ConcentratePool;
    concentratePool.poolLabel = QStringLiteral("精矿缓冲池");
    poolSource.addItem(new FlotationUnitItem(tailingsPool));
    poolSource.addItem(new FlotationUnitItem(concentratePool));
    const QString poolPath = directory.filePath("pools.afs.json");
    if (!ProjectSerializer::save(poolSource, magneticDocument, poolPath, &error)) return 56;
    FlowsheetScene poolLoaded;
    FlowsheetDocument poolLoadedDocument;
    if (!ProjectSerializer::load(poolLoaded, poolLoadedDocument, poolPath, &error)) return 57;
    FlotationUnitItem* loadedTailingsPool = nullptr;
    FlotationUnitItem* loadedConcentratePool = nullptr;
    for (auto* item : poolLoaded.items()) {
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item); unit) {
            if (unit->unit().id == "tailings-pool") loadedTailingsPool = unit;
            if (unit->unit().id == "concentrate-pool") loadedConcentratePool = unit;
        }
    }
    if (!loadedTailingsPool || !loadedConcentratePool
        || loadedTailingsPool->unit().kind != UnitKind::TailingsPool
        || loadedTailingsPool->unit().poolLabel != QStringLiteral("尾矿浓缩池")
        || loadedTailingsPool->products().size() != 0
        || loadedConcentratePool->unit().kind != UnitKind::ConcentratePool
        || loadedConcentratePool->unit().poolLabel != QStringLiteral("精矿缓冲池")
        || loadedConcentratePool->products().size() != 1
        || loadedConcentratePool->products().front()->side() != ProductSide::Right) return 58;

    FlowsheetScene gravitySource;
    FlotationUnit spiral{"spiral", {0, 0}};
    spiral.kind = UnitKind::SpiralChute;
    FlotationUnit shakingTable{"shaking-table", {180, 0}};
    shakingTable.kind = UnitKind::ShakingTable;
    FlotationUnit cyclone{"cyclone", {360, 0}};
    cyclone.kind = UnitKind::DenseMediumCyclone;
    FlotationUnit sedimentationTank{"sedimentation-tank", {540, 0}};
    sedimentationTank.kind = UnitKind::SedimentationTank;
    FlotationUnit screening{"screening", {720, 0}};
    screening.kind = UnitKind::Screening;
    FlotationUnit classifier{"classifier", {1080, 0}};
    classifier.kind = UnitKind::Classification;
    FlotationUnit demedium{"demedium", {1440, 0}};
    demedium.kind = UnitKind::DemediumScreen;
    FlotationUnit mediaTank{"media-tank", {1800, 0}};
    mediaTank.kind = UnitKind::MediaTank;
    FlotationUnit mixingTank{"mixing-tank", {2160, 0}};
    mixingTank.kind = UnitKind::MixingTank;
    gravitySource.addItem(new FlotationUnitItem(spiral));
    gravitySource.addItem(new FlotationUnitItem(shakingTable));
    gravitySource.addItem(new FlotationUnitItem(cyclone));
    gravitySource.addItem(new FlotationUnitItem(sedimentationTank));
    gravitySource.addItem(new FlotationUnitItem(screening));
    gravitySource.addItem(new FlotationUnitItem(classifier));
    gravitySource.addItem(new FlotationUnitItem(demedium));
    gravitySource.addItem(new FlotationUnitItem(mediaTank));
    gravitySource.addItem(new FlotationUnitItem(mixingTank));
    const QString gravityPath = directory.filePath("gravity.afs.json");
    if (!ProjectSerializer::save(gravitySource, magneticDocument, gravityPath, &error)) return 59;
    FlowsheetScene gravityLoaded;
    FlowsheetDocument gravityLoadedDocument;
    if (!ProjectSerializer::load(gravityLoaded, gravityLoadedDocument, gravityPath, &error)) return 60;
    bool loadedSpiral = false;
    bool loadedShakingTable = false;
    bool loadedCyclone = false;
    bool loadedSedimentationTank = false;
    bool loadedScreening = false;
    bool loadedClassification = false;
    bool loadedDemedium = false;
    FlotationUnitItem* loadedMediaTank = nullptr;
    FlotationUnitItem* loadedMixingTank = nullptr;
    for (auto* item : gravityLoaded.items()) {
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item); unit) {
            loadedSpiral = loadedSpiral || unit->unit().kind == UnitKind::SpiralChute;
            loadedShakingTable = loadedShakingTable || unit->unit().kind == UnitKind::ShakingTable;
            loadedCyclone = loadedCyclone || unit->unit().kind == UnitKind::DenseMediumCyclone;
            loadedSedimentationTank = loadedSedimentationTank
                || unit->unit().kind == UnitKind::SedimentationTank;
            loadedScreening = loadedScreening || unit->unit().kind == UnitKind::Screening;
            loadedClassification = loadedClassification
                || unit->unit().kind == UnitKind::Classification;
            loadedDemedium = loadedDemedium || unit->unit().kind == UnitKind::DemediumScreen;
            if (unit->unit().kind == UnitKind::MediaTank) loadedMediaTank = unit;
            if (unit->unit().kind == UnitKind::MixingTank) loadedMixingTank = unit;
        }
    }
    if (!loadedSpiral || !loadedShakingTable || !loadedCyclone || !loadedSedimentationTank || !loadedScreening || !loadedClassification
        || !loadedDemedium || !loadedMediaTank || loadedMediaTank->products().size() != 1
        || loadedMediaTank->products().front()->side() != ProductSide::Right
        || !loadedMixingTank || loadedMixingTank->products().size() != 1
        || loadedMixingTank->products().front()->side() != ProductSide::Right) return 61;

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

    FlowsheetScene threeScreenSource;
    FlotationUnit threeScreenUnit{"three-screen", {240, 160}};
    threeScreenUnit.kind = UnitKind::ThreeProductScreening;
    threeScreenSource.addItem(new FlotationUnitItem(threeScreenUnit));
    const QString threeScreenPath = directory.filePath("three-product-screen.afs.json");
    if (!ProjectSerializer::save(
            threeScreenSource, threeProductDocument, threeScreenPath, &error)) return 62;
    FlowsheetScene threeScreenLoaded;
    FlowsheetDocument threeScreenLoadedDocument;
    if (!ProjectSerializer::load(threeScreenLoaded, threeScreenLoadedDocument,
                                 threeScreenPath, &error)) return 63;
    FlotationUnitItem* loadedThreeScreen = nullptr;
    for (auto* item : threeScreenLoaded.items())
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item);
            unit && unit->unit().id == "three-screen") loadedThreeScreen = unit;
    const auto threeScreenSnapshot = CanvasTopologyBuilder::build(threeScreenLoaded);
    if (!loadedThreeScreen
        || loadedThreeScreen->unit().kind != UnitKind::ThreeProductScreening
        || loadedThreeScreen->products().size() != 3
        || threeScreenSnapshot.graph.streamsFrom(
               "three-screen", topology::PortKind::MiddleProduct).size() != 1
        || threeScreenSnapshot.requiredMeasurements.size() != 3) return 64;

    FlowsheetScene threeDemediumSource;
    FlotationUnit threeDemediumUnit{"three-demedium", {240, 160}};
    threeDemediumUnit.kind = UnitKind::ThreeProductDemediumScreen;
    threeDemediumSource.addItem(new FlotationUnitItem(threeDemediumUnit));
    const QString threeDemediumPath = directory.filePath("three-product-demedium.afs.json");
    if (!ProjectSerializer::save(
            threeDemediumSource, threeProductDocument, threeDemediumPath, &error)) return 65;
    FlowsheetScene threeDemediumLoaded;
    FlowsheetDocument threeDemediumLoadedDocument;
    if (!ProjectSerializer::load(threeDemediumLoaded, threeDemediumLoadedDocument,
                                 threeDemediumPath, &error)) return 66;
    FlotationUnitItem* loadedThreeDemedium = nullptr;
    for (auto* item : threeDemediumLoaded.items())
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item);
            unit && unit->unit().id == "three-demedium") loadedThreeDemedium = unit;
    const auto threeDemediumSnapshot = CanvasTopologyBuilder::build(threeDemediumLoaded);
    if (!loadedThreeDemedium
        || loadedThreeDemedium->unit().kind != UnitKind::ThreeProductDemediumScreen
        || loadedThreeDemedium->products().size() != 3
        || threeDemediumSnapshot.graph.streamsFrom(
               "three-demedium", topology::PortKind::MiddleProduct).size() != 1
        || threeDemediumSnapshot.requiredMeasurements.size() != 3) return 67;

    FlowsheetScene undoScene;
    FlowsheetDocument undoDocument;
    undoScene.addItem(new FlotationUnitItem({"undo-a", {0, 0}}));
    ProjectUndoManager undoManager(undoScene, undoDocument, {}, {});
    undoManager.initialize();
    undoScene.addItem(new FlotationUnitItem({"undo-b", {300, 200}}));
    undoDocument.setDryMass("undo-a:left", 25.0);
    undoManager.scheduleCheckpoint("test edit");
    undoManager.undo();
    const auto unitCount = [&undoScene] {
        int count = 0;
        for (auto* item : undoScene.items())
            if (dynamic_cast<FlotationUnitItem*>(item)) ++count;
        return count;
    };
    if (unitCount() != 1 || undoDocument.measurement("undo-a:left").dryMass) return 47;
    undoManager.redo();
    if (unitCount() != 2
        || undoDocument.measurement("undo-a:left").dryMass != std::optional<double>(25.0))
        return 48;

    // Manual merge corridors use scene coordinates. A copied group must offset
    // them with its cloned units, and must retain that relationship when the
    // whole clone is subsequently dragged.
    FlowsheetScene clipboardScene;
    auto* copyA = new FlotationUnitItem({"copy-a", {0, 0}});
    auto* copyB = new FlotationUnitItem({"copy-b", {300, 140}});
    auto* copyC = new FlotationUnitItem({"copy-c", {650, 340}});
    clipboardScene.addItem(copyA);
    clipboardScene.addItem(copyB);
    clipboardScene.addItem(copyC);
    auto* copyMerge = clipboardScene.mergeProducts(
        copyA->products().front(), copyB->products().front(), "copy-merge");
    if (!copyMerge || !clipboardScene.connectMergeDirect(copyMerge, copyC->inputLine())) return 75;
    copyMerge->setManualMergeY(480.0);
    copyA->setSelected(true); copyB->setSelected(true); copyC->setSelected(true);
    FlowsheetDocument clipboardDocument;
    FlowGroupClipboard clipboard;
    if (clipboard.copy(clipboardScene, clipboardDocument).merges != 1) return 76;
    int cloneNumber = 0;
    if (clipboard.paste(clipboardScene, clipboardDocument,
                        [&cloneNumber] { return QString("clone-%1").arg(++cloneNumber); }).merges != 1)
        return 77;
    FlotationUnitItem* cloneA = nullptr;
    FlotationUnitItem* cloneB = nullptr;
    FlotationUnitItem* cloneC = nullptr;
    for (auto* item : clipboardScene.items()) {
        auto* unit = dynamic_cast<FlotationUnitItem*>(item);
        if (!unit) continue;
        if (unit->unit().id == "clone-1") cloneA = unit;
        else if (unit->unit().id == "clone-2") cloneB = unit;
        else if (unit->unit().id == "clone-3") cloneC = unit;
    }
    if (!cloneA || !cloneB || !cloneC) return 78;
    auto* cloneMerge = cloneA->products().front()->mergeJunction();
    if (!cloneMerge || !cloneMerge->manualMergeY()
        || std::abs(*cloneMerge->manualMergeY() - 520.0) > 0.001) return 79;
    const QPointF dragDelta(70.0, -90.0);
    cloneA->setPos(cloneA->pos() + dragDelta);
    cloneB->setPos(cloneB->pos() + dragDelta);
    cloneC->setPos(cloneC->pos() + dragDelta);
    QCoreApplication::processEvents();
    if (!cloneMerge->manualMergeY()
        || std::abs(*cloneMerge->manualMergeY() - 430.0) > 0.001) return 80;

    FlowsheetScene exampleScene;
    FlowsheetDocument exampleDocument;
    QDir exampleRoot(QCoreApplication::applicationDirPath());
    exampleRoot.cdUp();
    exampleRoot.cdUp();
    if (!ProjectSerializer::load(exampleScene, exampleDocument,
                                 exampleRoot.filePath(
                                     QStringLiteral("example/DataBalanceJSOPENLOOP.repaired.afs.json")),
                                 &error)) {
        qCritical() << "example load failed:" << error;
        return 81;
    }
    const auto exampleSnapshot = CanvasTopologyBuilder::build(exampleScene);
    const auto exampleFeeds = exampleSnapshot.graph.externalFeedStreams();
    if (exampleFeeds.size() != 1 || exampleFeeds.front() != QStringLiteral("12:feed")) return 82;
    const auto exampleResult = FlowsheetCalculationService::calculate(exampleSnapshot, exampleDocument);
    if (!exampleResult.values.contains(QStringLiteral("12:feed"))) return 84;
    // The example intentionally has some unmeasured terminal streams, so its
    // feed is not expected to be fully solved.  It must nevertheless have no
    // phantom fresh-feed boundaries at its source-only multi-input junctions.
    for (const auto& issue : exampleResult.issues)
        if (issue.code == topology::IssueCode::Underdetermined
            && issue.objectId.contains(QStringLiteral(":external-feed"))) return 83;
    return 0;
}
