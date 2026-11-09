#include "graphics/items/MergeJunctionItem.h"
#include "graphics/FlotationGeometry.h"
#include "graphics/items/ProductLineItem.h"
#include <QPainterPathStroker>
#include <QSet>

namespace afs {
QString MergeJunctionItem::streamAtScenePosition(const QPointF& position) const {
    const auto point = mapFromScene(position);
    QPainterPathStroker stroker; stroker.setWidth(FlotationGeometry::RouteHitWidth);
    QSet<QString> matches;
    const auto hit = [&](const QPainterPath& path) { return stroker.createStroke(path).contains(point); };
    for (auto* product : m_products) {
        QPainterPath branch(product->sourceAnchorScenePosition());
        branch.lineTo(product->unconnectedEndScenePosition().x(), m_mergeY); branch.lineTo(m_mergeX, m_mergeY);
        if (hit(branch)) matches.insert(product->streamId());
    }
    if (!m_feedJunction || m_dragging) {
        const auto end = m_dragging ? m_dragEnd : m_outputEnd;
        QPainterPath output(mergePosition());
        if (m_targetUnit && !m_dragging) output.lineTo(end.x(), m_mergeY);
        output.lineTo(end);
        if (hit(output) || m_arrowPath.contains(point)) matches.insert(outputStreamId());
    }
    return matches.size() == 1 ? *matches.cbegin() : QString{};
}
}
