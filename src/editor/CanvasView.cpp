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
    if (event->key() == Qt::Key_Home) {
        fitAllContents();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Space) {
        // Qt does not expose Space as a keyboard modifier on subsequent arrow
        // events, so keep this small modal state explicitly.
        m_spaceHeld = true;
        event->accept();
        return;
    }
    if (m_spaceHeld && (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right
                        || event->key() == Qt::Key_Up || event->key() == Qt::Key_Down)) {
        const int step = event->modifiers().testFlag(Qt::ShiftModifier) ? 160 : 40;
        QPoint viewportDelta;
        if (event->key() == Qt::Key_Left) viewportDelta.setX(-step);
        if (event->key() == Qt::Key_Right) viewportDelta.setX(step);
        if (event->key() == Qt::Key_Up) viewportDelta.setY(-step);
        if (event->key() == Qt::Key_Down) viewportDelta.setY(step);
        panViewportBy(viewportDelta);
        event->accept();
        return;
    }
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

void CanvasView::fitAllContents() {
    if (!scene()) return;
    QRectF bounds = scene()->itemsBoundingRect();
    if (bounds.isEmpty()) return;
    const qreal padding = std::max<qreal>(60.0, std::max(bounds.width(), bounds.height()) * 0.05);
    bounds.adjust(-padding, -padding, padding, padding);
    fitInView(bounds, Qt::KeepAspectRatio);
}

void CanvasView::keyReleaseEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Space) {
        m_spaceHeld = false;
        event->accept();
        return;
    }
    QGraphicsView::keyReleaseEvent(event);
}

void CanvasView::mousePressEvent(QMouseEvent* event) {
    if (m_spaceHeld && event->button() == Qt::LeftButton) {
        // Space temporarily turns the ordinary left-button drag into a hand
        // tool, matching the familiar CAD/graphics-editor interaction and
        // making trackpad panning possible without a middle button.
        m_panning = true;
        m_spacePanning = true;
        m_lastPanPosition = event->position().toPoint();
        viewport()->setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    if (event->button() == Qt::MiddleButton) {
        m_panning = true;
        m_spacePanning = false;
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
        expandSceneForPan(delta);
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void CanvasView::expandSceneForPan(const QPoint& viewportDelta) {
    if (!scene() || viewportDelta.isNull()) return;
    const QRectF visible = mapToScene(viewport()->rect()).boundingRect();
    if (visible.isEmpty()) return;

    // Moving the view by -delta is what the scrollbar update below performs.
    // Grow the scene around that target viewport before changing the scroll
    // bars, so their finite range can never clip a valid pan at the old edge.
    const qreal scaleX = std::abs(transform().m11());
    const qreal scaleY = std::abs(transform().m22());
    if (scaleX <= 0.0 || scaleY <= 0.0) return;
    const QRectF desired = visible.translated(-viewportDelta.x() / scaleX,
                                              -viewportDelta.y() / scaleY);
    const qreal padding = std::max<qreal>(600.0, std::max(visible.width(), visible.height()));
    const QRectF expanded = scene()->sceneRect().united(
        desired.adjusted(-padding, -padding, padding, padding));
    if (expanded != scene()->sceneRect()) scene()->setSceneRect(expanded);
}

void CanvasView::panViewportBy(const QPoint& viewportDelta) {
    if (viewportDelta.isNull()) return;
    // expandSceneForPan receives a hand-drag delta, whose view movement has
    // the opposite sign to a scrollbar/view movement.
    expandSceneForPan(-viewportDelta);
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() + viewportDelta.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() + viewportDelta.y());
}

void CanvasView::mouseReleaseEvent(QMouseEvent* event) {
    const bool finishesPan = m_panning
        && ((m_spacePanning && event->button() == Qt::LeftButton)
            || (!m_spacePanning && event->button() == Qt::MiddleButton));
    if (finishesPan) {
        m_panning = false;
        m_spacePanning = false;
        viewport()->unsetCursor();
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

} // namespace afs
