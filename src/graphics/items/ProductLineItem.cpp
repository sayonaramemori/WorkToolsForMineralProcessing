#include "graphics/items/ProductLineItem.h"
#include "graphics/FlotationGeometry.h"
#include "graphics/items/FlotationUnitItem.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/MergeJunctionItem.h"

#include <QApplication>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSimpleTextItem>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPalette>
#include <QPen>

namespace afs {

ProductLineItem::ProductLineItem(FlotationUnitItem* sourceUnit, ProductSide side)
    : QGraphicsPathItem(sourceUnit), m_sourceUnit(sourceUnit), m_side(side),
      m_streamId(sourceUnit->unit().id + (side == ProductSide::Left ? ":left" : ":right")) {
    setFlag(ItemIsSelectable);
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
    return static_cast<int>(m_side) * m_sourceUnit->unit().width / 2.0;
}

QPointF ProductLineItem::unconnectedEndScenePosition() const {
    return m_sourceUnit->mapToScene(QPointF(sideX(), m_sourceUnit->unit().bodyHeight));
}

QPointF ProductLineItem::sourceAnchorScenePosition() const {
    return m_sourceUnit->mapToScene(QPointF(sideX(), 0));
}

QPainterPath ProductLineItem::shape() const {
    QPainterPathStroker stroker;
    stroker.setWidth(FlotationGeometry::HitWidth);
    QPainterPath hit = stroker.createStroke(path());
    hit.addPath(path());
    return hit;
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
    QPointF end(x, m_sourceUnit->unit().bodyHeight);
    if (m_dragging) {
        end = m_dragEnd;
    } else if (m_targetUnit) {
        end = m_sourceUnit->mapFromScene(m_targetUnit->mapToScene(QPointF(0, 0)));
    }
    QPainterPath p(QPointF(x, 0));
    p.lineTo(end);
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
    setBrush(color);
    m_nameLabel->setBrush(m_textSettings.color.isValid() ? m_textSettings.color
                                                         : palette.color(QPalette::Text));
    update();
}

void ProductLineItem::setHighlightedInput(InputLineItem* input) {
    if (m_highlightedInput == input) return;
    if (m_highlightedInput) m_highlightedInput->setDropHighlighted(false);
    m_highlightedInput = input;
    if (m_highlightedInput) m_highlightedInput->setDropHighlighted(true);
}

void ProductLineItem::setHighlightedProduct(ProductLineItem* product) {
    if (m_highlightedProduct == product) return;
    if (m_highlightedProduct) m_highlightedProduct->setDropHighlighted(false);
    m_highlightedProduct = product;
    if (m_highlightedProduct) m_highlightedProduct->setDropHighlighted(true);
}

void ProductLineItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        QGraphicsPathItem::mousePressEvent(event);
        return;
    }
    if (!(event->modifiers() & Qt::ControlModifier)) scene()->clearSelection();
    setSelected(true);
    refreshAppearance();
    if (isAvailable()) {
        m_dragging = true;
        m_dragEnd = mapFromScene(event->scenePos());
        updatePath();
    }
    event->accept();
}

void ProductLineItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
    if (!m_dragging) {
        event->accept();
        return;
    }
    m_dragEnd = mapFromScene(event->scenePos());
    InputLineItem* candidate = nullptr;
    ProductLineItem* productCandidate = nullptr;
    MergeJunctionItem* mergeCandidate = nullptr;
    for (auto* item : scene()->items(event->scenePos())) {
        auto* input = dynamic_cast<InputLineItem*>(item);
        if (input && input->isVisible() && input->unit() != m_sourceUnit) {
            candidate = input;
            break;
        }
        auto* product = dynamic_cast<ProductLineItem*>(item);
        if (product && product != this && product->isAvailable()
            && product->sourceUnit() != m_sourceUnit) productCandidate = product;
        auto* merge = dynamic_cast<MergeJunctionItem*>(item);
        if (merge && merge->isAvailable()) mergeCandidate = merge;
    }
    if (!candidate) {
        if (auto* flowsheet = dynamic_cast<FlowsheetScene*>(scene())) {
            auto* hiddenInput = flowsheet->inputAtDropPosition(event->scenePos());
            if (hiddenInput && hiddenInput->unit() != m_sourceUnit) candidate = hiddenInput;
        }
    }
    setHighlightedInput(candidate);
    setHighlightedProduct(candidate ? nullptr : productCandidate);
    m_highlightedMerge = (candidate || productCandidate) ? nullptr : mergeCandidate;
    updatePath();
    event->accept();
}

void ProductLineItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
    if (!m_dragging) {
        event->accept();
        return;
    }
    InputLineItem* targetInput = m_highlightedInput;
    ProductLineItem* targetProduct = m_highlightedProduct;
    MergeJunctionItem* targetMerge = m_highlightedMerge;
    setHighlightedInput(nullptr);
    setHighlightedProduct(nullptr);
    m_highlightedMerge = nullptr;
    m_dragging = false;
    if (targetInput) {
        if (auto* flowsheet = dynamic_cast<FlowsheetScene*>(scene()))
            flowsheet->connectProduct(this, targetInput);
    } else if (targetProduct) {
        if (auto* flowsheet = dynamic_cast<FlowsheetScene*>(scene()))
            flowsheet->mergeProducts(this, targetProduct);
    } else if (targetMerge) {
        if (auto* flowsheet = dynamic_cast<FlowsheetScene*>(scene()))
            flowsheet->addProductToMerge(this, targetMerge);
    }
    // Always restore the canonical path. A drop target can become invalid
    // between hover and release, in which case no scene operation updates it.
    updatePath();
    event->accept();
}

} // namespace afs
