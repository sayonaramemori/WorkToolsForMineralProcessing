#include "graphics/items/ProductLineItem.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <cmath>

namespace afs {

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
    setFocus();
    refreshAppearance();
    if (isConnected()) {
        m_routeEditing = true;
        if (!m_manualRouteY) {
            const QPointF end = mapFromScene(m_targetUnit->inputAnchorScenePosition());
            m_manualRouteY = defaultRouteY(end);
        }
    } else if (isAvailable()) {
        m_dragging = true;
        m_dragEnd = mapFromScene(event->scenePos());
        updatePath();
    }
    event->accept();
}

void ProductLineItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
    if (m_routeEditing) {
        m_manualRouteY = std::round(event->pos().y() / 10.0) * 10.0;
        updatePath();
        event->accept();
        return;
    }
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
    if (m_routeEditing) {
        m_routeEditing = false;
        if (auto* flowsheet = dynamic_cast<FlowsheetScene*>(scene())) flowsheet->notifyRouteChanged();
        updatePath();
        event->accept();
        return;
    }
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
            flowsheet->connectProduct(this, targetInput,
                                      event->modifiers() & Qt::ControlModifier);
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

void ProductLineItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) {
    if (isConnected()) {
        m_manualRouteY = std::round(event->pos().y() / 10.0) * 10.0;
        updatePath();
        if (auto* flowsheet = dynamic_cast<FlowsheetScene*>(scene())) flowsheet->notifyRouteChanged();
        event->accept();
        return;
    }
    QGraphicsPathItem::mouseDoubleClickEvent(event);
}

void ProductLineItem::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Delete && m_manualRouteY) {
        setManualRouteY(std::nullopt);
        if (auto* flowsheet = dynamic_cast<FlowsheetScene*>(scene())) flowsheet->notifyRouteChanged();
        event->accept();
        return;
    }
    QGraphicsPathItem::keyPressEvent(event);
}

} // namespace afs
