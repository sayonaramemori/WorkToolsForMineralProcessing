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
#include "topology/TopologyAlgorithms.h"
#include <QFile>
#include <QTemporaryDir>
#include <cmath>

namespace afs::tests {

int checkProjectRoundTrip(const QTemporaryDir& directory) {
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

    if (const int result = checkProjectFormat(source, sourceDocument, loaded, loadedDocument)) return result;
    if (const int result = checkExternalProject(directory)) return result;

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
    return 0;
}

} // namespace afs::tests
