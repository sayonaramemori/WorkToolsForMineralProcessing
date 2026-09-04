#include "graphics/items/FeedJunctionItem.h"
#include "graphics/FlotationGeometry.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"
#include "editor/FlowsheetScene.h"

#include <QApplication>
#include <QPainter>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsScene>
#include <QKeyEvent>
#include <QLineF>
#include <QMenu>
#include <QPainterPathStroker>
#include <QPalette>
#include <QPen>
#include <algorithm>
#include <utility>
#include <cmath>

namespace afs {
namespace {
constexpr double kRouteClearance = 80.0;
constexpr double kJunctionRadius = 4.5;
}

FeedJunctionItem::FeedJunctionItem(QString id, ProductLineItem* recycleProduct,
                                   FlotationUnitItem* targetUnit,
                                   ProductLineItem* processProduct,
                                   MergeJunctionItem* processMerge)
    : m_id(std::move(id)), m_recycleProducts{recycleProduct},
      m_processProduct(processProduct), m_processMerge(processMerge), m_targetUnit(targetUnit) {
    setFlag(ItemIsSelectable);
    setFlag(ItemIsFocusable);
    setZValue(2.5);
    updatePath();
}

FeedJunctionItem::FeedJunctionItem(QString id, MergeJunctionItem* recycleMerge,
                                   FlotationUnitItem* targetUnit,
                                   ProductLineItem* processProduct,
                                   MergeJunctionItem* processMerge)
    : m_id(std::move(id)), m_recycleMerges{recycleMerge},
      m_processProduct(processProduct), m_processMerge(processMerge), m_targetUnit(targetUnit) {
    setFlag(ItemIsSelectable);
    setFlag(ItemIsFocusable);
    setZValue(2.5);
    updatePath();
}

QPointF FeedJunctionItem::processSourceAnchor() const {
    return m_processProduct ? m_processProduct->sourceAnchorScenePosition()
                            : m_processMerge->mergePosition();
}

QString FeedJunctionItem::recycleStreamId() const {
    if (auto* product = recycleProduct()) return product->streamId();
    if (auto* merge = recycleMerge()) return merge->outputStreamId();
    return {};
}

bool FeedJunctionItem::addRecycleProduct(ProductLineItem* product) {
    if (!product || m_recycleProducts.contains(product)) return false;
    m_recycleProducts.append(product);
    updatePath();
    return true;
}

bool FeedJunctionItem::addRecycleMerge(MergeJunctionItem* merge) {
    if (!merge || m_recycleMerges.contains(merge)) return false;
    m_recycleMerges.append(merge);
    updatePath();
    return true;
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

QPainterPath FeedJunctionItem::shape() const {
    QPainterPathStroker stroker;
    stroker.setWidth(FlotationGeometry::HitWidth);
    QPainterPath hit = stroker.createStroke(m_linePath);
    hit.addEllipse(m_junctionPosition, kJunctionRadius + 2.0, kJunctionRadius + 2.0);
    return hit;
}

void FeedJunctionItem::updatePath() {
    prepareGeometryChange();
    const QPointF targetTop = m_targetUnit->mapToScene(QPointF(0, 0));
    const QPointF freshFeedStart = m_targetUnit->mapToScene(
        QPointF(0, -FlotationGeometry::InputHeight));
    m_junctionPosition = (targetTop + freshFeedStart) / 2.0;

    m_recycleAnchor = m_junctionPosition;
    m_externalFeedAnchor = hasExternalFeed()
        ? (freshFeedStart + m_junctionPosition) / 2.0 : QPointF();
    m_outputAnchor = (m_junctionPosition + targetTop) / 2.0;

    m_linePath = QPainterPath();
    m_commonPath = QPainterPath();
    m_sourcePaths.clear();
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
        m_commonPath.moveTo(freshFeedStart);
    } else {
        const QPointF processStart = processSourceAnchor();
        m_commonPath.moveTo(processStart);
        m_commonPath.lineTo(processStart.x(), m_junctionPosition.y());
        m_commonPath.lineTo(m_junctionPosition);
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
    bounds.addEllipse(m_junctionPosition, kJunctionRadius, kJunctionRadius);
    setPath(bounds);
    update();
}

void FeedJunctionItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setRenderHint(QPainter::Antialiasing);
    const QPalette palette = QApplication::palette();
    bool anotherFeedSelected = false;
    if (!isSelected() && scene()) {
        for (auto* item : scene()->selectedItems()) {
            if (dynamic_cast<FeedJunctionItem*>(item)) {
                anotherFeedSelected = true;
                break;
            }
        }
    }
    QColor color = isSelected() ? palette.color(QPalette::Highlight)
                                : palette.color(QPalette::Text);
    if (anotherFeedSelected) color.setAlphaF(0.22);
    painter->setPen(QPen(color, FlotationGeometry::BodyLineWidth,
                         Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(m_commonPath);
    for (auto it = m_sourcePaths.cbegin(); it != m_sourcePaths.cend(); ++it) {
        QColor branchColor = color;
        double width = FlotationGeometry::BodyLineWidth;
        if (isSelected() && !m_selectedStreamId.isEmpty()) {
            if (it.key() == m_selectedStreamId) width = 3.2;
            else branchColor.setAlphaF(0.3);
        } else if (isSelected()) {
            width = 2.6;
        }
        painter->setPen(QPen(branchColor, width, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        painter->drawPath(it.value());
    }
    painter->setPen(Qt::NoPen);
    painter->setBrush(color);
    painter->drawPath(m_arrowPath);
    painter->drawEllipse(m_junctionPosition, kJunctionRadius, kJunctionRadius);
    if (isSelected()) {
        painter->setPen(QPen(palette.color(QPalette::Highlight), 1.5));
        painter->setBrush(palette.color(QPalette::Base));
        for (auto it = m_verticalHandles.cbegin(); it != m_verticalHandles.cend(); ++it) {
            painter->drawRect(QRectF(it.value() - QPointF(4, 4), QSizeF(8, 8)));
            const QPointF horizontal = m_horizontalHandles.value(it.key());
            painter->drawRect(QRectF(horizontal - QPointF(4, 4), QSizeF(8, 8)));
        }
    }
}

void FeedJunctionItem::refreshAppearance() {
    update();
}

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
        stroker.setWidth(FlotationGeometry::HitWidth);
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
        m_selectedStreamId = closest;
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

void FeedJunctionItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event) {
    QMenu menu;
    auto* reset = menu.addAction(QStringLiteral("恢复全部支路自动布线"));
    reset->setEnabled(!m_manualRouteXs.isEmpty() || !m_manualRouteYs.isEmpty());
    if (menu.exec(event->screenPos()) == reset) {
        resetManualRoutes();
        if (auto* flowsheet = dynamic_cast<FlowsheetScene*>(scene())) flowsheet->notifyRouteChanged();
    }
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

QVariant FeedJunctionItem::itemChange(GraphicsItemChange change, const QVariant& value) {
    const QVariant result = QGraphicsPathItem::itemChange(change, value);
    if (change == ItemSelectedHasChanged) {
        if (!value.toBool()) m_selectedStreamId.clear();
        refreshAppearance();
        if (scene())
            for (auto* item : scene()->items())
                if (auto* feed = dynamic_cast<FeedJunctionItem*>(item); feed && feed != this)
                    feed->refreshAppearance();
    }
    return result;
}

} // namespace afs
