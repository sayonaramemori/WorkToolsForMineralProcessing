#include "graphics/items/FlotationUnitItem.h"
#include "graphics/FlotationGeometry.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/ProductLineItem.h"

#include <QApplication>
#include <QPainter>
#include <QPainterPathStroker>
#include <QPalette>
#include <QStyleOptionGraphicsItem>
#include <utility>
#include <algorithm>

namespace {
constexpr double kMinimumWidth = 160.0;
constexpr double kMaximumWidth = 720.0;
}

namespace afs {

FlotationUnitItem::FlotationUnitItem(FlotationUnit unit) : m_unit(std::move(unit)) {
    setPos(m_unit.position);
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
    setCursor(Qt::OpenHandCursor);
    m_inputLine = new InputLineItem(this);
    m_leftProduct = new ProductLineItem(this, ProductSide::Left);
    m_rightProduct = new ProductLineItem(this, ProductSide::Right);
}

bool FlotationUnitItem::adjustWidth(double delta) {
    const double newWidth = std::clamp(m_unit.width + delta, kMinimumWidth, kMaximumWidth);
    if (qFuzzyCompare(newWidth, m_unit.width)) return false;
    const double oldWidth = m_unit.width;
    prepareGeometryChange();
    m_unit.width = newWidth;
    update();
    m_leftProduct->updatePath();
    m_rightProduct->updatePath();
    if (auto* flowsheet = dynamic_cast<FlowsheetScene*>(scene()))
        flowsheet->sourceWidthChanged(this, oldWidth);
    return true;
}

QList<ProductLineItem*> FlotationUnitItem::products() const {
    return {m_leftProduct, m_rightProduct};
}

QRectF FlotationUnitItem::boundingRect() const {
    const double halfWidth = m_unit.width / 2.0;
    return {-halfWidth - 4.0, -4.0, m_unit.width + 8.0,
            FlotationGeometry::DoubleLineGap + 8.0};
}

QPainterPath FlotationUnitItem::shape() const {
    const double halfWidth = m_unit.width / 2.0;
    QPainterPath body;
    body.addRect(QRectF(-halfWidth, 0, m_unit.width, FlotationGeometry::DoubleLineGap));
    return body;
}

void FlotationUnitItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setRenderHint(QPainter::Antialiasing);
    const QPalette palette = QApplication::palette();
    const QColor color = isSelected() ? palette.color(QPalette::Highlight)
                                      : palette.color(QPalette::Text);
    const double halfWidth = m_unit.width / 2.0;
    painter->setPen(QPen(color, FlotationGeometry::BodyLineWidth,
                         Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    painter->setBrush(Qt::NoBrush);
    painter->drawLine(QPointF(-halfWidth, 0), QPointF(-halfWidth, FlotationGeometry::DoubleLineGap));
    painter->drawLine(QPointF(halfWidth, 0), QPointF(halfWidth, FlotationGeometry::DoubleLineGap));
    painter->drawLine(QPointF(-halfWidth, FlotationGeometry::DoubleLineGap),
                      QPointF(halfWidth, FlotationGeometry::DoubleLineGap));

    // 参考论文图：槽体最上方横线更粗，并与两侧竖线闭合。
    painter->setPen(QPen(color, FlotationGeometry::TopLineWidth,
                         Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    painter->drawLine(QPointF(-halfWidth, 0), QPointF(halfWidth, 0));
}

QVariant FlotationUnitItem::itemChange(GraphicsItemChange change, const QVariant& value) {
    if (change == ItemPositionChange && scene()) {
        if (auto* flowsheet = dynamic_cast<FlowsheetScene*>(scene()))
            flowsheet->moveConnectedPeers(this, value.toPointF() - pos());
    }
    if (change == ItemPositionHasChanged)
        m_unit.position = value.toPointF();
    if (change == ItemSelectedHasChanged)
        setCursor(value.toBool() ? Qt::ClosedHandCursor : Qt::OpenHandCursor);
    const QVariant result = QGraphicsItem::itemChange(change, value);
    if (change == ItemPositionHasChanged) {
        if (auto* flowsheet = dynamic_cast<FlowsheetScene*>(scene()))
            flowsheet->refreshConnections();
    }
    return result;
}

} // namespace afs
