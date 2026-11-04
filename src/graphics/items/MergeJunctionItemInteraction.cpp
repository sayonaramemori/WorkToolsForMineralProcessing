#include "graphics/items/MergeJunctionItem.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/InputLineItem.h"
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <QKeyEvent>
#include <QLineF>
#include <QMenu>
#include <cmath>

namespace afs {

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
            flowsheet->connectMerge(this, targetInput,
                                    event->modifiers() & Qt::ControlModifier);
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

} // namespace afs
