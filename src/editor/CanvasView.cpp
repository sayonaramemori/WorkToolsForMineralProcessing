#include "editor/CanvasView.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace afs {

CanvasView::CanvasView(QGraphicsScene* scene, QWidget* parent)
    : QGraphicsView(scene, parent) {
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
}

void CanvasView::wheelEvent(QWheelEvent* event) {
    const int delta = event->angleDelta().y();
    if (delta == 0) {
        event->accept();
        return;
    }

    // 使用连续指数倍率，兼容传统滚轮和高分辨率触控板。
    const qreal requestedFactor = std::pow(1.0015, delta);
    const qreal currentScale = transform().m11();
    const qreal targetScale = std::clamp(
        currentScale * requestedFactor, MinimumScale, MaximumScale);
    scale(targetScale / currentScale, targetScale / currentScale);
    event->accept();
}

void CanvasView::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Delete && m_deleteHandler && m_deleteHandler()) {
        event->accept();
        return;
    }
    if (m_verticalNudgeHandler
        && (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down)) {
        const qreal step = event->modifiers().testFlag(Qt::ShiftModifier) ? 10.0 : 2.0;
        const qreal direction = event->key() == Qt::Key_Up ? -1.0 : 1.0;
        if (m_verticalNudgeHandler(direction * step)) {
            event->accept();
            return;
        }
    }
    QGraphicsView::keyPressEvent(event);
}

void CanvasView::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        m_panning = true;
        m_lastPanPosition = event->position().toPoint();
        viewport()->setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void CanvasView::mouseMoveEvent(QMouseEvent* event) {
    if (m_panning) {
        const QPoint position = event->position().toPoint();
        const QPoint delta = position - m_lastPanPosition;
        m_lastPanPosition = position;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void CanvasView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton && m_panning) {
        m_panning = false;
        viewport()->unsetCursor();
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

} // namespace afs
