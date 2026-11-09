#include "graphics/items/FeedJunctionItem.h"
#include "graphics/FlotationGeometry.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/ProductLineItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include <QPainterPathStroker>
#include <QSet>

namespace afs {
QString FeedJunctionItem::streamAtScenePosition(const QPointF& position) const {
    const auto point = mapFromScene(position);
    QPainterPathStroker stroker; stroker.setWidth(FlotationGeometry::RouteHitWidth);
    QSet<QString> matches;
    const auto hit = [&](const QPainterPath& path) { return stroker.createStroke(path).contains(point); };
    for (auto it = m_sourcePaths.cbegin(); it != m_sourcePaths.cend(); ++it)
        if (hit(it.value()) || m_sourceArrowPaths.value(it.key()).contains(point)) matches.insert(it.key());

    QPainterPath process;
    QString processId;
    if (hasExternalFeed()) {
        process.moveTo(m_targetUnit->inputLine()->startScenePosition());
        process.lineTo(m_junctionPosition); processId = externalFeedStreamId();
    } else if (hasProcessSource()) {
        const auto start = processSourceAnchor();
        process.moveTo(start); process.lineTo(start.x(), m_junctionPosition.y()); process.lineTo(m_junctionPosition);
        processId = m_processProduct ? m_processProduct->streamId() : m_processMerge->outputStreamId();
    }
    if (!processId.isEmpty() && hit(process)) matches.insert(processId);
    QPainterPath output(m_junctionPosition); output.lineTo(m_targetUnit->inputAnchorScenePosition());
    if (hit(output) || m_arrowPath.contains(point)) matches.insert(outputStreamId());
    // Shared segments and crossing points do not identify one stream.
    return matches.size() == 1 ? *matches.cbegin() : QString{};
}
}
