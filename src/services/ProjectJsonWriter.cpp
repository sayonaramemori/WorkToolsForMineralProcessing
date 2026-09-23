#include "services/ProjectJsonWriter.h"
#include "annotations/AnnotationTypes.h"
#include "document/FlowsheetDocument.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FeedJunctionItem.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

namespace afs {
namespace {
constexpr int kFormatVersion = 23;
QString unitKindKey(UnitKind kind) {
    switch (kind) {
    case UnitKind::Flotation: return "flotation";
    case UnitKind::MagneticSeparation: return "magnetic-separation";
    case UnitKind::WeakMagneticSeparation: return "weak-magnetic-separation";
    case UnitKind::StrongMagneticSeparation: return "strong-magnetic-separation";
    case UnitKind::SpiralChute: return "spiral-chute";
    case UnitKind::ShakingTable: return "shaking-table";
    case UnitKind::DenseMediumCyclone: return "dense-medium-cyclone";
    case UnitKind::SedimentationTank: return "sedimentation-tank";
    case UnitKind::DemediumScreen: return "demedium-screen";
    case UnitKind::Screening: return "screening";
    case UnitKind::Classification: return "classification";
    case UnitKind::TailingsPool: return "tailings-pool";
    case UnitKind::ConcentratePool: return "concentrate-pool";
    case UnitKind::WaterPool: return "water-pool";
    case UnitKind::MediaTank: return "media-tank";
    case UnitKind::MixingTank: return "mixing-tank";
    case UnitKind::BinarySplitter: return "binary-splitter";
    case UnitKind::ThreeProductFlotation: return "three-product-flotation";
    case UnitKind::ThreeProductScreening: return "three-product-screening";
    case UnitKind::ThreeProductDemediumScreen: return "three-product-demedium-screen";
    }
    return {};
}
QJsonObject measurementObject(const QString& streamId, const StreamMeasurement& value) {
    QJsonObject object{{"streamId", streamId}};
    object.insert("dryMass", value.dryMass ? QJsonValue(*value.dryMass) : QJsonValue::Null);
    object.insert("dryMassSharePercent", value.dryMassSharePercent ? QJsonValue(*value.dryMassSharePercent) : QJsonValue::Null);
    object.insert("dryMassStdDev", value.dryMassStdDev ? QJsonValue(*value.dryMassStdDev) : QJsonValue::Null);
    QJsonObject grades;
    for (auto it = value.components.cbegin(); it != value.components.cend(); ++it)
        if (it->gradePercent) grades.insert(it.key(), *it->gradePercent);
    object.insert("gradePercents", grades);
    QJsonObject gradeStdDevs;
    for (auto it = value.components.cbegin(); it != value.components.cend(); ++it)
        if (it->gradeStdDev) gradeStdDevs.insert(it.key(), *it->gradeStdDev);
    object.insert("gradeStdDevs", gradeStdDevs);
    QJsonObject shares;
    for (auto it = value.components.cbegin(); it != value.components.cend(); ++it)
        if (it->sharePercent) shares.insert(it.key(), *it->sharePercent);
    object.insert("componentSharePercents", shares);
    return object;
}
}

QByteArray ProjectJsonWriter::serialize(const FlowsheetScene& scene,
                                        const FlowsheetDocument& document) {
    QVector<FlotationUnitItem*> units;
    QVector<MergeJunctionItem*> merges;
    QVector<FeedJunctionItem*> feeds;
    for (auto* item : scene.items()) {
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item)) units.append(unit);
        else if (auto* merge = dynamic_cast<MergeJunctionItem*>(item)) merges.append(merge);
        else if (auto* feed = dynamic_cast<FeedJunctionItem*>(item)) feeds.append(feed);
    }
    std::sort(units.begin(), units.end(), [](auto* a, auto* b) { return a->unit().id < b->unit().id; });
    std::sort(merges.begin(), merges.end(), [](auto* a, auto* b) { return a->id() < b->id(); });
    std::sort(feeds.begin(), feeds.end(), [](auto* a, auto* b) { return a->id() < b->id(); });

    QJsonArray unitArray;
    QJsonArray connectionArray;
    for (auto* unit : units) {
        const auto& value = unit->unit();
        QJsonObject unitObject{{"id", value.id}, {"x", value.position.x()},
            {"y", value.position.y()}, {"width", value.width}, {"bodyHeight", value.bodyHeight},
            {"kind", unitKindKey(value.kind)}};
        if (value.kind == UnitKind::BinarySplitter)
            unitObject.insert("leftSplitPercent", value.leftSplitPercent);
        if (isStoragePool(value.kind) && !value.poolLabel.isEmpty())
            unitObject.insert("poolLabel", value.poolLabel);
        QJsonObject terminalLengths;
        for (auto* product : unit->products())
            if (product->terminalLengthOverride())
                terminalLengths.insert(productSideSuffix(product->side()).mid(1),
                                       *product->terminalLengthOverride());
        if (!terminalLengths.isEmpty()) unitObject.insert("terminalLengths", terminalLengths);
        unitArray.append(unitObject);
        for (auto* product : unit->products()) {
            if (product->targetUnit()) {
                QJsonObject connection{{"source", product->streamId()},
                                       {"targetUnit", product->targetUnit()->unit().id}};
                if (product->manualRouteY()) connection.insert("routeY", *product->manualRouteY());
                connectionArray.append(connection);
            }
        }
    }
    QJsonArray mergeArray;
    for (auto* merge : merges) {
        QJsonArray branches;
        for (auto* product : merge->products()) branches.append(product->streamId());
        QJsonObject object{{"id", merge->id()}, {"branches", branches}};
        if (merge->targetUnit()) object.insert("targetUnit", merge->targetUnit()->unit().id);
        if (merge->manualMergeY()) object.insert("mergeY", *merge->manualMergeY());
        mergeArray.append(object);
    }
    QJsonArray feedArray;
    for (auto* feed : feeds) {
        QJsonArray sources;
        for (auto* product : feed->recycleProducts()) {
            QJsonObject source{{"type", "product"}, {"source", product->streamId()}};
            if (feed->manualRouteXs().contains(product->streamId()))
                source.insert("routeX", feed->manualRouteXs().value(product->streamId()));
            if (feed->manualRouteYs().contains(product->streamId()))
                source.insert("routeY", feed->manualRouteYs().value(product->streamId()));
            sources.append(source);
        }
        for (auto* merge : feed->recycleMerges()) {
            QJsonObject source{{"type", "merge"}, {"source", merge->id()}};
            if (feed->manualRouteXs().contains(merge->outputStreamId()))
                source.insert("routeX", feed->manualRouteXs().value(merge->outputStreamId()));
            if (feed->manualRouteYs().contains(merge->outputStreamId()))
                source.insert("routeY", feed->manualRouteYs().value(merge->outputStreamId()));
            sources.append(source);
        }
        QJsonObject object{{"id", feed->id()}, {"sources", sources},
                           {"targetUnit", feed->targetUnit()->unit().id},
                           {"hasExternalFeed", feed->hasExternalFeed()}};
        if (feed->processProduct()) {
            object.insert("processSourceType", "product");
            object.insert("processSource", feed->processProduct()->streamId());
        } else if (feed->processMerge()) {
            object.insert("processSourceType", "merge");
            object.insert("processSource", feed->processMerge()->id());
        }
        feedArray.append(object);
    }
    QJsonArray measurementArray;
    QStringList measurementIds = document.measurements().keys();
    std::sort(measurementIds.begin(), measurementIds.end());
    for (const auto& id : measurementIds)
        measurementArray.append(measurementObject(id, document.measurements().value(id)));
    QJsonArray annotationArray;
    QStringList annotationIds = document.annotationRecords().keys();
    std::sort(annotationIds.begin(), annotationIds.end());
    for (const auto& id : annotationIds) {
        const auto value = document.annotationRecords().value(id);
        if (value.kind == AnnotationKind::Reagent) continue;
        annotationArray.append(QJsonObject{{"id", value.id},
            {"kind", static_cast<int>(value.kind)}, {"style", static_cast<int>(value.style)},
            {"ownerKind", static_cast<int>(value.ownerKind)}, {"ownerId", value.ownerId},
            {"offsetX", value.manualOffset.x()}, {"offsetY", value.manualOffset.y()},
            {"manuallyPlaced", value.manuallyPlaced}, {"visible", value.visible},
            {"text", value.text}, {"dosage", value.dosage},
            {"dosageUnit", value.dosageUnit}, {"note", value.note},
            {"noteFontFamily", value.noteFontFamily},
            {"notePointSize", value.notePointSize}, {"noteBold", value.noteBold},
            {"noteBorderVisible", value.noteBorderVisible},
            {"noteColor", value.noteColor.isValid()
                ? value.noteColor.name(QColor::HexArgb) : QString{}}});
    }
    QJsonArray scenarioArray;
    for (const auto& scenario : document.scenarios()) {
        QJsonArray scenarioMeasurements;
        QStringList ids = scenario.measurements.keys(); std::sort(ids.begin(), ids.end());
        for (const auto& id : ids)
            scenarioMeasurements.append(measurementObject(id, scenario.measurements.value(id)));
        QJsonArray reagents;
        QStringList reagentIds = scenario.reagentAnnotations.keys();
        std::sort(reagentIds.begin(), reagentIds.end());
        for (const auto& id : reagentIds) {
            const auto& record = scenario.reagentAnnotations[id];
            reagents.append(QJsonObject{{"id", record.id}, {"ownerId", record.ownerId},
                {"text", record.text}, {"dosage", record.dosage},
                {"unit", record.dosageUnit}, {"note", record.note},
                {"offsetY", record.manualOffset.y()},
                {"manuallyPlaced", record.manuallyPlaced}});
        }
        scenarioArray.append(QJsonObject{{"id", scenario.id}, {"name", scenario.name},
            {"calculationMode", scenario.calculationMode == CalculationMode::DataReconciliation
                ? "reconciliation" : "strict"},
            {"calculated", scenario.calculationResult.has_value()},
            {"measurements", scenarioMeasurements}, {"reagents", reagents}});
    }
    QJsonArray productNameArray;
    QStringList namedStreamIds = document.productNames().keys();
    std::sort(namedStreamIds.begin(), namedStreamIds.end());
    for (const auto& id : namedStreamIds)
        productNameArray.append(QJsonObject{{"streamId", id},
                                            {"name", document.productNames().value(id)}});
    const auto& textSettings = document.annotationTextSettings();
    QJsonArray componentArray;
    for (const auto& component : document.components())
        componentArray.append(QJsonObject{{"id", component.id}, {"name", component.name}});
    QJsonArray exportOrderArray;
    for (const auto& streamId : document.exportStreamOrder()) exportOrderArray.append(streamId);
    const QJsonObject annotationTextStyleObject{
        {"pointSize", textSettings.pointSize}, {"bold", textSettings.bold},
        {"color", textSettings.color.isValid() ? textSettings.color.name(QColor::HexArgb)
                                                 : QString()},
        {"massUnit", textSettings.massUnit == MassUnit::Custom
             ? textSettings.customMassUnit : massUnitSymbol(textSettings.massUnit)},
        {"resultAnnotationsVisible", textSettings.resultAnnotationsVisible},
        {"showDryMass", textSettings.showDryMass}, {"showGrade", textSettings.showGrade},
        {"showOverallYield", textSettings.showOverallYield},
        {"showOverallRecovery", textSettings.showOverallRecovery},
        {"showProductName", textSettings.showProductName},
        {"metricLabelMode", textSettings.metricLabelMode == MetricLabelMode::Symbols
             ? "symbols" : "chinese"}};
    const QJsonObject root{{"format", "AutoFlotationSheet"}, {"version", kFormatVersion},
        {"units", unitArray}, {"connections", connectionArray},
        {"productMerges", mergeArray}, {"feedJunctions", feedArray},
        {"measurements", measurementArray}, {"productNames", productNameArray},
        {"components", componentArray},
        {"exportStreamOrder", exportOrderArray},
        {"annotationTextStyle", annotationTextStyleObject},
        {"scenarios", scenarioArray}, {"currentScenario", document.currentScenarioIndex()},
        {"annotations", annotationArray}};

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

} // namespace afs
