#include "app/MainWindow.h"
#include "adapters/CanvasTopologyBuilder.h"
#include "annotations/AnnotationManager.h"
#include "annotations/ReagentAnnotationItem.h"
#include "document/FlowsheetDocument.h"
#include "editor/CanvasView.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FeedJunctionItem.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"
#include "services/FlowGroupClipboard.h"
#include "services/ProjectSerializer.h"
#include "services/ProjectUndoManager.h"

#include <QApplication>
#include <QDebug>
#include <QTest>
#include <stdexcept>

using namespace afs;

namespace {
void require(bool condition, const QString& message) {
    if (!condition) throw std::runtime_error(message.toStdString());
}

AnnotationRecord reagent(QString id, QString stream, QString text) {
    AnnotationRecord record{std::move(id), AnnotationKind::Reagent, AnnotationStyle::PlainText,
        AnnotationOwnerKind::Stream, std::move(stream), {0, 16}, true, true};
    record.text = std::move(text); record.dosage = "120";
    record.dosageUnit = "g/t"; record.note = "试验备注";
    return record;
}

QVector<AnnotationRecord> reagents(const FlowsheetDocument& document) {
    QVector<AnnotationRecord> result;
    for (const auto& record : document.annotationRecords())
        if (record.kind == AnnotationKind::Reagent) result.append(record);
    return result;
}

ReagentAnnotationItem* graphics(FlowsheetScene& scene, const QString& id) {
    for (auto* item : scene.items())
        if (auto* annotation = dynamic_cast<ReagentAnnotationItem*>(item);
            annotation && annotation->record().id == id) return annotation;
    return nullptr;
}

void expectRecord(const AnnotationRecord& source, const AnnotationRecord& copy, const QString& owner) {
    require(!copy.id.isEmpty() && copy.id != source.id && copy.ownerId == owner
        && copy.kind == source.kind && copy.ownerKind == source.ownerKind && copy.style == source.style
        && copy.text == source.text && copy.dosage == source.dosage && copy.dosageUnit == source.dosageUnit
        && copy.note == source.note && copy.manualOffset == source.manualOffset
        && copy.manuallyPlaced == source.manuallyPlaced && copy.visible == source.visible,
        "Copied reagent lost its fields, owner or relative offset");
}

void shortcut(CanvasView& canvas, Qt::Key key) {
    canvas.setFocus();
    QTest::keyClick(canvas.viewport(), key, Qt::ControlModifier);
    QApplication::processEvents();
}

QString selectedUnitId(const FlowsheetScene& scene) {
    for (auto* item : scene.selectedItems())
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item)) return unit->unit().id;
    throw std::runtime_error("Pasted unit is not selected");
}

