#include "services/FlowGroupSnapshotOperations.h"

#include "annotations/AnnotationItem.h"
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

CopyResult capture(const FlowsheetScene& scene, const FlowsheetDocument& document, Snapshot& snapshot) {
    snapshot = {};
    QSet<QString> unitIds;
    for (auto* item : scene.selectedItems()) {
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item)) {
            snapshot.units.append(unit->unit()); unitIds.insert(unit->unit().id);
        } else if (auto* note = dynamic_cast<AnnotationItem*>(item);
                   note && note->record().kind == AnnotationKind::UserNote) {
            auto record = document.annotationRecord(note->record().id);
            if (record.id.isEmpty() || record.kind != AnnotationKind::UserNote) continue;
            record.manualOffset = note->scenePos();
            snapshot.notes.append(std::move(record));
        }
    }
    CopyResult result; result.units = snapshot.units.size(); result.notes = snapshot.notes.size();
    if (!snapshot.hasData()) return result;
    std::sort(snapshot.units.begin(), snapshot.units.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    std::sort(snapshot.notes.begin(), snapshot.notes.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    QSet<QString> streams;
    for (auto* item : scene.items()) if (auto* unit = dynamic_cast<FlotationUnitItem*>(item)) {
        if (!unitIds.contains(unit->unit().id)) continue;
        auto* input = unit->inputLine();
        if (!input->sourceProduct() && !input->sourceMerge() && !input->feedJunction())
            streams.insert(unit->unit().id + ":feed");
        for (auto* product : unit->products()) {
            streams.insert(product->streamId());
            if (const auto length = product->terminalLengthOverride())
                snapshot.terminalLengths.insert(product->streamId(), *length);
            if (document.productNames().contains(product->streamId()))
                snapshot.productNames.insert(product->streamId(), document.productName(product->streamId()));
        }
    }
    // Only complete product merges are copied; a branch that merges into an
    // unselected process stays an unconnected output in the clone.
    QSet<QString> copiedMerges;
    for (auto* item : scene.items()) if (auto* merge = dynamic_cast<MergeJunctionItem*>(item)) {
        Snapshot::Merge data; data.id = merge->id(); data.y = merge->manualMergeY();
        bool complete = true;
        for (auto* product : merge->products()) {
            data.sources.append(product->streamId()); complete &= streams.contains(product->streamId());
        }
        if (!complete) continue;
        if (merge->targetUnit() && unitIds.contains(merge->targetUnit()->unit().id))
            data.target = merge->targetUnit()->unit().id;
        snapshot.merges.append(std::move(data)); copiedMerges.insert(merge->id()); ++result.merges;
        streams.insert(merge->outputStreamId());
    }
    for (auto* item : scene.items()) if (auto* unit = dynamic_cast<FlotationUnitItem*>(item)) {
        if (!unitIds.contains(unit->unit().id)) continue;
        auto* input = unit->inputLine();
        if (!input->feedJunction() && input->sourceProduct()
            && streams.contains(input->sourceProduct()->streamId())) {
            snapshot.direct.append({input->sourceProduct()->streamId(), unit->unit().id,
                             input->sourceProduct()->manualRouteY()}); ++result.connections;
        }
    }
    for (auto* item : scene.items()) if (auto* feed = dynamic_cast<FeedJunctionItem*>(item)) {
        if (!unitIds.contains(feed->targetUnit()->unit().id)) continue;
        Snapshot::Feed data; data.id = feed->id(); data.target = feed->targetUnit()->unit().id;
        data.hasExternalFeed = feed->hasExternalFeed();
        bool complete = true;
        if (feed->processProduct()) { data.processType = "product"; data.processId = feed->processProduct()->streamId(); complete &= streams.contains(data.processId); }
        if (feed->processMerge()) { data.processType = "merge"; data.processId = feed->processMerge()->id(); complete &= copiedMerges.contains(data.processId); }
        for (auto* product : feed->recycleProducts()) { data.recycleTypes.append("product"); data.recycleIds.append(product->streamId()); complete &= streams.contains(product->streamId()); }
        for (auto* merge : feed->recycleMerges()) { data.recycleTypes.append("merge"); data.recycleIds.append(merge->id()); complete &= copiedMerges.contains(merge->id()); }
        if (!complete || data.recycleIds.isEmpty()) continue;
        data.routeXs = feed->manualRouteXs(); data.routeYs = feed->manualRouteYs();
        snapshot.feeds.append(std::move(data)); ++result.feeds;
        streams.insert(feed->outputStreamId());
        if (feed->hasExternalFeed()) streams.insert(feed->externalFeedStreamId());
    }
    const auto records = document.annotationRecords();
    for (const auto& record : records)
        if (record.kind == AnnotationKind::Reagent && streams.contains(record.ownerId))
            snapshot.reagents.append(record);
    std::sort(snapshot.reagents.begin(), snapshot.reagents.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    result.reagents = snapshot.reagents.size();
    return result;
}

} // namespace afs::flow_group
