#include "adapters/CanvasTopologyBuilder.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/FeedJunctionItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"

#include <algorithm>

namespace afs {

CanvasTopologySnapshot CanvasTopologyBuilder::build(const FlowsheetScene& scene) {
    CanvasTopologySnapshot result;
    QVector<FlotationUnitItem*> units;
    QVector<MergeJunctionItem*> merges;
    QVector<FeedJunctionItem*> feedJunctions;
    for (auto* item : scene.items()) {
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item)) units.append(unit);
        if (auto* merge = dynamic_cast<MergeJunctionItem*>(item)) merges.append(merge);
        if (auto* junction = dynamic_cast<FeedJunctionItem*>(item)) feedJunctions.append(junction);
    }
    std::sort(units.begin(), units.end(), [](auto* a, auto* b) { return a->unit().id < b->unit().id; });
    std::sort(merges.begin(), merges.end(), [](auto* a, auto* b) { return a->id() < b->id(); });
    std::sort(feedJunctions.begin(), feedJunctions.end(),
              [](auto* a, auto* b) { return a->id() < b->id(); });

    for (auto* unit : units) {
        result.interestObjects.append({unit->unit().id,
                                       QString("浮选单元 · %1").arg(unit->unit().id)});
        result.graph.addFlotationNode({unit->unit().id, topology::ProductRole::Unknown,
                                      topology::ProductRole::Unknown,
                                      unit->unit().kind == UnitKind::BinarySplitter
                                          ? std::optional<double>(unit->unit().leftSplitPercent)
                                          : std::nullopt,
                                      unit->unit().kind == UnitKind::ThreeProductFlotation});
    }
    for (auto* merge : merges) {
        result.graph.addMergeNode({merge->id()});
        result.interestObjects.append({merge->id(), QString("产品汇流 · %1").arg(merge->id())});
    }
    for (auto* junction : feedJunctions) {
        result.graph.addMergeNode({junction->id()});
        result.interestObjects.append({junction->id(), QString("入料汇流 · %1").arg(junction->id())});
    }

    for (auto* unit : units) {
        if (!unit->inputLine()->sourceProduct() && !unit->inputLine()->sourceMerge()
            && !unit->inputLine()->feedJunction()) {
            result.graph.addStream({unit->unit().id + ":feed", std::nullopt,
                                    topology::PortRef{unit->unit().id, topology::PortKind::Feed}});
            result.reportStreams.append({
                unit->unit().id + ":feed",
                QString("单元 %1 主入料（无回流）").arg(unit->unit().id), unit->inputLine(),
                false, false, true, false, {unit->unit().id}});
        }
        const auto products = unit->products();
        for (int index = 0; index < products.size(); ++index) {
            auto* product = products[index];
            const auto side = product->side();
            const auto sourcePort = side == ProductSide::Left ? topology::PortKind::LeftProduct
                : side == ProductSide::Middle ? topology::PortKind::MiddleProduct
                                              : topology::PortKind::RightProduct;
            std::optional<topology::PortRef> target;
            if (product->targetUnit()) {
                target = topology::PortRef{product->targetUnit()->unit().id, topology::PortKind::Feed};
            } else if (product->mergeJunction()) {
                target = topology::PortRef{product->mergeJunction()->id(), topology::PortKind::MergeInput};
            } else if (product->feedJunction()) {
                target = topology::PortRef{product->feedJunction()->id(), topology::PortKind::MergeInput};
            }
            result.graph.addStream({product->streamId(),
                                    topology::PortRef{unit->unit().id, sourcePort}, target});
            QString displayName = QString("单元 %1 - %2产品")
                                      .arg(unit->unit().id, productSideLabel(side));
            CanvasStreamDescriptor descriptor{
                product->streamId(),
                displayName,
                product, product->mergeJunction() != nullptr};
            descriptor.terminal = !target.has_value();
            descriptor.feed = target.has_value() && target->port == topology::PortKind::Feed;
            if (auto* feed = product->feedJunction()) {
                const bool processFeed = feed->processProduct() == product;
                descriptor.feed = processFeed;
                descriptor.recycle = !processFeed;
                descriptor.displayName += processFeed
                    ? QString("（单元 %1 主入料）").arg(feed->targetUnit()->unit().id)
                    : QString("（回流至单元 %1）").arg(feed->targetUnit()->unit().id);
            } else if (auto* merge = product->mergeJunction(); merge && merge->feedJunction()) {
                auto* feed = merge->feedJunction();
                const bool processFeed = feed->processMerge() == merge;
                descriptor.feed = processFeed;
                descriptor.recycle = !processFeed;
            } else if (product->targetUnit()) {
                descriptor.displayName += QString("（单元 %1 主入料）")
                                              .arg(product->targetUnit()->unit().id);
            }
            descriptor.ownerIds.append(unit->unit().id);
            if (product->targetUnit()) descriptor.ownerIds.append(product->targetUnit()->unit().id);
            if (product->mergeJunction()) descriptor.ownerIds.append(product->mergeJunction()->id());
            if (product->feedJunction()) descriptor.ownerIds.append(product->feedJunction()->id());
            result.productStreams.append(descriptor);
            if (!target) {
                result.terminalProducts.append(descriptor);
            }
        }
    }
    for (auto* merge : merges) {
        std::optional<topology::PortRef> target;
        if (merge->targetUnit()) {
            target = topology::PortRef{merge->targetUnit()->unit().id,
                                       topology::PortKind::Feed};
        } else if (merge->feedJunction()) {
            target = topology::PortRef{merge->feedJunction()->id(), topology::PortKind::MergeInput};
        }
        result.graph.addStream({merge->outputStreamId(),
                                topology::PortRef{merge->id(), topology::PortKind::MergeOutput}, target});
        CanvasStreamDescriptor descriptor{
            merge->outputStreamId(), QString("合流产品 · %1").arg(merge->id()), merge};
        descriptor.terminal = !target.has_value();
        descriptor.feed = target.has_value() && target->port == topology::PortKind::Feed;
        if (auto* feed = merge->feedJunction()) {
            const bool processFeed = feed->processMerge() == merge;
            descriptor.feed = processFeed;
            descriptor.recycle = !processFeed;
            descriptor.displayName += processFeed
                ? QString("（单元 %1 主入料）").arg(feed->targetUnit()->unit().id)
                : QString("（回流至单元 %1）").arg(feed->targetUnit()->unit().id);
        } else if (merge->targetUnit()) {
            descriptor.displayName += QString("（单元 %1 主入料）")
                                          .arg(merge->targetUnit()->unit().id);
        }
        descriptor.ownerIds.append(merge->id());
        if (merge->targetUnit()) descriptor.ownerIds.append(merge->targetUnit()->unit().id);
        if (merge->feedJunction()) descriptor.ownerIds.append(merge->feedJunction()->id());
        result.productStreams.append(descriptor);
        if (!target)
            result.terminalProducts.append(descriptor);
    }
    for (auto* junction : feedJunctions) {
        if (junction->hasExternalFeed()) {
            result.graph.addStream({junction->externalFeedStreamId(), std::nullopt,
                                    topology::PortRef{junction->id(), topology::PortKind::MergeInput}});
            result.reportStreams.append({
                junction->externalFeedStreamId(),
                QString("单元 %1 主入料（不含回流）").arg(junction->targetUnit()->unit().id),
                junction, false, false, true, false,
                {junction->id(), junction->targetUnit()->unit().id}});
        }
        result.graph.addStream({junction->outputStreamId(),
                                topology::PortRef{junction->id(), topology::PortKind::MergeOutput},
                                topology::PortRef{junction->targetUnit()->unit().id,
                                                  topology::PortKind::Feed}});
        result.reportStreams.append({
            junction->outputStreamId(),
            QString("单元 %1 总入料（含回流）").arg(junction->targetUnit()->unit().id),
            junction, false, false, true, false,
            {junction->id(), junction->targetUnit()->unit().id}});
    }
    result.reportStreams += result.productStreams;
    for (const auto& terminal : result.terminalProducts) {
        auto* product = dynamic_cast<ProductLineItem*>(terminal.graphicsItem);
        if (product && product->sourceUnit()->unit().kind == UnitKind::BinarySplitter
            && product->side() == ProductSide::Right) continue;
        result.requiredMeasurements.append(terminal);
    }
    // A downstream merge provides only the sum of its two input branches.
    // One measured branch is therefore required to identify the other one.
    for (auto* merge : merges) {
        if (!merge->targetUnit()) continue;
        // A downstream merge with N inputs has N-1 allocation degrees of freedom.
        for (int index = 0; index + 1 < merge->products().size(); ++index) {
            auto* product = merge->products()[index];
            result.requiredMeasurements.append({
                product->streamId(),
                QString("合流支路：单元 %1 - %2产品")
                    .arg(product->sourceUnit()->unit().id, productSideLabel(product->side())),
                merge});
        }
    }
    for (auto* junction : feedJunctions) {
        for (auto* product : junction->recycleProducts()) {
            result.requiredMeasurements.append({
                product->streamId(),
                QString("回流：单元 %1 - %2产品")
                    .arg(product->sourceUnit()->unit().id, productSideLabel(product->side())),
                junction});
        }
        for (auto* merge : junction->recycleMerges()) {
            for (auto* product : merge->products()) {
                result.requiredMeasurements.append({
                    product->streamId(),
                    QString("回流支路：单元 %1 - %2产品")
                        .arg(product->sourceUnit()->unit().id, productSideLabel(product->side())),
                    junction});
            }
        }
    }
    return result;
}

} // namespace afs
