#include "services/FlowGroupSnapshotOperations.h"

#include "document/FlowsheetDocument.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FeedJunctionItem.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"

#include <QSet>
#include <algorithm>

namespace afs::flow_group {

PasteResult paste(const Snapshot& snapshot, const QPointF& offset,
                  FlowsheetScene& scene, FlowsheetDocument& document,
                  const std::function<QString()>& nextUnitId) {
    PasteResult result; if (!snapshot.hasData()) return result;
    QHash<QString, FlotationUnitItem*> units; QHash<QString, ProductLineItem*> products;
    QHash<QString, QString> streamIds;
    for (const auto& source : snapshot.units) {
        FlotationUnit clone = source; clone.id = nextUnitId(); clone.position += offset;
        auto* unit = new FlotationUnitItem(clone); scene.addItem(unit); units.insert(source.id, unit);
        streamIds.insert(source.id + ":feed", clone.id + ":feed");
        for (auto* product : unit->products()) {
            const auto oldStream = source.id + productSideSuffix(product->side());
            // Feed and merge routes use this endpoint even when the product
            // is connected, so restore its length before rebuilding topology.
            if (snapshot.terminalLengths.contains(oldStream))
                product->setTerminalLength(snapshot.terminalLengths.value(oldStream));
            products.insert(oldStream, product);
            streamIds.insert(oldStream, product->streamId());
        }
        ++result.units;
    }
    const auto product = [&products](const QString& oldStream) { return products.value(oldStream); };
    for (const auto& data : snapshot.direct) if (scene.connectProductDirect(product(data.source), units.value(data.target)->inputLine())) {
        product(data.source)->setManualRouteY(data.routeY); ++result.connections;
    }
    QHash<QString, MergeJunctionItem*> merges;
    for (const auto& data : snapshot.merges) {
        if (data.sources.size() < 2) continue;
        auto* merge = scene.mergeProducts(product(data.sources[0]), product(data.sources[1]));
        if (!merge) continue;
        bool valid = true;
        for (int index = 2; index < data.sources.size(); ++index)
            valid &= scene.addProductToMerge(product(data.sources[index]), merge);
        if (!valid) { scene.splitMerge(merge); continue; }
        // Merge corridors are stored in scene coordinates. Unlike a product
        // line's local route Y, they must follow the paste offset or a cloned
        // group is immediately connected back to the original group's route.
        merge->setManualMergeY(data.y ? std::optional<double>(*data.y + offset.y())
                                      : std::nullopt);
        merges.insert(data.id, merge); ++result.merges;
        streamIds.insert(data.id + ":output", merge->outputStreamId());
    }
    for (const auto& data : snapshot.merges) {
        auto* merge = merges.value(data.id); if (merge && !data.target.isEmpty())
            scene.connectMergeDirect(merge, units.value(data.target)->inputLine());
    }
    for (const auto& data : snapshot.feeds) {
        auto* input = units.value(data.target)->inputLine();
        if (data.processType == "product") scene.connectProductDirect(product(data.processId), input);
        else if (data.processType == "merge") scene.connectMergeDirect(merges.value(data.processId), input);
        FeedJunctionItem* feed = nullptr;
        for (int index = 0; index < data.recycleIds.size(); ++index) {
            feed = data.recycleTypes[index] == "product"
                ? scene.connectRecycle(product(data.recycleIds[index]), input)
                : scene.connectMergedProduct(merges.value(data.recycleIds[index]), input);
            if (!feed) break;
        }
        if (!feed) continue;
        feed->setExternalFeed(data.hasExternalFeed);
        streamIds.insert(data.id + ":output", feed->outputStreamId());
        if (data.hasExternalFeed)
            streamIds.insert(data.id + ":external-feed", feed->externalFeedStreamId());
        for (auto it = data.routeXs.cbegin(); it != data.routeXs.cend(); ++it) {
            const QString id = it.key().endsWith(":output")
                ? merges.value(it.key().chopped(7))->outputStreamId() : product(it.key())->streamId();
            // Feed-junction route handles are absolute scene coordinates.
            // Translate them into the clone's coordinate space so subsequent
            // group dragging can keep the copied wiring rigid.
            feed->setManualRouteX(id, it.value() + offset.x());
        }
        for (auto it = data.routeYs.cbegin(); it != data.routeYs.cend(); ++it) {
            const QString id = it.key().endsWith(":output")
                ? merges.value(it.key().chopped(7))->outputStreamId() : product(it.key())->streamId();
            feed->setManualRouteY(id, it.value() + offset.y());
        }
        ++result.feeds;
    }
    for (auto* item : scene.selectedItems()) item->setSelected(false);
    for (auto it = units.cbegin(); it != units.cend(); ++it) {
        it.value()->setSelected(true);
        for (auto* line : it.value()->products()) {
            const QString oldStream = it.key() + productSideSuffix(line->side());
            if (snapshot.productNames.contains(oldStream)) document.setProductName(line->streamId(), snapshot.productNames.value(oldStream));
        }
    }
    const auto existingAnnotations = document.annotationRecords();
    QSet<QString> annotationIds(existingAnnotations.keyBegin(), existingAnnotations.keyEnd());
    int nextNoteId = 1;
    for (auto note : snapshot.notes) {
        while (annotationIds.contains(QString("note-%1").arg(nextNoteId))) ++nextNoteId;
        note.id = QString("note-%1").arg(nextNoteId++);
        annotationIds.insert(note.id);
        note.manualOffset += offset;
        note.manuallyPlaced = true;
        document.setAnnotationRecord(note);
        result.noteIds.append(note.id);
    }
    // A shared fresh prefix and padded indices retain the original ID order,
    // which also preserves the stacking order of reagents on the same stream.
    int batch = 1;
    QString reagentPrefix;
    do {
        reagentPrefix = QString("reagent-copy-%1-").arg(batch++);
    } while (std::any_of(annotationIds.cbegin(), annotationIds.cend(),
                         [&](const QString& id) { return id.startsWith(reagentPrefix); }));
    const int digits = QString::number(snapshot.reagents.size()).size();
    for (int index = 0; index < snapshot.reagents.size(); ++index) {
        auto reagent = snapshot.reagents[index];
        const auto owner = streamIds.constFind(reagent.ownerId);
        if (owner == streamIds.cend()) continue;
        reagent.id = reagentPrefix + QString("%1").arg(index + 1, digits, 10, QLatin1Char('0'));
        reagent.ownerId = owner.value();
        // Offsets are relative to the remapped stream anchor, so applying the
        // group translation here as well would move the annotation twice.
        document.setAnnotationRecord(std::move(reagent));
        ++result.reagents;
    }
    if (result.units > 0) {
        scene.refreshConnections(); scene.notifyTopologyChanged();
    }
    return result;
}

} // namespace afs::flow_group
