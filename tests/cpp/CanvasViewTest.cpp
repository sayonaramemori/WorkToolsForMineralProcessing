#include "editor/CanvasView.h"

#include <QApplication>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QWheelEvent>
#include <cmath>

using namespace afs;

namespace {

void sendWheel(CanvasView& view, int delta) {
    const QPointF local(view.viewport()->rect().center());
    QWheelEvent event(local, view.viewport()->mapToGlobal(local.toPoint()),
                      {}, QPoint(0, delta), Qt::NoButton, Qt::NoModifier,
                      Qt::NoScrollPhase, false);
    QApplication::sendEvent(view.viewport(), &event);
}

void sendMouse(CanvasView& view, QEvent::Type type, const QPointF& local,
               Qt::MouseButton button, Qt::MouseButtons buttons) {
    QMouseEvent event(type, local, view.viewport()->mapToGlobal(local.toPoint()),
                      button, buttons, Qt::NoModifier);
    QApplication::sendEvent(view.viewport(), &event);
}

void sendKey(CanvasView& view, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent event(QEvent::KeyPress, key, modifiers);
    QApplication::sendEvent(view.viewport(), &event);
}

void releaseKey(CanvasView& view, int key,
                Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent event(QEvent::KeyRelease, key, modifiers);
    QApplication::sendEvent(view.viewport(), &event);
}

} // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QGraphicsScene scene(-2000, -2000, 4000, 4000);
    CanvasView view(&scene);
    view.resize(500, 400);
    view.show();
    app.processEvents();

    sendWheel(view, 120);
    if (view.transform().m11() <= 1.0) return 1;

    for (int i = 0; i < 100; ++i) sendWheel(view, -120);
    if (std::abs(view.transform().m11() - 0.2) > 0.0001) return 2;
    for (int i = 0; i < 200; ++i) sendWheel(view, 120);
    if (std::abs(view.transform().m11() - 4.0) > 0.0001) return 3;

    view.centerOn(0, 0);
    const int before = view.horizontalScrollBar()->value();
    const QPointF start(250, 200);
    sendMouse(view, QEvent::MouseButtonPress, start,
              Qt::MiddleButton, Qt::MiddleButton);
    sendMouse(view, QEvent::MouseMove, start + QPointF(40, 0),
              Qt::NoButton, Qt::MiddleButton);
    sendMouse(view, QEvent::MouseButtonRelease, start + QPointF(40, 0),
              Qt::MiddleButton, Qt::NoButton);
    if (view.horizontalScrollBar()->value() != before - 40) return 4;

    // Space turns the primary-button drag into a hand tool, for trackpads
    // which often do not expose a convenient middle-button gesture.
    const int spaceDragBefore = view.horizontalScrollBar()->value();
    sendKey(view, Qt::Key_Space);
    sendMouse(view, QEvent::MouseButtonPress, start,
              Qt::LeftButton, Qt::LeftButton);
    sendMouse(view, QEvent::MouseMove, start + QPointF(35, 0),
              Qt::NoButton, Qt::LeftButton);
    sendMouse(view, QEvent::MouseButtonRelease, start + QPointF(35, 0),
              Qt::LeftButton, Qt::NoButton);
    releaseKey(view, Qt::Key_Space);
    if (view.horizontalScrollBar()->value() != spaceDragBefore - 35) return 11;

    const QRectF originalBounds = scene.sceneRect();
    view.centerOn(originalBounds.left() + 20.0, 0.0);
    sendMouse(view, QEvent::MouseButtonPress, start,
              Qt::MiddleButton, Qt::MiddleButton);
    sendMouse(view, QEvent::MouseMove, start + QPointF(180, 0),
              Qt::NoButton, Qt::MiddleButton);
    sendMouse(view, QEvent::MouseButtonRelease, start + QPointF(180, 0),
              Qt::MiddleButton, Qt::NoButton);
    if (scene.sceneRect().left() >= originalBounds.left()) return 8;

    auto* nudgeItem = scene.addRect(-10, -10, 20, 20);
    nudgeItem->setFlag(QGraphicsItem::ItemIsSelectable);
    nudgeItem->setSelected(true);
    view.setVerticalNudgeHandler([nudgeItem](qreal delta) {
        if (!nudgeItem->isSelected()) return false;
        nudgeItem->setPos(nudgeItem->pos() + QPointF(0, delta));
        return true;
    });
    const QPointF beforeNudge = nudgeItem->pos();
    const int verticalBefore = view.verticalScrollBar()->value();
    sendKey(view, Qt::Key_Down);
    if (nudgeItem->pos() != beforeNudge + QPointF(0, 2)
        || view.verticalScrollBar()->value() != verticalBefore) return 5;
    sendKey(view, Qt::Key_Up, Qt::ShiftModifier);
    if (nudgeItem->pos() != beforeNudge + QPointF(0, -8)) return 6;

    // Space + arrows pan the viewport and deliberately take precedence over
    // the selected-item nudge handler.
    const int keyboardPanBefore = view.horizontalScrollBar()->value();
    const QPointF nudgeBeforeKeyboardPan = nudgeItem->pos();
    sendKey(view, Qt::Key_Space);
    sendKey(view, Qt::Key_Right);
    if (view.horizontalScrollBar()->value() != keyboardPanBefore + 40
        || nudgeItem->pos() != nudgeBeforeKeyboardPan) return 9;
    releaseKey(view, Qt::Key_Space);
    const int verticalAfterSpaceRelease = view.verticalScrollBar()->value();
    sendKey(view, Qt::Key_Down);
    if (nudgeItem->pos() != nudgeBeforeKeyboardPan + QPointF(0, 2)
        || view.verticalScrollBar()->value() != verticalAfterSpaceRelease) return 10;
    bool deleteHandled = false;
    view.setDeleteHandler([&deleteHandled] { deleteHandled = true; return true; });
    sendKey(view, Qt::Key_Delete);
    if (!deleteHandled) return 7;

    auto* distantItem = scene.addRect(1800, 1400, 80, 60);
    sendKey(view, Qt::Key_Home);
    const QRectF visibleAfterHome = view.mapToScene(view.viewport()->rect()).boundingRect();
    if (!visibleAfterHome.contains(nudgeItem->sceneBoundingRect())
        || !visibleAfterHome.contains(distantItem->sceneBoundingRect())) return 12;
    return 0;
}
