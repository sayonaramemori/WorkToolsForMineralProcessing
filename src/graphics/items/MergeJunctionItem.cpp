#include "graphics/items/MergeJunctionItem.h"
#include "graphics/FlotationGeometry.h"
#include <QApplication>
#include <QGraphicsSimpleTextItem>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <utility>

namespace afs {

MergeJunctionItem::MergeJunctionItem(QString id, ProductLineItem* first, ProductLineItem* second)
    : m_id(std::move(id)), m_products{first, second} {
    setFlag(ItemIsSelectable);
    setFlag(ItemIsFocusable);
    setZValue(2.5);
    m_nameLabel = new QGraphicsSimpleTextItem(this);
    m_nameLabel->setAcceptedMouseButtons(Qt::NoButton);
    QFont labelFont = QApplication::font();
    labelFont.setPointSizeF(9.0);
    m_nameLabel->setFont(labelFont);
    m_nameLabel->setZValue(1.0);
    m_nameLabel->hide();
    updatePath();
}

bool MergeJunctionItem::addProduct(ProductLineItem* product) {
    if (!product || m_products.contains(product)) return false;
    m_products.append(product);
    updatePath();
    return true;
}

void MergeJunctionItem::setProductName(const QString& name) {
    m_nameLabel->setText(name);
    updatePath();
}

void MergeJunctionItem::setTextSettings(const AnnotationTextSettings& settings) {
    m_textSettings = settings;
    QFont font = m_nameLabel->font();
    font.setPointSize(settings.pointSize);
    font.setBold(settings.bold);
    m_nameLabel->setFont(font);
    updatePath();
}

void MergeJunctionItem::setFeedJunction(FeedJunctionItem* junction) {
    m_feedJunction = junction;
    m_dragging = false;
    updatePath();
}

void MergeJunctionItem::setTargetUnit(FlotationUnitItem* target) {
    prepareGeometryChange();
    m_targetUnit = target;
    m_dragging = false;
    updatePath();
}

void MergeJunctionItem::refreshAppearance() {
    const QPalette palette = QApplication::palette();
    m_nameLabel->setBrush(m_textSettings.color.isValid() ? m_textSettings.color
                                                         : palette.color(QPalette::Text));
    update();
}

void MergeJunctionItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
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
    if (isSelected()) {
        painter->setPen(QPen(palette.color(QPalette::Highlight), 1.5));
        painter->setBrush(palette.color(QPalette::Base));
        painter->drawRect(QRectF(QPointF(m_mergeX - 4.0, m_mergeY - 4.0), QSizeF(8, 8)));
    }
}

QVariant MergeJunctionItem::itemChange(GraphicsItemChange change, const QVariant& value) {
    const QVariant result = QGraphicsPathItem::itemChange(change, value);
    if (change == ItemSelectedHasChanged) refreshAppearance();
    return result;
}

} // namespace afs
