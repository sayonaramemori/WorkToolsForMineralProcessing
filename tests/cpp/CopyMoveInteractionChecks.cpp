#include "app/MainWindow.h"
#include "editor/CanvasView.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FeedJunctionItem.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/ProductLineItem.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QAbstractButton>
#include <QMessageBox>
#include <QMouseEvent>
#include <QTest>
#include <QTimer>
#include <stdexcept>

using namespace afs;

namespace {
void require(bool condition, const QString& message) {
    if (!condition) throw std::runtime_error(message.toStdString());
}

bool translatedPath(const QPainterPath& before, const QPainterPath& after, QPointF delta) {
    if (before.elementCount() != after.elementCount()) return false;
    for (int index = 0; index < before.elementCount(); ++index) {
        const auto a = before.elementAt(index), b = after.elementAt(index);
        if (a.type != b.type || QLineF(QPointF(a.x, a.y) + delta, {b.x, b.y}).length() > 0.001)
            return false;
    }
    return true;
}

QHash<ProductLineItem*, QPainterPath> productPaths(const QHash<QString, FlotationUnitItem*>& units) {
    QHash<ProductLineItem*, QPainterPath> result;
    for (auto* unit : units)
        for (auto* product : unit->products()) result.insert(product, product->mapToScene(product->path()));
    return result;
}

void expectProductTranslation(const QHash<ProductLineItem*, QPainterPath>& paths, QPointF delta) {
    for (auto it = paths.cbegin(); it != paths.cend(); ++it)
        require(translatedPath(it.value(), it.key()->mapToScene(it.key()->path()), delta),
                "Dragging changed a direct or terminal product path");
}
}

