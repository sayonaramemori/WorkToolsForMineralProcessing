#include "editor/FlowsheetScene.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/FeedJunctionItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"

#include <QQueue>
#include <QGraphicsSceneMouseEvent>
#include <QScopedValueRollback>
#include <algorithm>

namespace afs {

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
            if (auto* merge = product->mergeJunction()) {
                // A product merge itself is a physical/topological join even
                // before its output is connected downstream.  Treat every
                // branch source as part of the same movable component.
                for (auto* branch : merge->products()) {
                    auto* sibling = branch->sourceUnit();
                    if (!visited.contains(sibling)) {
                        visited.insert(sibling);
                        queue.enqueue(sibling);
                    }
                }
            }
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
    translateComponentRoutes(component, delta);
    refreshConnections();
}

void FlowsheetScene::translateComponentRoutes(
    const QSet<FlotationUnitItem*>& component, const QPointF& delta) {
    if (delta.isNull()) return;
    QSet<MergeJunctionItem*> merges;
    QSet<FeedJunctionItem*> feeds;
    for (auto* unit : component) {
        for (auto* product : unit->products()) {
            // ProductLineItem is a child of its source unit, so its manual
            // route Y is already in source-local coordinates and must not be
            // translated with a moving component.  Doing so stretches a
            // three-product unit's middle branch on every vertical move.
            if (auto* merge = product->mergeJunction()) merges.insert(merge);
            if (auto* feed = product->feedJunction()) feeds.insert(feed);
        }
        if (auto* feed = unit->inputLine()->feedJunction()) feeds.insert(feed);
    }
    for (auto* merge : merges) {
        bool whollyInside = true;
        for (auto* product : merge->products())
            whollyInside &= component.contains(product->sourceUnit());
        if (whollyInside) merge->translateManualRoute(delta);
        if (auto* feed = merge->feedJunction()) feeds.insert(feed);
    }
    for (auto* feed : feeds) {
        bool whollyInside = component.contains(feed->targetUnit());
        if (auto* product = feed->processProduct()) whollyInside &= component.contains(product->sourceUnit());
        if (auto* merge = feed->processMerge())
            for (auto* product : merge->products()) whollyInside &= component.contains(product->sourceUnit());
        for (auto* product : feed->recycleProducts()) whollyInside &= component.contains(product->sourceUnit());
        for (auto* merge : feed->recycleMerges())
            for (auto* product : merge->products()) whollyInside &= component.contains(product->sourceUnit());
        if (whollyInside) feed->translateManualRoutes(delta);
    }
}

void FlowsheetScene::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
    const QScopedValueRollback<bool> dispatching(m_dispatchingMouseMove, true);
    m_groupMovedUnits.clear();
    QGraphicsScene::mouseMoveEvent(event);
    m_groupMovedUnits.clear();
}

void FlowsheetScene::moveConnectedPeers(FlotationUnitItem* movedUnit, const QPointF& delta) {
    if (m_movingComponent || delta.isNull()) return;
    // QGraphicsView moves every selected item during one drag gesture.  Move
    // only unselected peers ourselves and translate routes once per component
    // in this mouse event. A zero-delay timer can span several mouse events,
    // losing route translations while Qt continues moving the selected units.
    if (m_dispatchingMouseMove && movedUnit->isSelected() && selectedItems().size() > 1) {
        if (m_groupMovedUnits.contains(movedUnit)) return;
        const auto component = connectedComponent(movedUnit);
        m_groupMovedUnits.unite(component);
        m_movingComponent = true;
        for (auto* unit : component)
            if (unit != movedUnit && !unit->isSelected()) unit->setPos(unit->pos() + delta);
        m_movingComponent = false;
        translateComponentRoutes(component, delta);
        refreshConnections();
        return;
    }
    moveComponent(movedUnit, delta, false);
}

void FlowsheetScene::sourceWidthChanged(FlotationUnitItem* source, double oldWidth) {
    if (hasCenteredVerticalPorts(source->unit().kind)) {
        refreshConnections();
        return;
    }
    for (auto* product : source->products()) {
        if (!product->targetUnit()) continue;
        const double oldX = productSideDirection(product->side()) * oldWidth / 2.0;
        const double newX = productSideDirection(product->side()) * source->unit().width / 2.0;
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
    constexpr double minimumLength = 50.0;
    constexpr double maximumLength = 5000.0;
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

} // namespace afs
