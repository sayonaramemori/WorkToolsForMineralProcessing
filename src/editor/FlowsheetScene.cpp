#include "editor/FlowsheetScene.h"
#include "editor/CrossingBridgeRenderer.h"
#include "editor/FlowsheetRoutingCoordinator.h"
#include "graphics/FlotationGeometry.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/FeedJunctionItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"

#include <QLineF>
#include <QGraphicsPathItem>
#include <QGraphicsSceneMouseEvent>
#include <QRegularExpression>
#include <algorithm>
#include <limits>

namespace afs {

namespace {

// QGraphicsScene resolves overlapping item shapes by z-order.  That is a
// poor fit for dense orthogonal flowsheets: a long route can be above a short
// terminal line even when the pointer is visibly closer to the latter.  Find
// the nearest actual path segment instead, while retaining the items' normal
// press handlers (route editing, dragging and context state).
qreal squaredDistanceToSegment(const QPointF& point, const QPointF& start,
                               const QPointF& end) {
    const QPointF direction = end - start;
    const qreal lengthSquared = QPointF::dotProduct(direction, direction);
    if (qFuzzyIsNull(lengthSquared)) {
        const QPointF delta = point - start;
        return QPointF::dotProduct(delta, delta);
    }
    const qreal projection = std::clamp(
        QPointF::dotProduct(point - start, direction) / lengthSquared, 0.0, 1.0);
    const QPointF delta = point - (start + projection * direction);
    return QPointF::dotProduct(delta, delta);
}

qreal squaredDistanceToRoute(const QGraphicsPathItem* item, const QPointF& scenePosition) {
    qreal closest = std::numeric_limits<qreal>::infinity();
    const auto polygons = item->path().toSubpathPolygons(item->sceneTransform());
    for (const auto& polygon : polygons) {
        for (int index = 1; index < polygon.size(); ++index) {
            closest = std::min(closest, squaredDistanceToSegment(
                scenePosition, polygon.at(index - 1), polygon.at(index)));
        }
    }
    return closest;
}

QGraphicsPathItem* nearestRouteAt(const QGraphicsScene& scene, const QPointF& scenePosition) {
    QGraphicsPathItem* nearest = nullptr;
    qreal closest = std::numeric_limits<qreal>::infinity();
    for (auto* item : scene.items(scenePosition, Qt::IntersectsItemShape, Qt::DescendingOrder)) {
        // Input lines intentionally have a generous hit area for connection
        // gestures.  They are not part of route selection arbitration.
        if (!dynamic_cast<ProductLineItem*>(item)
            && !dynamic_cast<MergeJunctionItem*>(item)
            && !dynamic_cast<FeedJunctionItem*>(item)) continue;
        auto* route = dynamic_cast<QGraphicsPathItem*>(item);
        if (!route || !route->isVisible()) continue;
        const qreal distance = squaredDistanceToRoute(route, scenePosition);
        if (distance < closest) {
            closest = distance;
            nearest = route;
        }
    }
    return nearest;
}

} // namespace

FlowsheetScene::FlowsheetScene(QObject* parent) : QGraphicsScene(parent) {}

void FlowsheetScene::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        QGraphicsScene::mousePressEvent(event);
        return;
    }

    auto* nearest = nearestRouteAt(*this, event->scenePos());
    if (!nearest) {
        QGraphicsScene::mousePressEvent(event);
        return;
    }

    // Let QGraphicsScene deliver the original event so the chosen item keeps
    // its existing interaction behavior.  Temporarily raising it changes only
    // the dispatch priority; restoring it immediately keeps rendering and the
    // saved stacking order unchanged.
    const qreal originalZ = nearest->zValue();
    nearest->setZValue(10000.0);
    QGraphicsScene::mousePressEvent(event);
    nearest->setZValue(originalZ);
}

void FlowsheetScene::drawForeground(QPainter* painter, const QRectF&) {
    CrossingBridgeRenderer::draw(*this, *painter);
}

InputLineItem* FlowsheetScene::inputAtDropPosition(const QPointF& scenePosition) const {
    constexpr qreal dropTolerance = 24.0;
    InputLineItem* closest = nullptr;
    qreal closestDistance = dropTolerance;
    for (auto* graphicsItem : items()) {
        auto* unit = dynamic_cast<FlotationUnitItem*>(graphicsItem);
        if (!unit) continue;
        const QPointF start = unit->inputLine()->startScenePosition();
        const QPointF end = unit->inputAnchorScenePosition();
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

void FlowsheetScene::refreshConnections() {
    FlowsheetRoutingCoordinator::refreshPaths(*this);
    emit geometryChanged();
}

void FlowsheetScene::refreshAppearance() {
    FlowsheetRoutingCoordinator::refreshAppearance(*this);
}

} // namespace afs