void runCopyMoveInteractionChecks() {
    MainWindow window;
    window.resize(1600, 1000); window.show(); window.activateWindow();
    QDir root(QCoreApplication::applicationDirPath());
    root.cdUp(); root.cdUp();
    QTimer::singleShot(0, [] {
        if (auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
            if (auto* button = dialog->button(QMessageBox::Discard)) button->click();
    });
    require(window.openProject(root.filePath("tests/copy-move.afs.json")), "Copy/move fixture did not load");
    auto* canvas = dynamic_cast<CanvasView*>(window.centralWidget());
    auto* scene = qobject_cast<FlowsheetScene*>(canvas->scene());
    QHash<QString, FlotationUnitItem*> originals;
    FeedJunctionItem* originalFeed = nullptr;
    for (auto* item : scene->items()) {
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item)) {
            originals.insert(unit->unit().id, unit); unit->setSelected(true);
        }
        if (auto* feed = dynamic_cast<FeedJunctionItem*>(item)) originalFeed = feed;
    }
    require(originals.size() == 3 && originalFeed, "Unexpected copy/move fixture topology");
    const auto originalPath = originalFeed->path();
    window.activateWindow(); canvas->setFocus();
    QApplication::processEvents();
    QTest::keyClick(canvas->viewport(), Qt::Key_C, Qt::ControlModifier);
    QApplication::processEvents();
    QTest::keyClick(canvas->viewport(), Qt::Key_V, Qt::ControlModifier);
    QApplication::processEvents();
    QHash<QString, FlotationUnitItem*> clones;
    FeedJunctionItem* feed = nullptr;
    for (auto* item : scene->selectedItems())
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item)) clones.insert(unit->unit().id, unit);
    for (auto* item : scene->items())
        if (auto* candidate = dynamic_cast<FeedJunctionItem*>(item); candidate && candidate != originalFeed)
            feed = candidate;
    require(clones.size() == 3 && feed, "Canvas paste did not clone the complete group");
    require(translatedPath(originalPath, feed->path(), {40, 40}), "Paste changed fixture recycle route geometry");
    for (auto* clone : clones) {
        FlotationUnitItem* original = nullptr;
        for (auto* candidate : originals)
            if (QLineF(candidate->pos() + QPointF(40, 40), clone->pos()).length() < 0.001)
                original = candidate;
        require(original, "Copied unit has an unexpected position");
        const auto sources = original->products(), products = clone->products();
        require(sources.size() == products.size(), "Copied product count changed");
        for (int index = 0; index < sources.size(); ++index) {
            require(sources[index]->terminalLengthOverride() == products[index]->terminalLengthOverride(),
                    "Paste lost a product endpoint length override");
            require(translatedPath(sources[index]->mapToScene(sources[index]->path()),
                                   products[index]->mapToScene(products[index]->path()), {40, 40}),
                    "Paste changed a direct or terminal product path");
        }
    }
    qInfo() << "Fixture paste preserves product lengths and complete recycle route geometry";
    auto* grabbed = feed->recycleProducts().front()->sourceUnit();
    QHash<QString, QPointF> positions;
    for (auto it = clones.cbegin(); it != clones.cend(); ++it) positions.insert(it.key(), it.value()->pos());
    const auto before = feed->path();
    const auto beforeProducts = productPaths(clones);
    canvas->centerOn(grabbed);
    QApplication::processEvents();
    const QPoint press = canvas->mapFromScene(grabbed->mapToScene({70, 0}));
    QTest::mousePress(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, press);
    require(scene->mouseGrabberItem() == grabbed, "Mouse did not grab the copied unit");
    for (const QPoint step : {QPoint(35, 20), QPoint(70, -30), QPoint(110, 45)}) {
        const QPointF point = press + step;
        QMouseEvent move(QEvent::MouseMove, point, canvas->viewport()->mapToGlobal(point.toPoint()),
                         Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(canvas->viewport(), &move);
        const auto delta = grabbed->pos() - positions.value(grabbed->unit().id);
        require(!delta.isNull(), "Mouse did not move copied units");
        for (auto it = clones.cbegin(); it != clones.cend(); ++it)
            require(QLineF(it.value()->pos(), positions.value(it.key()) + delta).length() < 0.001,
                    "Copied units did not move together");
        require(translatedPath(before, feed->path(), delta), "Mouse drag changed copied recycle route geometry");
        expectProductTranslation(beforeProducts, delta);
        require(translatedPath(originalPath, originalFeed->path(), {}), "Dragging clone changed original routes");
    }
    QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, press + QPoint(110, 45));
    QApplication::processEvents();
    require(translatedPath(before, feed->path(), grabbed->pos() - positions.value(grabbed->unit().id)),
            "Mouse release changed copied recycle route geometry");

    // Exercise ordinary event-loop turns, partial selection (the unselected
    // connected unit must follow once), and dragging a single selected unit.
    for (int selectedCount : {3, 2, 1}) {
        scene->clearSelection(); grabbed->setSelected(true);
        int count = 1;
        for (auto* unit : clones)
            if (unit != grabbed && count++ < selectedCount) unit->setSelected(true);
        const auto route = feed->path();
        const auto products = productPaths(clones);
        for (auto* unit : clones) positions[unit->unit().id] = unit->pos();
        canvas->centerOn(grabbed); QApplication::processEvents();
        const QPoint start = canvas->mapFromScene(grabbed->mapToScene({70, 0}));
        QTest::mousePress(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, start);
        require(scene->mouseGrabberItem() == grabbed, "Second gesture did not grab the copied unit");
        for (const QPoint step : {QPoint(-20, 15), QPoint(-55, -25)}) {
            QTest::mouseMove(canvas->viewport(), start + step);
            QApplication::processEvents();
            const auto delta = grabbed->pos() - positions.value(grabbed->unit().id);
            require(!delta.isNull() && translatedPath(route, feed->path(), delta),
                    "Ordinary or partially selected drag changed recycle route geometry");
            expectProductTranslation(products, delta);
            for (auto* unit : clones)
                require(QLineF(unit->pos(), positions.value(unit->unit().id) + delta).length() < 0.001,
                        "Unselected connected peer did not follow the drag exactly once");
        }
        QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, start + QPoint(-55, -25));
        QApplication::processEvents();
        require(translatedPath(originalPath, originalFeed->path(), {}), "Dragging clone changed original routes");
    }
    qInfo() << "Batched mouse events, repeated gestures, partial and single selection preserve route geometry";

    // Two disconnected groups may be selected together. Each component's
    // absolute route coordinates must translate once in the same event.
    for (auto* unit : originals) unit->setSelected(true);
    for (auto* unit : clones) unit->setSelected(true);
    const auto cloneRoute = feed->path(), sourceRoute = originalFeed->path();
    const auto sourceProducts = productPaths(originals), cloneProducts = productPaths(clones);
    const auto startPosition = grabbed->pos();
    canvas->centerOn(grabbed); QApplication::processEvents();
    const auto start = canvas->mapFromScene(grabbed->mapToScene({70, 0}));
    QTest::mousePress(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, start);
    require(scene->mouseGrabberItem() == grabbed, "Multi-component drag did not grab the copied unit");
    QTest::mouseMove(canvas->viewport(), start + QPoint(45, 30));
    QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, start + QPoint(45, 30));
    QApplication::processEvents();
    const auto delta = grabbed->pos() - startPosition;
    require(!delta.isNull() && translatedPath(cloneRoute, feed->path(), delta)
            && translatedPath(sourceRoute, originalFeed->path(), delta),
            "Multi-component drag did not translate each component's recycle route");
    expectProductTranslation(sourceProducts, delta);
    expectProductTranslation(cloneProducts, delta);
}