void testShortcuts(const QString& directory) {
    qInfo() << "Group copy includes unselected reagents, repeated paste and stacking order";
    MainWindow window;
    window.resize(1600, 1100); window.show(); window.activateWindow();
    QApplication::processEvents();
    auto* canvas = dynamic_cast<CanvasView*>(window.centralWidget());
    auto* scene = qobject_cast<FlowsheetScene*>(canvas->scene());
    auto* document = window.findChild<FlowsheetDocument*>();
    auto* manager = window.findChild<AnnotationManager*>();
    auto* undo = window.findChild<ProjectUndoManager*>();
    FlotationUnitItem* source = nullptr;
    for (auto* item : scene->items())
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item)) source = unit;
    require(source, "Missing initial unit");
    const QString sourceId = source->unit().id;
    auto* outside = new FlotationUnitItem({"outside", {700, 400}});
    scene->addItem(outside); scene->notifyTopologyChanged();
    QVector<AnnotationRecord> originals;
    for (int index = 1; index <= 12; ++index)
        originals.append(reagent(QString("reagent-%1").arg(index), sourceId + ":left",
                                  QString("药剂 %1").arg(index)));
    originals.append(reagent("root-feed", sourceId + ":feed", "入料药剂"));
    originals.append(reagent("right-product", sourceId + ":right", "右产品药剂"));
    for (const auto& record : originals) document->setAnnotationRecord(record);
    // Occupy a generated-ID prefix, too; paste must never overwrite it.
    const auto unrelated = reagent("reagent-copy-1-01", "outside:feed", "外部流程药剂");
    document->setAnnotationRecord(unrelated); manager->synchronize();
    QHash<QString, QPointF> positions;
    for (const auto& record : originals) {
        require(graphics(*scene, record.id), "Source reagent was not rendered");
        positions.insert(record.text, graphics(*scene, record.id)->pos());
    }
    scene->clearSelection(); canvas->centerOn(source); QApplication::processEvents();
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::NoModifier,
                      canvas->mapFromScene(source->mapToScene(QPointF(60, 12))));
    require(source->isSelected(), "Could not select source group");
    shortcut(*canvas, Qt::Key_C);
    auto edited = originals.front(); edited.text = "复制后修改源药剂";
    document->setAnnotationRecord(edited); manager->synchronize(); undo->markClean();
    const auto checkPaste = [&](const QString& newId, QPointF offset) {
        QSet<QString> copiedIds;
        for (const auto& original : originals) {
            const QString owner = newId + original.ownerId.mid(sourceId.size());
            bool found = false;
            for (const auto& copy : reagents(*document)) {
                if (copy.ownerId != owner || copy.text != original.text) continue;
                expectRecord(original, copy, owner);
                auto* item = graphics(*scene, copy.id);
                require(item && (item->pos() - positions.value(original.text) - offset).manhattanLength() < 1e-6,
                        "Pasted reagent moved twice or changed stacking order");
                copiedIds.insert(copy.id); found = true;
            }
            require(found, "Group copy omitted a reagent");
        }
        require(copiedIds.size() == originals.size(), "Pasted reagents reused IDs");
        require(document->annotationRecord(unrelated.id).text == unrelated.text,
                "Paste overwrote an unrelated annotation");
    };
    shortcut(*canvas, Qt::Key_V);
    const QString firstId = selectedUnitId(*scene);
    checkPaste(firstId, {40, 40});
    require(reagents(*document).size() == 29, "First paste copied the wrong number of reagents");
    undo->undo();
    require(reagents(*document).size() == 15, "Undo did not remove copied reagents");
    undo->redo();
    checkPaste(firstId, {40, 40});
    shortcut(*canvas, Qt::Key_V);
    const QString secondId = selectedUnitId(*scene);
    require(secondId != firstId && reagents(*document).size() == 43, "Repeated paste reused records");
    checkPaste(secondId, {80, 80});

    qInfo() << "Clipboard snapshot pastes into only the current scenario and survives project round trip";
    const int scenario = document->addScenario("另一试验方案", false);
    require(document->setCurrentScenario(scenario) && reagents(*document).isEmpty(), "Scenario did not switch");
    shortcut(*canvas, Qt::Key_V);
    const QString thirdId = selectedUnitId(*scene);
    require(reagents(*document).size() == 14
        && document->scenarios().front().reagentAnnotations.size() == 43,
        "Paste changed reagents in another scenario");
    // Other scenarios legitimately do not contain the original outside reagent.
    for (const auto& original : originals) {
        const QString owner = thirdId + original.ownerId.mid(sourceId.size());
        bool found = false;
        for (const auto& copy : reagents(*document)) if (copy.ownerId == owner && copy.text == original.text) {
            expectRecord(original, copy, owner); found = true;
        }
        require(found, "Cross-scenario paste lost clipboard snapshot");
    }
    const QString path = directory + "/reagent-copies.afs.json";
    QString error;
    require(ProjectSerializer::save(*scene, *document, path, &error), error);
    FlowsheetScene restored;
    FlowsheetDocument restoredDocument;
    require(ProjectSerializer::load(restored, restoredDocument, path, &error), error);
    require(restoredDocument.currentScenarioIndex() == scenario && reagents(restoredDocument).size() == 14
        && restoredDocument.scenarios().front().reagentAnnotations.size() == 43,
        "Project round trip lost copied scenario reagents");
    const auto graph = CanvasTopologyBuilder::build(restored).graph;
    for (const auto& record : reagents(restoredDocument))
        require(graph.stream(record.ownerId), "Copied reagent points to a missing stream");
}

