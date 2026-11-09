#include "graphics/items/FeedJunctionItem.h"
#include "graphics/FlotationGeometry.h"
#include "editor/FlowsheetScene.h"
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QLineF>
#include <QPainterPathStroker>
#include <cmath>

namespace afs {

void FeedJunctionItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        QGraphicsPathItem::mousePressEvent(event);
        return;
    }
    QString closest;
    EditingAxis axis = EditingAxis::None;
    double distance = 14.0;
    for (auto it = m_verticalHandles.cbegin(); it != m_verticalHandles.cend(); ++it) {
        const double verticalDistance = QLineF(event->scenePos(), it.value()).length();
        if (verticalDistance <= distance) {
            distance = verticalDistance; closest = it.key(); axis = EditingAxis::Horizontal;
        }
        const double horizontalDistance = QLineF(
            event->scenePos(), m_horizontalHandles.value(it.key())).length();
        if (horizontalDistance <= distance) {
            distance = horizontalDistance; closest = it.key(); axis = EditingAxis::Vertical;
        }
    }
    if (closest.isEmpty()) {
        QPainterPathStroker stroker;
        stroker.setWidth(FlotationGeometry::RouteHitWidth);
        for (auto it = m_sourcePaths.cbegin(); it != m_sourcePaths.cend(); ++it) {
            if (stroker.createStroke(it.value()).contains(event->scenePos())) {
                closest = it.key(); break;
            }
        }
    }
    if (!closest.isEmpty()) {
        if (!(event->modifiers() & Qt::ControlModifier)) scene()->clearSelection();
        setSelected(true);
        setFocus();
        selectSourceStream(closest);
        selectAnnotationStreamAt(event->scenePos());
        m_editingStreamId = axis == EditingAxis::None ? QString{} : closest;
        m_editingAxis = axis;
        if (axis == EditingAxis::Horizontal && !m_manualRouteXs.contains(closest))
            m_manualRouteXs.insert(closest, m_verticalHandles.value(closest).x());
        if (axis == EditingAxis::Vertical && !m_manualRouteYs.contains(closest))
            m_manualRouteYs.insert(closest, m_horizontalHandles.value(closest).y());
        update();
        event->accept();
        return;
    }
    QGraphicsPathItem::mousePressEvent(event);
    if (isSelected()) selectAnnotationStreamAt(event->scenePos());
}

void FeedJunctionItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
    if (m_editingStreamId.isEmpty()) {
        QGraphicsPathItem::mouseMoveEvent(event);
        return;
    }
    if (m_editingAxis == EditingAxis::Horizontal)
        m_manualRouteXs.insert(m_editingStreamId,
                              std::round(event->scenePos().x() / 10.0) * 10.0);
    else if (m_editingAxis == EditingAxis::Vertical)
        m_manualRouteYs.insert(m_editingStreamId,
                              std::round(event->scenePos().y() / 10.0) * 10.0);
    updatePath();
    event->accept();
}

void FeedJunctionItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
    if (m_editingStreamId.isEmpty()) {
        QGraphicsPathItem::mouseReleaseEvent(event);
        return;
    }
    m_editingStreamId.clear();
    m_editingAxis = EditingAxis::None;
    if (auto* flowsheet = dynamic_cast<FlowsheetScene*>(scene())) flowsheet->notifyRouteChanged();
    event->accept();
}

void FeedJunctionItem::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Delete
        && (!m_manualRouteXs.isEmpty() || !m_manualRouteYs.isEmpty())) {
        resetManualRoutes();
        if (auto* flowsheet = dynamic_cast<FlowsheetScene*>(scene())) flowsheet->notifyRouteChanged();
        event->accept();
        return;
    }
    QGraphicsPathItem::keyPressEvent(event);
}

} // namespace afs
