#include "graphics/items/MergeJunctionItem.h"
#include "graphics/FlotationGeometry.h"
#include "graphics/items/ProductLineItem.h"
#include "graphics/items/FlotationUnitItem.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/InputLineItem.h"

#include <QApplication>
#include <QPainterPathStroker>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSimpleTextItem>
#include <QLineF>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QKeyEvent>
#include <QMenu>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace afs {

namespace {
constexpr double kMergeClearance = 50.0;
constexpr double kOutputLength = 50.0;
}

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

QPainterPath MergeJunctionItem::shape() const {
    QPainterPathStroker stroker;
    stroker.setWidth(FlotationGeometry::HitWidth);
    QPainterPath hit = stroker.createStroke(path());
    hit.addPath(path());
    return hit;
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
        ? m_targetUnit->mapToScene(QPointF(0, 0))
        : QPointF(m_mergeX, m_mergeY + kOutputLength);
    m_arrowPath = QPainterPath();
    if (!m_feedJunction || m_dragging) {
        const QPointF end = m_dragging ? m_dragEnd : m_outputEnd;
        m_linePath.moveTo(m_mergeX, m_mergeY);
        m_linePath.lineTo(end);
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

void MergeJunctionItem::setHighlightedInput(InputLineItem* input) {
    if (m_highlightedInput == input) return;
    if (m_highlightedInput) m_highlightedInput->setDropHighlighted(false);
    m_highlightedInput = input;
    if (m_highlightedInput) m_highlightedInput->setDropHighlighted(true);
}

void MergeJunctionItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() == Qt::LeftButton && std::abs(event->scenePos().y() - m_mergeY) <= 16.0) {
        if (!(event->modifiers() & Qt::ControlModifier)) scene()->clearSelection();
        setSelected(true);
        setFocus();
        m_routeEditing = true;
        if (!m_manualMergeY) m_manualMergeY = m_mergeY;
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && isAvailable()
        && QLineF(event->scenePos(), m_outputEnd).length() <= 30.0) {
        if (!(event->modifiers() & Qt::ControlModifier)) scene()->clearSelection();
        setSelected(true);
        m_dragging = true;
        m_dragEnd = event->scenePos();
        updatePath();
        event->accept();
        return;
    }
    QGraphicsPathItem::mousePressEvent(event);
}

void MergeJunctionItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
    if (m_routeEditing) {
        m_manualMergeY = std::round(event->scenePos().y() / 10.0) * 10.0;
        updatePath();
        event->accept();
        return;
    }
    if (!m_dragging) {
        QGraphicsPathItem::mouseMoveEvent(event);
        return;
    }
    m_dragEnd = event->scenePos();
    InputLineItem* candidate = nullptr;
    for (auto* item : scene()->items(event->scenePos())) {
        auto* input = dynamic_cast<InputLineItem*>(item);
        if (input && input->isVisible()) {
            candidate = input;
            break;
        }
    }
    if (!candidate) {
        if (auto* flowsheet = dynamic_cast<FlowsheetScene*>(scene()))
            candidate = flowsheet->inputAtDropPosition(event->scenePos());
    }
    setHighlightedInput(candidate);
    updatePath();
    event->accept();
}

void MergeJunctionItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
    if (m_routeEditing) {
        m_routeEditing = false;
        if (auto* flowsheet = dynamic_cast<FlowsheetScene*>(scene())) flowsheet->notifyRouteChanged();
        updatePath();
        event->accept();
        return;
    }
    if (!m_dragging) {
        QGraphicsPathItem::mouseReleaseEvent(event);
        return;
    }
    auto* targetInput = m_highlightedInput;
    setHighlightedInput(nullptr);
    m_dragging = false;
    if (targetInput) {
        if (auto* flowsheet = dynamic_cast<FlowsheetScene*>(scene()))
            flowsheet->connectMerge(this, targetInput);
    }
    updatePath();
    event->accept();
}

void MergeJunctionItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) {
    m_manualMergeY = std::round(event->scenePos().y() / 10.0) * 10.0;
    updatePath();
    if (auto* flowsheet = dynamic_cast<FlowsheetScene*>(scene())) flowsheet->notifyRouteChanged();
    event->accept();
}

void MergeJunctionItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event) {
    QMenu menu;
    auto* reset = menu.addAction(QStringLiteral("恢复自动布线"));
    reset->setEnabled(m_manualMergeY.has_value());
    if (menu.exec(event->screenPos()) == reset) {
        setManualMergeY(std::nullopt);
        if (auto* flowsheet = dynamic_cast<FlowsheetScene*>(scene())) flowsheet->notifyRouteChanged();
    }
    event->accept();
}

void MergeJunctionItem::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Delete && m_manualMergeY) {
        setManualMergeY(std::nullopt);
        if (auto* flowsheet = dynamic_cast<FlowsheetScene*>(scene())) flowsheet->notifyRouteChanged();
        event->accept();
        return;
    }
    QGraphicsPathItem::keyPressEvent(event);
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
