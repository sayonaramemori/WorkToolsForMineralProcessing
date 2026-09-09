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
    bool deleteHandled = false;
    view.setDeleteHandler([&deleteHandled] { deleteHandled = true; return true; });
    sendKey(view, Qt::Key_Delete);
    if (!deleteHandled) return 7;
    return 0;
}
