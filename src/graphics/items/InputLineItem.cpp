#include "graphics/items/InputLineItem.h"
#include "graphics/FlotationGeometry.h"
#include "graphics/items/FlotationUnitItem.h"

#include <QApplication>
#include <QPalette>
#include <QPainterPathStroker>
#include <QPen>

namespace afs {

InputLineItem::InputLineItem(FlotationUnitItem* unit)
    : QGraphicsLineItem(unit), m_unit(unit) {
    setLine(0, -FlotationGeometry::InputHeight, 0, 0);
    setPen(QPen(QApplication::palette().color(QPalette::Text),
                FlotationGeometry::BodyLineWidth, Qt::SolidLine, Qt::SquareCap));
    setZValue(1.0);
    setFlag(ItemIsSelectable);
}

QPainterPath InputLineItem::shape() const {
    QPainterPath path;
    path.moveTo(line().p1());
    path.lineTo(line().p2());
    QPainterPathStroker stroker;
    stroker.setWidth(FlotationGeometry::InputHitWidth);
    return stroker.createStroke(path);
}

void InputLineItem::setSourceProduct(ProductLineItem* product) {
    m_sourceProduct = product;
    setVisible(m_dropHighlighted
               || (product == nullptr && m_sourceMerge == nullptr && m_feedJunction == nullptr));
}

void InputLineItem::setSourceMerge(MergeJunctionItem* merge) {
    m_sourceMerge = merge;
    setVisible(m_dropHighlighted
               || (merge == nullptr && m_sourceProduct == nullptr && m_feedJunction == nullptr));
}

void InputLineItem::setFeedJunction(FeedJunctionItem* junction) {
    m_feedJunction = junction;
    setVisible(m_dropHighlighted
               || (junction == nullptr && m_sourceProduct == nullptr && m_sourceMerge == nullptr));
}

void InputLineItem::setDropHighlighted(bool highlighted) {
    if (m_dropHighlighted == highlighted) return;
    m_dropHighlighted = highlighted;
    setVisible(highlighted
               || (m_sourceProduct == nullptr && m_sourceMerge == nullptr && m_feedJunction == nullptr));
    refreshAppearance();
}

void InputLineItem::refreshAppearance() {
    const QPalette palette = QApplication::palette();
    setPen(QPen((m_dropHighlighted || isSelected()) ? palette.color(QPalette::Highlight)
                                  : palette.color(QPalette::Text),
                (m_dropHighlighted || isSelected()) ? 4.0 : FlotationGeometry::BodyLineWidth,
                Qt::SolidLine, Qt::SquareCap));
}

QVariant InputLineItem::itemChange(GraphicsItemChange change, const QVariant& value) {
    const QVariant result = QGraphicsLineItem::itemChange(change, value);
    if (change == ItemSelectedHasChanged) refreshAppearance();
    return result;
}

} // namespace afs
