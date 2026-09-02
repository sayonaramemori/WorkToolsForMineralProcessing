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
        result.graph.addFlotationNode({unit->unit().id, topology::ProductRole::Unknown,
                                      topology::ProductRole::Unknown});
    }
    for (auto* merge : merges) result.graph.addMergeNode({merge->id()});
    for (auto* junction : feedJunctions) result.graph.addMergeNode({junction->id()});

    for (auto* unit : units) {
        if (!unit->inputLine()->sourceProduct() && !unit->inputLine()->sourceMerge()
            && !unit->inputLine()->feedJunction()) {
            result.graph.addStream({unit->unit().id + ":feed", std::nullopt,
                                    topology::PortRef{unit->unit().id, topology::PortKind::Feed}});
            result.reportStreams.append({
                unit->unit().id + ":feed",
                QString("合计（单元 %1 入料）").arg(unit->unit().id), unit->inputLine()});
        }
        const auto products = unit->products();
        for (int index = 0; index < products.size(); ++index) {
            auto* product = products[index];
            const auto sourcePort = index == 0 ? topology::PortKind::LeftProduct
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
            const CanvasStreamDescriptor descriptor{
                product->streamId(),
                QString("单元 %1 - %2产品").arg(unit->unit().id,
                    index == 0 ? QString("左") : QString("右")),
                product, product->mergeJunction() != nullptr};
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
        const CanvasStreamDescriptor descriptor{
            merge->outputStreamId(), QString("合流产品 · %1").arg(merge->id()), merge};
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
                QString("合计（单元 %1 外部入料）").arg(junction->targetUnit()->unit().id), junction});
        }
        result.graph.addStream({junction->outputStreamId(),
                                topology::PortRef{junction->id(), topology::PortKind::MergeOutput},
                                topology::PortRef{junction->targetUnit()->unit().id,
                                                  topology::PortKind::Feed}});
    }
    result.reportStreams += result.productStreams;
    result.requiredMeasurements = result.terminalProducts;
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
                    .arg(product->sourceUnit()->unit().id,
                         product->side() == ProductSide::Left ? QString("左") : QString("右")),
                merge});
        }
    }
    for (auto* junction : feedJunctions) {
        for (auto* product : junction->recycleProducts()) {
            result.requiredMeasurements.append({
                product->streamId(),
                QString("回流：单元 %1 - %2产品")
                    .arg(product->sourceUnit()->unit().id,
                         product->side() == ProductSide::Left ? QString("左") : QString("右")),
                junction});
        }
        for (auto* merge : junction->recycleMerges()) {
            for (auto* product : merge->products()) {
                result.requiredMeasurements.append({
                    product->streamId(),
                    QString("回流支路：单元 %1 - %2产品")
                        .arg(product->sourceUnit()->unit().id,
                             product->side() == ProductSide::Left ? QString("左") : QString("右")),
                    junction});
            }
        }
    }
    return result;
}

} // namespace afs
