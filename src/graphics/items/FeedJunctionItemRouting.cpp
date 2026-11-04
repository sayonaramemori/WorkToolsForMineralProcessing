#include "graphics/items/FeedJunctionItem.h"
#include "graphics/FlotationGeometry.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"
#include <QPainterPath>
#include <QPainterPathStroker>
#include <algorithm>

namespace afs {
namespace {
constexpr double kRouteClearance = 80.0;
constexpr double kRouteArrowLength = 11.0;
constexpr double kRouteArrowHalfWidth = 5.0;
}

QPointF FeedJunctionItem::processSourceAnchor() const {
    return m_processProduct ? m_processProduct->sourceAnchorScenePosition()
                            : m_processMerge->mergePosition();
}

void FeedJunctionItem::appendSourcePath(QPainterPath& path, const QString& streamId,
                                        const QPointF& start, const QPointF& end,
                                        bool routeLeft, int routeIndex, int entryIndex) {
    const double offset = routeIndex * 18.0;
    const double automaticX = routeLeft
        ? std::min(end.x(), m_junctionPosition.x()) - kRouteClearance - offset
        : std::max(end.x(), m_junctionPosition.x()) + kRouteClearance + offset;
    const double corridorX = m_manualRouteXs.value(streamId, automaticX);
    const double automaticY = m_junctionPosition.y() - (entryIndex + 1) * 18.0;
    const double entryY = m_manualRouteYs.value(streamId, automaticY);
    QPainterPath sourcePath(start);
    sourcePath.lineTo(end);
    sourcePath.lineTo(corridorX, end.y());
    sourcePath.lineTo(corridorX, entryY);
    sourcePath.lineTo(m_junctionPosition.x(), entryY);
    sourcePath.lineTo(m_junctionPosition);
    path.addPath(sourcePath);
    m_sourcePaths.insert(streamId, sourcePath);
    const double direction = m_junctionPosition.x() >= corridorX ? 1.0 : -1.0;
    const double arrowX = corridorX + (m_junctionPosition.x() - corridorX) * 0.68;
    QPainterPath sourceArrow;
    sourceArrow.moveTo(arrowX + direction * kRouteArrowLength / 2.0, entryY);
    sourceArrow.lineTo(arrowX - direction * kRouteArrowLength / 2.0,
                       entryY - kRouteArrowHalfWidth);
    sourceArrow.lineTo(arrowX - direction * kRouteArrowLength / 2.0,
                       entryY + kRouteArrowHalfWidth);
    sourceArrow.closeSubpath();
    m_sourceArrowPaths.insert(streamId, sourceArrow);
    m_sourceAnnotationAnchors.insert(
        streamId, QPointF(corridorX, (end.y() + entryY) / 2.0));
    m_verticalHandles.insert(streamId, QPointF(corridorX, (end.y() + entryY) / 2.0));
    m_horizontalHandles.insert(
        streamId, QPointF((corridorX + m_junctionPosition.x()) / 2.0, entryY));
}

void FeedJunctionItem::setManualRouteX(const QString& streamId, std::optional<double> x) {
    if (x) m_manualRouteXs.insert(streamId, *x);
    else m_manualRouteXs.remove(streamId);
    updatePath();
}

void FeedJunctionItem::setManualRouteY(const QString& streamId, std::optional<double> y) {
    if (y) m_manualRouteYs.insert(streamId, *y);
    else m_manualRouteYs.remove(streamId);
    updatePath();
}

void FeedJunctionItem::resetManualRoutes() {
    m_manualRouteXs.clear();
    m_manualRouteYs.clear();
    updatePath();
}

void FeedJunctionItem::translateManualRoutes(const QPointF& delta) {
    if (delta.isNull()) return;
    if (!qFuzzyIsNull(delta.x()))
        for (auto it = m_manualRouteXs.begin(); it != m_manualRouteXs.end(); ++it)
            it.value() += delta.x();
    if (!qFuzzyIsNull(delta.y()))
        for (auto it = m_manualRouteYs.begin(); it != m_manualRouteYs.end(); ++it)
            it.value() += delta.y();
}

QPainterPath FeedJunctionItem::shape() const {
    QPainterPathStroker stroker;
    stroker.setWidth(FlotationGeometry::RouteHitWidth);
    QPainterPath hit = stroker.createStroke(m_linePath);
    hit.addEllipse(m_junctionPosition, JunctionRadius + 2.0, JunctionRadius + 2.0);
    return hit;
}

void FeedJunctionItem::updatePath() {
    prepareGeometryChange();
    const QPointF targetTop = m_targetUnit->inputAnchorScenePosition();
    const QPointF freshFeedStart = m_targetUnit->inputLine()->startScenePosition();
    m_junctionPosition = (targetTop + freshFeedStart) / 2.0;

    m_recycleAnchor = m_junctionPosition;
    m_externalFeedAnchor = hasExternalFeed()
        ? (freshFeedStart + m_junctionPosition) / 2.0 : QPointF();
    m_outputAnchor = (m_junctionPosition + targetTop) / 2.0;

    m_linePath = QPainterPath();
    m_commonPath = QPainterPath();
    m_sourcePaths.clear();
    m_sourceArrowPaths.clear();
    m_sourceAnnotationAnchors.clear();
    m_verticalHandles.clear();
    m_horizontalHandles.clear();
    int leftIndex = 0;
    int rightIndex = 0;
    int entryIndex = 0;
    for (auto* product : m_recycleProducts) {
        const QPointF end = product->unconnectedEndScenePosition();
        const bool left = product->side() == ProductSide::Left
            || (product->side() == ProductSide::Middle
                && end.x() <= m_targetUnit->scenePos().x());
        appendSourcePath(m_linePath, product->streamId(), product->sourceAnchorScenePosition(), end, left,
                         left ? leftIndex++ : rightIndex++, entryIndex++);
    }
    for (auto* merge : m_recycleMerges) {
        const QPointF end = merge->outputEndPosition();
        const bool left = end.x() <= m_targetUnit->scenePos().x();
        appendSourcePath(m_linePath, merge->outputStreamId(), merge->mergePosition(), end, left,
                         left ? leftIndex++ : rightIndex++, entryIndex++);
    }
    if (hasExternalFeed()) {
        m_processAnnotationAnchor = QPointF();
        m_commonPath.moveTo(freshFeedStart);
    } else if (hasProcessSource()) {
        const QPointF processStart = processSourceAnchor();
        m_processAnnotationAnchor = QPointF(
            processStart.x(), (processStart.y() + m_junctionPosition.y()) / 2.0);
        m_commonPath.moveTo(processStart);
        m_commonPath.lineTo(processStart.x(), m_junctionPosition.y());
        m_commonPath.lineTo(m_junctionPosition);
        m_commonPath.moveTo(m_junctionPosition);
    } else {
        // Several source branches can jointly feed a unit without either a
        // designated process source or fresh external feed.  The branches
        // already terminate at this junction; only draw its common outlet.
        m_processAnnotationAnchor = QPointF();
        m_commonPath.moveTo(m_junctionPosition);
    }
    m_commonPath.lineTo(targetTop);
    m_linePath.addPath(m_commonPath);

    m_arrowPath = QPainterPath();
    m_arrowPath.moveTo(targetTop.x() - FlotationGeometry::ArrowHalfWidth,
                       targetTop.y() - FlotationGeometry::ArrowHeight);
    m_arrowPath.lineTo(targetTop.x() + FlotationGeometry::ArrowHalfWidth,
                       targetTop.y() - FlotationGeometry::ArrowHeight);
    m_arrowPath.lineTo(targetTop);
    m_arrowPath.closeSubpath();

    QPainterPath bounds = m_linePath;
    bounds.addPath(m_arrowPath);
    for (const auto& sourceArrow : m_sourceArrowPaths) bounds.addPath(sourceArrow);
    bounds.addEllipse(m_junctionPosition, JunctionRadius, JunctionRadius);
    setPath(bounds);
    update();
}

} // namespace afs
