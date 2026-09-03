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
#include <QGraphicsSceneMouseEvent>
#include <QInputDialog>
#include <utility>
#include <algorithm>
#include <cmath>

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

bool FlotationUnitItem::setLeftSplitPercent(double percent) {
    if (m_unit.kind != UnitKind::BinarySplitter || !std::isfinite(percent)
        || percent <= 0.0 || percent >= 100.0 || qFuzzyCompare(percent, m_unit.leftSplitPercent))
        return false;
    m_unit.leftSplitPercent = percent;
    update();
    if (auto* flowsheet = dynamic_cast<FlowsheetScene*>(scene()))
        flowsheet->notifyTopologyChanged();
    return true;
}

QList<ProductLineItem*> FlotationUnitItem::products() const {
    return {m_leftProduct, m_rightProduct};
}

QRectF FlotationUnitItem::boundingRect() const {
    const double halfWidth = m_unit.width / 2.0;
    if (m_unit.kind == UnitKind::BinarySplitter)
        return {-halfWidth - 4.0, -22.0, m_unit.width + 8.0, 86.0};
    return {-halfWidth - 4.0, -4.0, m_unit.width + 8.0,
            FlotationGeometry::DoubleLineGap + 8.0};
}

QPainterPath FlotationUnitItem::shape() const {
    const double halfWidth = m_unit.width / 2.0;
    QPainterPath body;
    if (m_unit.kind == UnitKind::BinarySplitter) {
        body.addRect(QRectF(-halfWidth, -20.0, m_unit.width, 42.0));
        return body;
    }
    body.addRect(QRectF(-halfWidth, 0, m_unit.width, FlotationGeometry::DoubleLineGap));
    return body;
}

void FlotationUnitItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setRenderHint(QPainter::Antialiasing);
    const QPalette palette = QApplication::palette();
    const QColor color = isSelected() ? palette.color(QPalette::Highlight)
                                      : palette.color(QPalette::Text);
    const double halfWidth = m_unit.width / 2.0;
    if (m_unit.kind == UnitKind::BinarySplitter) {
        painter->setPen(QPen(color, FlotationGeometry::BodyLineWidth,
                             Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        painter->setBrush(Qt::NoBrush);
        const double radius = 18.0;
        QPainterPath diamond;
        diamond.moveTo(0, -radius);
        diamond.lineTo(radius, 0);
        diamond.lineTo(0, radius);
        diamond.lineTo(-radius, 0);
        diamond.closeSubpath();
        painter->drawPath(diamond);
        painter->drawLine(QPointF(-radius, 0), QPointF(-halfWidth, 0));
        painter->drawLine(QPointF(radius, 0), QPointF(halfWidth, 0));
        painter->drawText(QRectF(-90, radius + 4, 180, 42), Qt::AlignHCenter | Qt::AlignTop,
                          QStringLiteral("二分流  左 %1% / 右 %2%")
                              .arg(m_unit.leftSplitPercent, 0, 'f', 1)
                              .arg(100.0 - m_unit.leftSplitPercent, 0, 'f', 1));
        return;
    }
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

void FlotationUnitItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) {
    if (m_unit.kind != UnitKind::BinarySplitter) {
        QGraphicsItem::mouseDoubleClickEvent(event);
        return;
    }
    bool accepted = false;
    const double value = QInputDialog::getDouble(nullptr, QStringLiteral("设置二分流比例"),
        QStringLiteral("左支路比例（右支路自动补足至 100%）"),
        m_unit.leftSplitPercent, 0.1, 99.9, 1, &accepted);
    if (accepted) setLeftSplitPercent(value);
    event->accept();
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
