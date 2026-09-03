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
#include <QKeyEvent>
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
                                        bool routeLeft, int routeIndex) {
    const double offset = routeIndex * 18.0;
    const double automaticX = routeLeft
        ? std::min(end.x(), m_junctionPosition.x()) - kRouteClearance - offset
        : std::max(end.x(), m_junctionPosition.x()) + kRouteClearance + offset;
    const double corridorX = m_manualRouteXs.value(streamId, automaticX);
    path.moveTo(start);
    path.lineTo(end);
    path.lineTo(corridorX, end.y());
    path.lineTo(corridorX, m_junctionPosition.y());
    path.lineTo(m_junctionPosition);
    m_sourceAnnotationAnchors.insert(
        streamId, QPointF(corridorX, (end.y() + m_junctionPosition.y()) / 2.0));
}

void FeedJunctionItem::setManualRouteX(const QString& streamId, std::optional<double> x) {
    if (x) m_manualRouteXs.insert(streamId, *x);
    else m_manualRouteXs.remove(streamId);
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
    m_sourceAnnotationAnchors.clear();
    int leftIndex = 0;
    int rightIndex = 0;
    for (auto* product : m_recycleProducts) {
        const QPointF end = product->unconnectedEndScenePosition();
        const bool left = product->side() == ProductSide::Left
            || (product->side() == ProductSide::Middle
                && end.x() <= m_targetUnit->scenePos().x());
        appendSourcePath(m_linePath, product->streamId(), product->sourceAnchorScenePosition(), end, left,
                         left ? leftIndex++ : rightIndex++);
    }
    for (auto* merge : m_recycleMerges) {
        const QPointF end = merge->outputEndPosition();
        const bool left = end.x() <= m_targetUnit->scenePos().x();
        appendSourcePath(m_linePath, merge->outputStreamId(), merge->mergePosition(), end, left,
                         left ? leftIndex++ : rightIndex++);
    }
    if (hasExternalFeed()) {
        m_linePath.moveTo(freshFeedStart);
    } else {
        const QPointF processStart = processSourceAnchor();
        m_linePath.moveTo(processStart);
        m_linePath.lineTo(processStart.x(), m_junctionPosition.y());
        m_linePath.lineTo(m_junctionPosition);
        m_linePath.moveTo(m_junctionPosition);
    }
    m_linePath.lineTo(targetTop);

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
    const QColor color = isSelected() ? palette.color(QPalette::Highlight)
                                      : palette.color(QPalette::Text);
    painter->setPen(QPen(color, isSelected() ? 3.2 : FlotationGeometry::BodyLineWidth,
                         Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(m_linePath);
    painter->setPen(Qt::NoPen);
    painter->setBrush(color);
    painter->drawPath(m_arrowPath);
    painter->drawEllipse(m_junctionPosition, kJunctionRadius, kJunctionRadius);
    if (isSelected()) {
        painter->setPen(QPen(palette.color(QPalette::Highlight), 1.5));
        painter->setBrush(palette.color(QPalette::Base));
        for (const auto& anchor : m_sourceAnnotationAnchors)
            painter->drawRect(QRectF(anchor - QPointF(4, 4), QSizeF(8, 8)));
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
    double distance = 18.0;
    for (auto it = m_sourceAnnotationAnchors.cbegin(); it != m_sourceAnnotationAnchors.cend(); ++it) {
        const double candidate = std::abs(event->scenePos().x() - it.value().x());
        if (candidate <= distance) { distance = candidate; closest = it.key(); }
    }
    if (!closest.isEmpty()) {
        if (!(event->modifiers() & Qt::ControlModifier)) scene()->clearSelection();
        setSelected(true);
        setFocus();
        m_editingStreamId = closest;
        if (!m_manualRouteXs.contains(closest))
            m_manualRouteXs.insert(closest, m_sourceAnnotationAnchors.value(closest).x());
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
    m_manualRouteXs.insert(m_editingStreamId,
                          std::round(event->scenePos().x() / 10.0) * 10.0);
    updatePath();
    event->accept();
}

void FeedJunctionItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
    if (m_editingStreamId.isEmpty()) {
        QGraphicsPathItem::mouseReleaseEvent(event);
        return;
    }
    m_editingStreamId.clear();
    if (auto* flowsheet = dynamic_cast<FlowsheetScene*>(scene())) flowsheet->notifyRouteChanged();
    event->accept();
}

void FeedJunctionItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event) {
    QMenu menu;
    auto* reset = menu.addAction(QStringLiteral("恢复全部支路自动布线"));
    reset->setEnabled(!m_manualRouteXs.isEmpty());
    if (menu.exec(event->screenPos()) == reset) {
        m_manualRouteXs.clear();
        updatePath();
        if (auto* flowsheet = dynamic_cast<FlowsheetScene*>(scene())) flowsheet->notifyRouteChanged();
    }
    event->accept();
}

void FeedJunctionItem::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Delete && !m_manualRouteXs.isEmpty()) {
        m_manualRouteXs.clear();
        updatePath();
        if (auto* flowsheet = dynamic_cast<FlowsheetScene*>(scene())) flowsheet->notifyRouteChanged();
        event->accept();
        return;
    }
    QGraphicsPathItem::keyPressEvent(event);
}

QVariant FeedJunctionItem::itemChange(GraphicsItemChange change, const QVariant& value) {
    const QVariant result = QGraphicsPathItem::itemChange(change, value);
    if (change == ItemSelectedHasChanged) refreshAppearance();
    return result;
}

} // namespace afs
