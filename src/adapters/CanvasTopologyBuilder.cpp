#include "adapters/CanvasTopologyBuilder.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/FeedJunctionItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"

#include <algorithm>

namespace afs {
namespace {

QString unitDisplayName(UnitKind kind) {
    switch (kind) {
    case UnitKind::Flotation: return QStringLiteral("浮选单元");
    case UnitKind::MagneticSeparation: return QStringLiteral("磁选机");
    case UnitKind::WeakMagneticSeparation: return QStringLiteral("弱磁选机");
    case UnitKind::StrongMagneticSeparation: return QStringLiteral("强磁选机");
    case UnitKind::SpiralChute: return QStringLiteral("螺旋溜槽");
    case UnitKind::ShakingTable: return QStringLiteral("摇床");
    case UnitKind::DenseMediumCyclone: return QStringLiteral("重介质旋流器");
    case UnitKind::SedimentationTank: return QStringLiteral("沉降箱");
    case UnitKind::DemediumScreen: return QStringLiteral("脱介筛");
    case UnitKind::Screening: return QStringLiteral("筛分机");
    case UnitKind::Classification: return QStringLiteral("分级机");
    case UnitKind::TailingsPool: return QStringLiteral("尾矿池");
    case UnitKind::ConcentratePool: return QStringLiteral("精矿池");
    case UnitKind::WaterPool: return QStringLiteral("回水池");
    case UnitKind::MediaTank: return QStringLiteral("介质桶");
    case UnitKind::MixingTank: return QStringLiteral("混料桶");
    case UnitKind::BinarySplitter: return QStringLiteral("二分流器");
    case UnitKind::ThreeProductFlotation: return QStringLiteral("三产品浮选单元");
    case UnitKind::ThreeProductScreening: return QStringLiteral("三产品筛分器");
    case UnitKind::ThreeProductDemediumScreen: return QStringLiteral("三产品脱介筛");
    }
    return QStringLiteral("流程单元");
}

QString productDisplayLabel(const FlotationUnitItem& unit, ProductSide side) {
    if (isStoragePool(unit.unit().kind)) return QStringLiteral("池出料");
    if (unit.unit().kind == UnitKind::SpiralChute)
        return side == ProductSide::Left ? QStringLiteral("螺旋精") : QStringLiteral("螺旋尾");
    if (unit.unit().kind == UnitKind::ShakingTable)
        return side == ProductSide::Left ? QStringLiteral("摇床精矿") : QStringLiteral("摇床尾矿");
    if (unit.unit().kind == UnitKind::DenseMediumCyclone)
        return side == ProductSide::Left ? QStringLiteral("重") : QStringLiteral("轻");
    if (unit.unit().kind == UnitKind::SedimentationTank)
        return side == ProductSide::Left ? QStringLiteral("溢流") : QStringLiteral("底流");
    if (unit.unit().kind == UnitKind::DemediumScreen)
        return side == ProductSide::Left ? QStringLiteral("脱介产品") : QStringLiteral("稀介质");
    if (unit.unit().kind == UnitKind::Screening)
        return side == ProductSide::Left ? QStringLiteral("筛上") : QStringLiteral("筛下");
    if (unit.unit().kind == UnitKind::Classification)
        return side == ProductSide::Left ? QStringLiteral("溢流") : QStringLiteral("沉砂");
    if (unit.unit().kind == UnitKind::ThreeProductScreening) {
        if (side == ProductSide::Left) return QStringLiteral("筛上");
        if (side == ProductSide::Middle) return QStringLiteral("筛中");
        return QStringLiteral("筛下");
    }
    if (unit.unit().kind == UnitKind::ThreeProductDemediumScreen) {
        if (side == ProductSide::Left) return QStringLiteral("脱介产品");
        if (side == ProductSide::Middle) return QStringLiteral("浓介质");
        return QStringLiteral("稀介质");
    }
    if (isMagneticSeparation(unit.unit().kind)) {
        if (side == ProductSide::Left) return QStringLiteral("磁性");
        if (side == ProductSide::Right) return QStringLiteral("非磁性");
    }
    return productSideLabel(side);
}

bool isTerminalPool(const FlotationUnitItem* unit) {
    return unit && isTerminalStoragePool(unit->unit().kind);
}

} // namespace

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
                                       QString("%1 · %2")
                                           .arg(unitDisplayName(unit->unit().kind), unit->unit().id)});
        if (isStoragePool(unit->unit().kind)) {
            result.graph.addStoragePoolNode({unit->unit().id,
                                              isTerminalStoragePool(unit->unit().kind)});
        } else {
            result.graph.addFlotationNode({unit->unit().id, topology::ProductRole::Unknown,
                                          topology::ProductRole::Unknown,
                                          unit->unit().kind == UnitKind::BinarySplitter
                                              ? std::optional<double>(unit->unit().leftSplitPercent)
                                              : std::nullopt,
                                          hasMiddleProduct(unit->unit().kind)});
        }
    }
    for (auto* merge : merges) {
        result.graph.addMergeNode({merge->id(), topology::MergeRole::ProductMerge});
        result.interestObjects.append({merge->id(), QString("产品汇流 · %1").arg(merge->id())});
    }
    for (auto* junction : feedJunctions) {
        result.graph.addMergeNode({junction->id(), topology::MergeRole::FeedJunction});
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
            const auto sourcePort = isStoragePool(unit->unit().kind)
                ? topology::PortKind::PoolOutput
                : side == ProductSide::Left ? topology::PortKind::LeftProduct
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
                                      .arg(unit->unit().id, productDisplayLabel(*unit, side));
            CanvasStreamDescriptor descriptor{
                product->streamId(),
                displayName,
                product, product->mergeJunction() != nullptr};
            descriptor.terminal = !target.has_value() || isTerminalPool(product->targetUnit());
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
        descriptor.terminal = !target.has_value() || isTerminalPool(merge->targetUnit());
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
