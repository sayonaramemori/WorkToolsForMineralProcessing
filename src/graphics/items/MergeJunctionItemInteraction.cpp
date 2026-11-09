#include "graphics/items/MergeJunctionItem.h"
#include "graphics/CanvasInteraction.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/ProductLineItem.h"
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QLineF>
#include <cmath>

namespace afs {

void MergeJunctionItem::setHighlightedInput(InputLineItem* input) {
    if (m_highlightedInput == input) return;
    if (m_highlightedInput) m_highlightedInput->setDropHighlighted(false);
    m_highlightedInput = input;
    if (m_highlightedInput) m_highlightedInput->setDropHighlighted(true);
}

void MergeJunctionItem::setHighlightedProduct(ProductLineItem* product) {
    if (m_highlightedProduct == product) return;
    if (m_highlightedProduct) m_highlightedProduct->setDropHighlighted(false);
    m_highlightedProduct = product;
    if (m_highlightedProduct) m_highlightedProduct->setDropHighlighted(true);
}

void MergeJunctionItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() == Qt::LeftButton && std::abs(event->scenePos().y() - m_mergeY) <= 16.0) {
        if (!(event->modifiers() & Qt::ControlModifier)) scene()->clearSelection();
        setSelected(true);
        setFocus();
        selectAnnotationStreamAt(event->scenePos());
        m_routeEditing = true;
        if (!m_manualMergeY) m_manualMergeY = m_mergeY;
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && isAvailable()
        && QLineF(event->scenePos(), m_outputEnd).length() <= 30.0) {
        if (!(event->modifiers() & Qt::ControlModifier)) scene()->clearSelection();
        setSelected(true);
        selectAnnotationStreamAt(event->scenePos());
        m_dragging = true;
        m_dragEnd = event->scenePos();
        updatePath();
        event->accept();
        return;
    }
    QGraphicsPathItem::mousePressEvent(event);
    if (event->button() == Qt::LeftButton && isSelected()) selectAnnotationStreamAt(event->scenePos());
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
    ProductLineItem* productCandidate = nullptr;
    for (auto* item : scene()->items(event->scenePos())) {
        auto* input = dynamic_cast<InputLineItem*>(item);
        if (input && input->isVisible()) {
            candidate = input;
            break;
        }
        if (auto* product = dynamic_cast<ProductLineItem*>(item); product && product->isAvailable())
            productCandidate = product;
    }
    if (!candidate) {
        if (auto* host = canvasConnectionHost(scene()))
            candidate = host->inputAtDropPosition(event->scenePos());
    }
    setHighlightedInput(candidate);
    setHighlightedProduct(candidate ? nullptr : productCandidate);
    updatePath();
    event->accept();
}

void MergeJunctionItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
    if (m_routeEditing) {
        m_routeEditing = false;
        if (auto* host = canvasGeometryHost(scene())) host->notifyRouteChanged();
        updatePath();
        event->accept();
        return;
    }
    if (!m_dragging) {
        QGraphicsPathItem::mouseReleaseEvent(event);
        return;
    }
    auto* targetInput = m_highlightedInput;
    auto* targetProduct = m_highlightedProduct;
    setHighlightedInput(nullptr);
    setHighlightedProduct(nullptr);
    m_dragging = false;
    if (targetInput) {
        if (auto* host = canvasConnectionHost(scene()))
            host->connectMerge(this, targetInput,
                                    event->modifiers() & Qt::ControlModifier);
    } else if (targetProduct) {
        if (auto* host = canvasConnectionHost(scene()))
            host->addProductToMerge(targetProduct, this);
    }
    updatePath();
    event->accept();
}

void MergeJunctionItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) {
    m_manualMergeY = std::round(event->scenePos().y() / 10.0) * 10.0;
    updatePath();
    if (auto* host = canvasGeometryHost(scene())) host->notifyRouteChanged();
    event->accept();
}

void MergeJunctionItem::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Delete && m_manualMergeY) {
        setManualMergeY(std::nullopt);
        if (auto* host = canvasGeometryHost(scene())) host->notifyRouteChanged();
        event->accept();
        return;
    }
    QGraphicsPathItem::keyPressEvent(event);
}

} // namespace afs