void testTopologyMapping() {
    qInfo() << "Complete merges, external feed and process/recycle junctions remap reagent owners";
    FlowsheetScene scene;
    FlowsheetDocument document;
    auto* a = new FlotationUnitItem({"a", {-400, 0}});
    auto* b = new FlotationUnitItem({"b", {200, 300}});
    auto* c = new FlotationUnitItem({"c", {700, 600}});
    for (auto* unit : {a, b, c}) scene.addItem(unit);
    require(scene.connectProductDirect(a->products().back(), b->inputLine()), "Missing direct fixture link");
    auto* external = scene.connectRecycle(b->products().back(), a->inputLine(), "fresh");
    auto* merged = scene.mergeProducts(a->products().front(), b->products().front(), "merged");
    require(external && merged && scene.connectMergeDirect(merged, c->inputLine()), "Missing junction fixture");
    auto* process = scene.connectRecycle(c->products().back(), b->inputLine(), "process");
    require(process && !process->hasExternalFeed(), "Process-feed fixture must have no external inlet");
    QVector<AnnotationRecord> originals;
    const auto snapshot = CanvasTopologyBuilder::build(scene);
    for (const auto& id : snapshot.graph.streamIds()) {
        originals.append(reagent("reagent:" + id, id, "药剂:" + id));
        document.setAnnotationRecord(originals.back());
    }
    document.setAnnotationRecord(reagent("outside", "outside:feed", "外部药剂"));
    document.setAnnotationRecord(reagent("absent-feed", "process:external-feed", "不存在的外部入料"));
    for (auto* unit : {a, b, c}) unit->setSelected(true);
    FlowGroupClipboard clipboard;
    const auto copied = clipboard.copy(scene, document);
    require(copied.reagents == originals.size() && copied.merges == 1 && copied.feeds == 2,
            "Copy omitted complete group reagent owners");
    int next = 0;
    const auto pasted = clipboard.paste(scene, document, [&] { return QString("clone-%1").arg(++next); });
    require(pasted.reagents == originals.size() && pasted.merges == 1 && pasted.feeds == 2,
            "Paste omitted remapped group reagents");
    QHash<QString, QString> owners;
    for (auto* item : scene.selectedItems()) {
        auto* unit = dynamic_cast<FlotationUnitItem*>(item);
        if (!unit) continue;
        const QString oldId = unit->unit().id == "clone-1" ? "a"
            : unit->unit().id == "clone-2" ? "b" : "c";
        for (auto* product : unit->products()) owners.insert(oldId + productSideSuffix(product->side()), product->streamId());
        if (auto* feed = unit->inputLine()->feedJunction()) {
            const QString oldFeed = oldId == "a" ? "fresh" : "process";
            owners.insert(oldFeed + ":output", feed->outputStreamId());
            if (feed->hasExternalFeed()) owners.insert(oldFeed + ":external-feed", feed->externalFeedStreamId());
        }
        if (auto* merge = unit->inputLine()->sourceMerge()) owners.insert("merged:output", merge->outputStreamId());
    }
    for (const auto& original : originals) {
        require(owners.contains(original.ownerId), "Missing cloned stream mapping");
        bool found = false;
        for (const auto& copy : reagents(document)) if (copy.ownerId == owners.value(original.ownerId)) {
            expectRecord(original, copy, copy.ownerId); found = true;
        }
        require(found, "Reagent still points at original topology");
    }
    scene.clearSelection(); a->setSelected(true);
    const auto partial = clipboard.copy(scene, document);
    require(partial.reagents == 2 && partial.merges == 0 && partial.feeds == 0,
            "Partial group copied annotations from omitted junctions");
    const auto partialPaste = clipboard.paste(scene, document, [] { return QString("partial"); });
    require(partialPaste.reagents == 2, "Partial paste did not retain owned product reagents");
}
} // namespace

void runReagentClipboardChecks(const QString& directory) {
    testShortcuts(directory);
    testTopologyMapping();
}
