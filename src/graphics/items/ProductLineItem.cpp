#include "graphics/items/ProductLineItem.h"
#include "graphics/ThickenerGeometry.h"
#include "graphics/FlotationGeometry.h"
#include "graphics/items/FlotationUnitItem.h"
#include <QApplication>
#include <QBrush>
#include <QGraphicsSimpleTextItem>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPalette>
#include <QPen>
#include <algorithm>

namespace afs {

ProductLineItem::ProductLineItem(FlotationUnitItem* sourceUnit, ProductSide side)
    : QGraphicsPathItem(sourceUnit), m_sourceUnit(sourceUnit), m_side(side),
      m_streamId(sourceUnit->unit().id + productSideSuffix(side)) {
    setFlag(ItemIsSelectable);
    setFlag(ItemIsFocusable);
    setAcceptHoverEvents(true);
    setZValue(2.0);
    m_nameLabel = new QGraphicsSimpleTextItem(this);
    m_nameLabel->setAcceptedMouseButtons(Qt::NoButton);
    QFont labelFont = QApplication::font();
    labelFont.setPointSizeF(9.0);
    m_nameLabel->setFont(labelFont);
    m_nameLabel->setZValue(1.0);
    m_nameLabel->hide();
    updatePath();
}

double ProductLineItem::sideX() const {
    if (hasCenteredVerticalPorts(m_sourceUnit->unit().kind)) return 0.0;
    return productSideDirection(m_side) * m_sourceUnit->unit().width / 2.0;
}

QPointF ProductLineItem::sourceAnchorPosition() const {
    return {sideX(), m_sourceUnit->unit().kind == UnitKind::Thickener ? ThickenerGeometry::Bottom
        : m_sourceUnit->unit().kind == UnitKind::BallMill
        ? m_sourceUnit->unit().ballMillDiameter / 2.0 : 0.0};
}

double ProductLineItem::defaultRouteY(const QPointF& end) const {
    const double startY = sourceAnchorPosition().y();
    const double outletEndY = startY + terminalLength();
    // Honor a shortened outlet when choosing the bend. If even that outlet
    // would pass the target inlet (including upward connections), use the
    // midpoint of the endpoints rather than doubling back through the unit.
    const bool outletFits = outletEndY > std::min(startY, end.y())
        && outletEndY < std::max(startY, end.y());
    return ((outletFits ? outletEndY : startY) + end.y()) / 2.0;
}

QPointF ProductLineItem::unconnectedEndScenePosition() const {
    return m_sourceUnit->mapToScene(sourceAnchorPosition() + QPointF(0, terminalLength()));
}

QPointF ProductLineItem::sourceAnchorScenePosition() const {
    return m_sourceUnit->mapToScene(sourceAnchorPosition());
}

QPainterPath ProductLineItem::shape() const {
    QPainterPathStroker stroker;
    stroker.setWidth(FlotationGeometry::RouteHitWidth);
    const auto hit = stroker.createStroke(path());
    // Only unconnected straight routes have a filled arrow. Filling an open
    // connected elbow would create a selectable polygon beside the line.
    return (!m_targetUnit || m_dragging) ? hit.united(path()) : hit;
}

QVariant ProductLineItem::itemChange(GraphicsItemChange change, const QVariant& value) {
    const QVariant result = QGraphicsPathItem::itemChange(change, value);
    if (change == ItemSelectedHasChanged) refreshAppearance();
    return result;
}

void ProductLineItem::setTargetUnit(FlotationUnitItem* target) {
    prepareGeometryChange();
    m_targetUnit = target;
    m_dragging = false;
    updatePath();
}

void ProductLineItem::setMergeJunction(MergeJunctionItem* junction) {
    m_mergeJunction = junction;
    m_dragging = false;
    if (junction) setSelected(false);
    setVisible(junction == nullptr);
    if (!junction) updatePath();
}

void ProductLineItem::setFeedJunction(FeedJunctionItem* junction) {
    m_feedJunction = junction;
    m_dragging = false;
    if (junction) setSelected(false);
    setVisible(junction == nullptr);
    if (!junction) updatePath();
}

void ProductLineItem::setDropHighlighted(bool highlighted) {
    if (m_dropHighlighted == highlighted) return;
    m_dropHighlighted = highlighted;
    refreshAppearance();
}

void ProductLineItem::updatePath() {
    prepareGeometryChange();
    const double x = sideX();
    QPointF end = sourceAnchorPosition() + QPointF(0, terminalLength());
    if (m_dragging) {
        end = m_dragEnd;
    } else if (m_targetUnit) {
        end = m_sourceUnit->mapFromScene(m_targetUnit->inputAnchorScenePosition());
    }
    QPainterPath p(sourceAnchorPosition());
    if (m_targetUnit && !m_dragging) {
        const double routeY = m_manualRouteY.value_or(defaultRouteY(end));
        p.lineTo(x, routeY);
        p.lineTo(end.x(), routeY);
        p.lineTo(end);
    } else {
        p.lineTo(end);
    }
    if (!m_targetUnit || m_dragging) {
        QPainterPath arrow;
        arrow.moveTo(end.x() - FlotationGeometry::ArrowHalfWidth, end.y());
        arrow.lineTo(end.x() + FlotationGeometry::ArrowHalfWidth, end.y());
        arrow.lineTo(end.x(), end.y() + FlotationGeometry::ArrowHeight);
        arrow.closeSubpath();
        p.addPath(arrow);
    }
    setPath(p);
    const QRectF labelBounds = m_nameLabel->boundingRect();
    m_nameLabel->setPos(end.x() - labelBounds.width() / 2.0,
                        end.y() + FlotationGeometry::ArrowHeight + 5.0);
    m_nameLabel->setVisible(!m_nameLabel->text().isEmpty() && isAvailable() && !m_dragging);
    refreshAppearance();
}

void ProductLineItem::setManualRouteY(std::optional<double> y) {
    m_manualRouteY = y;
    updatePath();
}

double ProductLineItem::terminalLength() const {
    return m_terminalLength.value_or(m_sourceUnit->unit().bodyHeight);
}

bool ProductLineItem::setTerminalLength(std::optional<double> length) {
    if (length && (!std::isfinite(*length) || *length < 30.0 || *length > 5000.0))
        return false;
    if (m_terminalLength == length) return false;
    m_terminalLength = length;
    updatePath();
    return true;
}

bool ProductLineItem::adjustTerminalLength(double delta) {
    if (!isAvailable() || !std::isfinite(delta)) return false;
    const double adjusted = std::clamp(terminalLength() + delta, 30.0, 5000.0);
    if (qFuzzyCompare(adjusted, terminalLength())) return false;
    m_terminalLength = adjusted;
    updatePath();
    return true;
}

void ProductLineItem::setProductName(const QString& name) {
    m_nameLabel->setText(name);
    updatePath();
}

void ProductLineItem::setTextSettings(const AnnotationTextSettings& settings) {
    m_textSettings = settings;
    QFont font = m_nameLabel->font();
    font.setPointSize(settings.pointSize);
    font.setBold(settings.bold);
    m_nameLabel->setFont(font);
    updatePath();
}

void ProductLineItem::refreshAppearance() {
    const QPalette palette = QApplication::palette();
    const QColor color = (isSelected() || m_dropHighlighted) ? palette.color(QPalette::Highlight)
                                      : palette.color(QPalette::Text);
    setPen(QPen(color, isSelected() ? 3.2 : FlotationGeometry::BodyLineWidth,
                Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    // Connected paths are open orthogonal polylines. When their two ends do
    // not share an x-coordinate (for example a fixed-position storage pool),
    // a solid brush makes QGraphicsPathItem close and fill that polyline as a
    // large triangle. Only terminal/dragging paths contain a closed arrowhead.
    setBrush(isConnected() ? Qt::NoBrush : QBrush(color));
    m_nameLabel->setBrush(m_textSettings.color.isValid() ? m_textSettings.color
                                                         : palette.color(QPalette::Text));
    update();
}

void ProductLineItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                            QWidget* widget) {
    QGraphicsPathItem::paint(painter, option, widget);
    if (!isSelected() || !isConnected()) return;
    const QPointF end = mapFromScene(m_targetUnit->inputAnchorScenePosition());
    const double routeY = m_manualRouteY.value_or(defaultRouteY(end));
    painter->setPen(QPen(QApplication::palette().color(QPalette::Highlight), 1.5));
    painter->setBrush(QApplication::palette().color(QPalette::Base));
    painter->drawRect(QRectF(QPointF((sideX() + end.x()) / 2.0 - 4.0, routeY - 4.0), QSizeF(8, 8)));
}

} // namespace afs
