#include "services/FlowGroupClipboard.h"

#include "document/FlowsheetDocument.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FeedJunctionItem.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"

#include <QSet>
#include <algorithm>

namespace afs {
FlowGroupClipboard::CopyResult FlowGroupClipboard::copy(
    const FlowsheetScene& scene, const FlowsheetDocument& document) {
    m_units.clear(); m_direct.clear(); m_merges.clear(); m_feeds.clear();
    m_productNames.clear(); m_pasteCount = 0;
    QSet<QString> unitIds;
    for (auto* item : scene.selectedItems()) {
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item)) {
            m_units.append(unit->unit()); unitIds.insert(unit->unit().id);
        }
    }
    CopyResult result; result.units = m_units.size();
    if (m_units.isEmpty()) return result;
    std::sort(m_units.begin(), m_units.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    QSet<QString> streams;
    for (auto* item : scene.items()) if (auto* unit = dynamic_cast<FlotationUnitItem*>(item)) {
        if (!unitIds.contains(unit->unit().id)) continue;
        for (auto* product : unit->products()) {
            streams.insert(product->streamId());
            if (document.productNames().contains(product->streamId()))
                m_productNames.insert(product->streamId(), document.productName(product->streamId()));
        }
    }
    // Only complete product merges are copied; a branch that merges into an
    // unselected process stays an unconnected output in the clone.
    QSet<QString> copiedMerges;
    for (auto* item : scene.items()) if (auto* merge = dynamic_cast<MergeJunctionItem*>(item)) {
        Merge data; data.id = merge->id(); data.y = merge->manualMergeY();
        bool complete = true;
        for (auto* product : merge->products()) {
            data.sources.append(product->streamId()); complete &= streams.contains(product->streamId());
        }
        if (!complete) continue;
        if (merge->targetUnit() && unitIds.contains(merge->targetUnit()->unit().id))
            data.target = merge->targetUnit()->unit().id;
        m_merges.append(std::move(data)); copiedMerges.insert(merge->id()); ++result.merges;
    }
    for (auto* item : scene.items()) if (auto* unit = dynamic_cast<FlotationUnitItem*>(item)) {
        if (!unitIds.contains(unit->unit().id)) continue;
        auto* input = unit->inputLine();
        if (!input->feedJunction() && input->sourceProduct()
            && streams.contains(input->sourceProduct()->streamId())) {
            m_direct.append({input->sourceProduct()->streamId(), unit->unit().id,
                             input->sourceProduct()->manualRouteY()}); ++result.connections;
        }
    }
    for (auto* item : scene.items()) if (auto* feed = dynamic_cast<FeedJunctionItem*>(item)) {
        if (!unitIds.contains(feed->targetUnit()->unit().id)) continue;
        Feed data; data.target = feed->targetUnit()->unit().id;
        data.hasExternalFeed = feed->hasExternalFeed();
        bool complete = true;
        if (feed->processProduct()) { data.processType = "product"; data.processId = feed->processProduct()->streamId(); complete &= streams.contains(data.processId); }
        if (feed->processMerge()) { data.processType = "merge"; data.processId = feed->processMerge()->id(); complete &= copiedMerges.contains(data.processId); }
        for (auto* product : feed->recycleProducts()) { data.recycleTypes.append("product"); data.recycleIds.append(product->streamId()); complete &= streams.contains(product->streamId()); }
        for (auto* merge : feed->recycleMerges()) { data.recycleTypes.append("merge"); data.recycleIds.append(merge->id()); complete &= copiedMerges.contains(merge->id()); }
        if (!complete || data.recycleIds.isEmpty()) continue;
        data.routeXs = feed->manualRouteXs(); data.routeYs = feed->manualRouteYs();
        m_feeds.append(std::move(data)); ++result.feeds;
    }
    return result;
}

FlowGroupClipboard::PasteResult FlowGroupClipboard::paste(
    FlowsheetScene& scene, FlowsheetDocument& document,
    const std::function<QString()>& nextUnitId) {
    PasteResult result; if (!hasData()) return result;
    QHash<QString, FlotationUnitItem*> units; QHash<QString, ProductLineItem*> products;
    // Do not increment and read m_pasteCount in separate QPointF arguments:
    // their evaluation order is unspecified and could produce (40, 0).
    const int pasteIndex = ++m_pasteCount;
    const QPointF offset(40.0 * pasteIndex, 40.0 * pasteIndex);
    for (const auto& source : m_units) {
        FlotationUnit clone = source; clone.id = nextUnitId(); clone.position += offset;
        auto* unit = new FlotationUnitItem(clone); scene.addItem(unit); units.insert(source.id, unit);
        for (auto* product : unit->products()) products.insert(source.id + productSideSuffix(product->side()), product);
        ++result.units;
    }
    const auto product = [&products](const QString& oldStream) { return products.value(oldStream); };
    for (const auto& data : m_direct) if (scene.connectProductDirect(product(data.source), units.value(data.target)->inputLine())) {
        product(data.source)->setManualRouteY(data.routeY); ++result.connections;
    }
    QHash<QString, MergeJunctionItem*> merges;
    for (const auto& data : m_merges) {
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
    }
    for (const auto& data : m_merges) {
        auto* merge = merges.value(data.id); if (merge && !data.target.isEmpty())
            scene.connectMergeDirect(merge, units.value(data.target)->inputLine());
    }
    for (const auto& data : m_feeds) {
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
            if (m_productNames.contains(oldStream)) document.setProductName(line->streamId(), m_productNames.value(oldStream));
        }
    }
    scene.refreshConnections(); scene.notifyTopologyChanged();
    return result;
}
} // namespace afs
