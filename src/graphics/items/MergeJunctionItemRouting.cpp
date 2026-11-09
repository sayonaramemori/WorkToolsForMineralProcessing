#include "graphics/items/MergeJunctionItem.h"
#include "graphics/FlotationGeometry.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/ProductLineItem.h"
#include <QGraphicsSimpleTextItem>
#include <QPainterPathStroker>
#include <algorithm>
#include <limits>

namespace afs {
namespace {
constexpr double kMergeClearance = 50.0;
constexpr double kOutputLength = 50.0;
}

QPainterPath MergeJunctionItem::shape() const {
    QPainterPathStroker stroker;
    stroker.setWidth(FlotationGeometry::RouteHitWidth);
    // Open routing paths must not contribute an implicitly closed fill: it
    // can cancel the stroked hit area or make empty space selectable.
    return stroker.createStroke(m_linePath).united(m_arrowPath);
}

void MergeJunctionItem::updatePath() {
    prepareGeometryChange();
    double endXSum = 0.0;
    double maximumEndY = -std::numeric_limits<double>::infinity();
    for (auto* product : m_products) {
        const auto end = product->unconnectedEndScenePosition();
        endXSum += end.x();
        maximumEndY = std::max(maximumEndY, end.y());
    }
    m_mergeX = endXSum / static_cast<double>(std::max<qsizetype>(1, m_products.size()));
    m_mergeY = m_manualMergeY.value_or(maximumEndY + kMergeClearance);

    m_linePath = QPainterPath();
    for (auto* product : m_products) {
        const auto start = product->sourceAnchorScenePosition();
        const auto end = product->unconnectedEndScenePosition();
        m_linePath.moveTo(start);
        m_linePath.lineTo(end.x(), m_mergeY);
        m_linePath.lineTo(m_mergeX, m_mergeY);
    }
    m_outputEnd = m_targetUnit
        ? m_targetUnit->inputAnchorScenePosition()
        : QPointF(m_mergeX, m_mergeY + kOutputLength);
    m_arrowPath = QPainterPath();
    if (!m_feedJunction || m_dragging) {
        const QPointF end = m_dragging ? m_dragEnd : m_outputEnd;
        m_linePath.moveTo(m_mergeX, m_mergeY);
        if (m_targetUnit && !m_dragging) {
            // A connected merge output follows the same orthogonal routing
            // contract as a regular product line. Directly joining these
            // endpoints produced a diagonal when the target was off-axis.
            m_linePath.lineTo(end.x(), m_mergeY);
            m_linePath.lineTo(end);
        } else {
            m_linePath.lineTo(end);
        }
        if (!m_targetUnit || m_dragging) {
            m_arrowPath.moveTo(end.x() - FlotationGeometry::ArrowHalfWidth, end.y());
            m_arrowPath.lineTo(end.x() + FlotationGeometry::ArrowHalfWidth, end.y());
            m_arrowPath.lineTo(end.x(), end.y() + FlotationGeometry::ArrowHeight);
            m_arrowPath.closeSubpath();
        }
    }
    QPainterPath boundsPath = m_linePath;
    boundsPath.addPath(m_arrowPath);
    setPath(boundsPath);
    const QRectF labelBounds = m_nameLabel->boundingRect();
    m_nameLabel->setPos(m_outputEnd.x() - labelBounds.width() / 2.0,
                        m_outputEnd.y() + FlotationGeometry::ArrowHeight + 5.0);
    m_nameLabel->setVisible(!m_nameLabel->text().isEmpty() && isAvailable() && !m_dragging);
    refreshAppearance();
}

void MergeJunctionItem::setManualMergeY(std::optional<double> y) {
    m_manualMergeY = y;
    updatePath();
}

void MergeJunctionItem::translateManualRoute(const QPointF& delta) {
    if (!m_manualMergeY || qFuzzyIsNull(delta.y())) return;
    m_manualMergeY = *m_manualMergeY + delta.y();
}

} // namespace afs
