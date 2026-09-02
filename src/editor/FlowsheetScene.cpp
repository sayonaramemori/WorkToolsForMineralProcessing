#include "editor/FlowsheetScene.h"
#include "graphics/FlotationGeometry.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/FeedJunctionItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"

#include <QQueue>
#include <QLineF>
#include <QRegularExpression>
#include <algorithm>

namespace afs {

FlowsheetScene::FlowsheetScene(QObject* parent) : QGraphicsScene(parent) {}

InputLineItem* FlowsheetScene::inputAtDropPosition(const QPointF& scenePosition) const {
    constexpr qreal dropTolerance = 24.0;
    InputLineItem* closest = nullptr;
    qreal closestDistance = dropTolerance;
    for (auto* graphicsItem : items()) {
        auto* unit = dynamic_cast<FlotationUnitItem*>(graphicsItem);
        if (!unit) continue;
        const QPointF start = unit->mapToScene(QPointF(0, -FlotationGeometry::InputHeight));
        const QPointF end = unit->mapToScene(QPointF(0, 0));
        const QLineF line(start, end);
        const qreal lengthSquared = line.length() * line.length();
        if (qFuzzyIsNull(lengthSquared)) continue;
        const QPointF relative = scenePosition - start;
        const QPointF direction = end - start;
        const qreal projection = std::clamp(
            QPointF::dotProduct(relative, direction) / lengthSquared, 0.0, 1.0);
        const QPointF nearest = start + projection * direction;
        const qreal distance = QLineF(scenePosition, nearest).length();
        if (distance <= closestDistance) {
            closestDistance = distance;
            closest = unit->inputLine();
        }
    }
    return closest;
}

QSet<FlotationUnitItem*> FlowsheetScene::connectedComponent(FlotationUnitItem* start) const {
    QSet<FlotationUnitItem*> visited;
    QQueue<FlotationUnitItem*> queue;
    visited.insert(start);
    queue.enqueue(start);
    while (!queue.isEmpty()) {
        auto* unit = queue.dequeue();
        for (auto* product : unit->products()) {
            FlotationUnitItem* next = product->targetUnit();
            if (!next && product->feedJunction()) next = product->feedJunction()->targetUnit();
            if (!next && product->mergeJunction()) {
                auto* merge = product->mergeJunction();
                next = merge->targetUnit();
                if (!next && merge->feedJunction()) next = merge->feedJunction()->targetUnit();
            }
            if (next && !visited.contains(next)) {
                visited.insert(next);
                queue.enqueue(next);
            }
        }
        if (auto* incoming = unit->inputLine()->sourceProduct()) {
            auto* previous = incoming->sourceUnit();
            if (!visited.contains(previous)) {
                visited.insert(previous);
                queue.enqueue(previous);
            }
        }
        if (auto* incomingMerge = unit->inputLine()->sourceMerge()) {
            for (auto* product : incomingMerge->products()) {
                auto* source = product->sourceUnit();
                if (!visited.contains(source)) {
                    visited.insert(source);
                    queue.enqueue(source);
                }
            }
        }
        if (auto* feedJunction = unit->inputLine()->feedJunction()) {
            if (auto* process = feedJunction->processProduct()) {
                auto* previous = process->sourceUnit();
                if (!visited.contains(previous)) {
                    visited.insert(previous);
                    queue.enqueue(previous);
                }
            }
            if (auto* processMerge = feedJunction->processMerge()) {
                for (auto* product : processMerge->products()) {
                    auto* previous = product->sourceUnit();
                    if (!visited.contains(previous)) {
                        visited.insert(previous);
                        queue.enqueue(previous);
                    }
                }
            }
            for (auto* recycle : feedJunction->recycleProducts()) {
                auto* previous = recycle->sourceUnit();
                if (!visited.contains(previous)) {
                    visited.insert(previous);
                    queue.enqueue(previous);
                }
            }
            for (auto* recycleMerge : feedJunction->recycleMerges()) {
                for (auto* product : recycleMerge->products()) {
                    auto* previous = product->sourceUnit();
                    if (!visited.contains(previous)) {
                        visited.insert(previous);
                        queue.enqueue(previous);
                    }
                }
            }
        }
    }
    return visited;
}

void FlowsheetScene::moveComponent(FlotationUnitItem* start, const QPointF& delta, bool includeStart) {
    if (delta.isNull()) return;
    const auto component = connectedComponent(start);
    m_movingComponent = true;
    for (auto* unit : component) {
        if (includeStart || unit != start) unit->setPos(unit->pos() + delta);
    }
    m_movingComponent = false;
    refreshConnections();
}

void FlowsheetScene::moveConnectedPeers(FlotationUnitItem* movedUnit, const QPointF& delta) {
    if (!m_movingComponent) moveComponent(movedUnit, delta, false);
}

bool FlowsheetScene::connectProduct(ProductLineItem* product, InputLineItem* input) {
    if (!product || !input || !product->isAvailable()
        || product->sourceUnit() == input->unit()) return false;
    if (input->sourceProduct() || input->sourceMerge() || input->feedJunction()
        || connectedComponent(product->sourceUnit()).contains(input->unit()))
        return connectRecycle(product, input) != nullptr;

    const QPointF desiredInputStart = product->unconnectedEndScenePosition();
    const QPointF currentInputStart = input->unit()->mapToScene(QPointF(0, -FlotationGeometry::InputHeight));
    moveComponent(input->unit(), desiredInputStart - currentInputStart, true);
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

bool FlowsheetScene::connectMerge(MergeJunctionItem* merge, InputLineItem* input) {
    if (!merge || !input || !merge->isAvailable()) return false;
    const auto component = connectedComponent(input->unit());
    for (auto* product : merge->products())
        if (component.contains(product->sourceUnit()))
            return connectMergedProduct(merge, input) != nullptr;

    if (input->sourceProduct() || input->sourceMerge() || input->feedJunction())
        return connectMergedProduct(merge, input) != nullptr;

    const QPointF desiredInputStart = merge->outputEndPosition();
    const QPointF currentInputStart = input->unit()->mapToScene(
        QPointF(0, -FlotationGeometry::InputHeight));
    moveComponent(input->unit(), desiredInputStart - currentInputStart, true);
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

bool FlowsheetScene::disconnectProduct(ProductLineItem* product) {
    if (!product || !product->isConnected()) return false;
    auto* target = product->targetUnit();
    target->inputLine()->setSourceProduct(nullptr);
    product->setTargetUnit(nullptr);
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

void FlowsheetScene::resetGeneratedIds() {
    int maximumMerge = 0;
    int maximumFeedMerge = 0;
    const QRegularExpression mergePattern("^merge-(\\d+)$");
    const QRegularExpression feedPattern("^feed-merge-(\\d+)$");
    for (auto* item : items()) {
        if (auto* merge = dynamic_cast<MergeJunctionItem*>(item)) {
            const auto match = mergePattern.match(merge->id());
            if (match.hasMatch()) maximumMerge = std::max(maximumMerge, match.captured(1).toInt());
        } else if (auto* feed = dynamic_cast<FeedJunctionItem*>(item)) {
            const auto match = feedPattern.match(feed->id());
            if (match.hasMatch()) maximumFeedMerge = std::max(maximumFeedMerge, match.captured(1).toInt());
        }
    }
    m_nextMergeId = maximumMerge + 1;
    m_nextFeedJunctionId = maximumFeedMerge + 1;
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

void FlowsheetScene::refreshConnections() {
    for (auto* item : items()) {
        if (auto* product = dynamic_cast<ProductLineItem*>(item)) product->updatePath();
        if (auto* input = dynamic_cast<InputLineItem*>(item)) input->refreshAppearance();
        if (auto* junction = dynamic_cast<MergeJunctionItem*>(item)) junction->updatePath();
    }
    for (auto* item : items())
        if (auto* junction = dynamic_cast<FeedJunctionItem*>(item)) junction->updatePath();
    emit geometryChanged();
}

void FlowsheetScene::sourceWidthChanged(FlotationUnitItem* source, double oldWidth) {
    for (auto* product : source->products()) {
        if (!product->targetUnit()) continue;
        const double oldX = static_cast<int>(product->side()) * oldWidth / 2.0;
        const double newX = static_cast<int>(product->side()) * source->unit().width / 2.0;
        const QPointF delta(newX - oldX, 0);
        // 改变上游槽宽时只平移该产品对应的下游分支，不能反向移动上游整体。
        QSet<FlotationUnitItem*> downstream;
        QQueue<FlotationUnitItem*> queue;
        downstream.insert(product->targetUnit());
        queue.enqueue(product->targetUnit());
        while (!queue.isEmpty()) {
            auto* unit = queue.dequeue();
            for (auto* childProduct : unit->products()) {
                if (childProduct->targetUnit() && !downstream.contains(childProduct->targetUnit())) {
                    downstream.insert(childProduct->targetUnit());
                    queue.enqueue(childProduct->targetUnit());
                }
            }
        }
        m_movingComponent = true;
        for (auto* unit : downstream) unit->setPos(unit->pos() + delta);
        m_movingComponent = false;
    }
    refreshConnections();
}

bool FlowsheetScene::adjustConnectionLength(ProductLineItem* product, double delta) {
    if (!product || !product->isConnected()) return false;
    constexpr double minimumLength = 100.0;
    constexpr double maximumLength = 1200.0;
    auto* source = product->sourceUnit();
    auto* target = product->targetUnit();
    const double currentLength = target->scenePos().y() - source->scenePos().y();
    const double requestedLength = std::clamp(currentLength + delta, minimumLength, maximumLength);
    const double appliedDelta = requestedLength - currentLength;
    if (qFuzzyIsNull(appliedDelta)) return false;

    // 连接线长度变化只移动目标单元及其下游分支，上游保持原位。
    QSet<FlotationUnitItem*> downstream;
    QQueue<FlotationUnitItem*> queue;
    downstream.insert(target);
    queue.enqueue(target);
    while (!queue.isEmpty()) {
        auto* unit = queue.dequeue();
        for (auto* childProduct : unit->products()) {
            if (childProduct->targetUnit() && !downstream.contains(childProduct->targetUnit())) {
                downstream.insert(childProduct->targetUnit());
                queue.enqueue(childProduct->targetUnit());
            }
        }
    }
    m_movingComponent = true;
    for (auto* unit : downstream) unit->setPos(unit->pos() + QPointF(0, appliedDelta));
    m_movingComponent = false;
    refreshConnections();
    return true;
}

void FlowsheetScene::refreshAppearance() {
    for (auto* item : items()) {
        if (auto* product = dynamic_cast<ProductLineItem*>(item)) product->refreshAppearance();
        if (auto* input = dynamic_cast<InputLineItem*>(item)) input->refreshAppearance();
        if (auto* junction = dynamic_cast<MergeJunctionItem*>(item)) junction->refreshAppearance();
        if (auto* junction = dynamic_cast<FeedJunctionItem*>(item)) junction->refreshAppearance();
        item->update();
    }
    update();
}

} // namespace afs
