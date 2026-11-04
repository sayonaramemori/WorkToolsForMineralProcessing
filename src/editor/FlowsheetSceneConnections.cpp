#include "editor/FlowsheetScene.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/FeedJunctionItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"
#include "graphics/FlotationGeometry.h"

namespace afs {

bool FlowsheetScene::connectProduct(ProductLineItem* product, InputLineItem* input,
                                    bool alignDownstream) {
    if (!product || !input || !product->isAvailable()
        || product->sourceUnit() == input->unit()) return false;
    if (input->sourceProduct() || input->sourceMerge() || input->feedJunction()
        || connectedComponent(product->sourceUnit()).contains(input->unit()))
        return connectRecycle(product, input) != nullptr;

    // Connections normally preserve the manually arranged diagram and are
    // routed orthogonally. Ctrl-drag explicitly moves every kind of
    // downstream unit (and its connected component) into alignment.
    if (alignDownstream) {
        const QPointF desiredInputStart = product->unconnectedEndScenePosition();
        const QPointF currentInputStart = input->startScenePosition();
        moveComponent(input->unit(), desiredInputStart - currentInputStart, true);
    }
    product->setTargetUnit(input->unit());
    input->setSourceProduct(product);
    refreshConnections();
    emit topologyChanged();
    return true;
}

bool FlowsheetScene::connectProductDirect(ProductLineItem* product, InputLineItem* input) {
    if (!product || !input || !product->isAvailable() || input->sourceProduct() || input->sourceMerge()
        || input->feedJunction() || product->sourceUnit() == input->unit()) return false;
    product->setTargetUnit(input->unit());
    input->setSourceProduct(product);
    refreshConnections();
    emit topologyChanged();
    return true;
}

FeedJunctionItem* FlowsheetScene::connectRecycle(
    ProductLineItem* product, InputLineItem* input, const QString& id) {
    if (!product || !input || !product->isAvailable()
        || product == input->sourceProduct() || product->sourceUnit() == input->unit()
        ) return nullptr;
    if (auto* existing = input->feedJunction()) {
        if (!existing->addRecycleProduct(product)) return nullptr;
        product->setFeedJunction(existing);
        emit topologyChanged();
        return existing;
    }
    auto* processProduct = input->sourceProduct();
    auto* processMerge = input->sourceMerge();
    const QString junctionId = id.isEmpty()
        ? QString("feed-merge-%1").arg(m_nextFeedJunctionId++) : id;
    auto* junction = new FeedJunctionItem(
        junctionId, product, input->unit(), processProduct, processMerge);
    addItem(junction);
    junction->setExternalFeed(!processProduct && !processMerge);
    if (processProduct) {
        processProduct->setTargetUnit(nullptr);
        input->setSourceProduct(nullptr);
        processProduct->setFeedJunction(junction);
    } else if (processMerge) {
        processMerge->setTargetUnit(nullptr);
        input->setSourceMerge(nullptr);
        processMerge->setFeedJunction(junction);
    }
    product->setFeedJunction(junction);
    input->setFeedJunction(junction);
    junction->updatePath();
    emit topologyChanged();
    return junction;
}

FeedJunctionItem* FlowsheetScene::connectMergedProduct(
    MergeJunctionItem* merge, InputLineItem* input, const QString& id) {
    if (!merge || !input || !merge->isAvailable()
        || merge == input->sourceMerge()) return nullptr;
    auto* target = input->unit();
    bool sameTopology = false;
    for (auto* product : merge->products())
        sameTopology = sameTopology
            || connectedComponent(product->sourceUnit()).contains(target);
    if (!sameTopology && !input->sourceProduct() && !input->sourceMerge()
        && !input->feedJunction()) return nullptr;

    if (auto* existing = input->feedJunction()) {
        if (!existing->addRecycleMerge(merge)) return nullptr;
        merge->setFeedJunction(existing);
        emit topologyChanged();
        return existing;
    }

    auto* processProduct = input->sourceProduct();
    auto* processMerge = input->sourceMerge();

    const QString junctionId = id.isEmpty()
        ? QString("feed-merge-%1").arg(m_nextFeedJunctionId++) : id;
    auto* junction = new FeedJunctionItem(
        junctionId, merge, target, processProduct, processMerge);
    addItem(junction);
    junction->setExternalFeed(!processProduct && !processMerge);
    if (processProduct) {
        processProduct->setTargetUnit(nullptr);
        input->setSourceProduct(nullptr);
        processProduct->setFeedJunction(junction);
    } else if (processMerge) {
        processMerge->setTargetUnit(nullptr);
        input->setSourceMerge(nullptr);
        processMerge->setFeedJunction(junction);
    }
    merge->setFeedJunction(junction);
    input->setFeedJunction(junction);
    junction->updatePath();
    emit topologyChanged();
    return junction;
}

bool FlowsheetScene::connectMerge(MergeJunctionItem* merge, InputLineItem* input,
                                  bool alignDownstream) {
    if (!merge || !input || !merge->isAvailable()) return false;
    const auto component = connectedComponent(input->unit());
    for (auto* product : merge->products())
        if (component.contains(product->sourceUnit()))
            return connectMergedProduct(merge, input) != nullptr;

    if (input->sourceProduct() || input->sourceMerge() || input->feedJunction())
        return connectMergedProduct(merge, input) != nullptr;

    if (alignDownstream) {
        const QPointF desiredInputStart = merge->outputEndPosition();
        const QPointF currentInputStart = input->startScenePosition();
        moveComponent(input->unit(), desiredInputStart - currentInputStart, true);
    }
    return connectMergeDirect(merge, input);
}

bool FlowsheetScene::connectMergeDirect(MergeJunctionItem* merge, InputLineItem* input) {
    if (!merge || !input || !merge->isAvailable() || input->sourceProduct()
        || input->sourceMerge() || input->feedJunction()) return false;
    merge->setTargetUnit(input->unit());
    input->setSourceMerge(merge);
    refreshConnections();
    emit topologyChanged();
    return true;
}

bool FlowsheetScene::disconnectMerge(MergeJunctionItem* merge) {
    if (!merge || !merge->targetUnit()) return false;
    merge->targetUnit()->inputLine()->setSourceMerge(nullptr);
    merge->setTargetUnit(nullptr);
    refreshConnections();
    emit topologyChanged();
    return true;
}

bool FlowsheetScene::disconnectRecycle(FeedJunctionItem* junction) {
    if (!junction) return false;
    auto* input = junction->targetUnit()->inputLine();
    for (auto* product : junction->recycleProducts()) product->setFeedJunction(nullptr);
    for (auto* merge : junction->recycleMerges()) merge->setFeedJunction(nullptr);
    input->setFeedJunction(nullptr);
    if (auto* process = junction->processProduct()) {
        process->setFeedJunction(nullptr);
        process->setTargetUnit(junction->targetUnit());
        input->setSourceProduct(process);
    } else if (auto* process = junction->processMerge()) {
        process->setFeedJunction(nullptr);
        process->setTargetUnit(junction->targetUnit());
        input->setSourceMerge(process);
    }
    removeItem(junction);
    delete junction;
    emit topologyChanged();
    return true;
}

bool FlowsheetScene::disconnectFeedSource(FeedJunctionItem* junction,
                                          const QString& streamId) {
    if (!junction || streamId.isEmpty()) return false;

    ProductLineItem* selectedProduct = nullptr;
    for (auto* product : junction->recycleProducts()) {
        if (product->streamId() == streamId) {
            selectedProduct = product;
            break;
        }
    }
    MergeJunctionItem* selectedMerge = nullptr;
    if (!selectedProduct) {
        for (auto* merge : junction->recycleMerges()) {
            if (merge->outputStreamId() == streamId) {
                selectedMerge = merge;
                break;
            }
        }
    }
    if (!selectedProduct && !selectedMerge) return false;

    const int sourceCount = junction->recycleProducts().size()
        + junction->recycleMerges().size();
    // Removing the last additional source collapses the now-redundant junction
    // and restores its process source (or the external-feed input line).
    if (sourceCount == 1) return disconnectRecycle(junction);

    if (selectedProduct) {
        selectedProduct->setFeedJunction(nullptr);
        if (!junction->removeRecycleProduct(selectedProduct)) return false;
    } else {
        selectedMerge->setFeedJunction(nullptr);
        if (!junction->removeRecycleMerge(selectedMerge)) return false;
    }
    emit topologyChanged();
    return true;
}

bool FlowsheetScene::disconnectProduct(ProductLineItem* product) {
    if (!product || !product->isConnected()) return false;
    auto* target = product->targetUnit();
    target->inputLine()->setSourceProduct(nullptr);
    product->setTargetUnit(nullptr);
    refreshConnections();
    emit topologyChanged();
    return true;
}

bool FlowsheetScene::removeUnit(FlotationUnitItem* unit) {
    if (!unit || unit->scene() != this) return false;

    auto* input = unit->inputLine();
    if (auto* feed = input->feedJunction()) disconnectRecycle(feed);
    if (auto* source = input->sourceProduct()) disconnectProduct(source);
    if (auto* source = input->sourceMerge()) disconnectMerge(source);

    const auto products = unit->products();
    for (auto* product : products) {
        if (auto* feed = product->feedJunction()) {
            if (feed->processProduct() == product) {
                product->setFeedJunction(nullptr);
                feed->clearProcessSource();
                emit topologyChanged();
            } else {
                disconnectFeedSource(feed, product->streamId());
            }
        }
        if (auto* merge = product->mergeJunction()) splitMerge(merge);
        if (product->targetUnit()) disconnectProduct(product);
    }

    removeItem(unit);
    delete unit;
    refreshConnections();
    emit topologyChanged();
    return true;
}

MergeJunctionItem* FlowsheetScene::mergeProducts(
    ProductLineItem* first, ProductLineItem* second, const QString& id) {
    if (!first || !second || first == second || !first->isAvailable() || !second->isAvailable()
        || first->sourceUnit() == second->sourceUnit()) return nullptr;
    const QString junctionId = id.isEmpty() ? QString("merge-%1").arg(m_nextMergeId++) : id;
    auto* junction = new MergeJunctionItem(junctionId, first, second);
    addItem(junction);
    first->setMergeJunction(junction);
    second->setMergeJunction(junction);
    junction->updatePath();
    emit topologyChanged();
    return junction;
}

bool FlowsheetScene::addProductToMerge(ProductLineItem* product, MergeJunctionItem* junction) {
    if (!product || !junction || !product->isAvailable() || !junction->isAvailable()) return false;
    for (auto* existing : junction->products())
        if (existing->sourceUnit() == product->sourceUnit()) return false;
    if (!junction->addProduct(product)) return false;
    product->setMergeJunction(junction);
    junction->updatePath();
    emit topologyChanged();
    return true;
}

bool FlowsheetScene::splitMerge(MergeJunctionItem* junction) {
    if (!junction) return false;
    if (junction->targetUnit()) disconnectMerge(junction);
    if (auto* feedJunction = junction->feedJunction())
        disconnectRecycle(feedJunction);
    for (auto* product : junction->products()) product->setMergeJunction(nullptr);
    removeItem(junction);
    delete junction;
    emit topologyChanged();
    return true;
}

} // namespace afs
