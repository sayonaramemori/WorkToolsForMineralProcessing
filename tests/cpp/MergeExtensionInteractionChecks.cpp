#include "editor/FlowsheetScene.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"
#include <QApplication>
#include <QDebug>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QMouseEvent>
#include <QTest>
#include <stdexcept>

using namespace afs;
namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
QPointF productPoint(ProductLineItem* product) {
    return product->sourceAnchorScenePosition()
        + (product->unconnectedEndScenePosition() - product->sourceAnchorScenePosition()) * 0.6;
}
void drag(QGraphicsView& view, QGraphicsItem* expected, QPointF start, QPointF finish) {
    view.setFocus(); QApplication::processEvents();
    const QPoint from = view.mapFromScene(start), to = view.mapFromScene(finish);
    QTest::mousePress(view.viewport(), Qt::LeftButton, Qt::NoModifier, from);
    require(view.scene()->mouseGrabberItem() == expected, "Merge extension drag did not grab its source");
    QMouseEvent move(QEvent::MouseMove, to, view.viewport()->mapToGlobal(to),
                     Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(view.viewport(), &move);
    QTest::mouseRelease(view.viewport(), Qt::LeftButton, Qt::NoModifier, to);
    QApplication::processEvents();
}
}
int runMergeExtensionInteractionChecks() {
    try {
        FlowsheetScene scene;
        auto* a = new FlotationUnitItem({"extend-a", {-400, 0}});
        auto* b = new FlotationUnitItem({"extend-b", {100, 100}});
        auto* c = new FlotationUnitItem({"extend-c", {700, 150}});
        auto* d = new FlotationUnitItem({"extend-d", {-350, 220}});
        auto* outside = new FlotationUnitItem({"outside", {650, 600}});
        auto* downstream = new FlotationUnitItem({"downstream", {1150, 750}});
        auto* mergeTarget = new FlotationUnitItem({"merge-target", {300, 850}});
        for (auto* unit : {a, b, c, d, outside, downstream, mergeTarget}) scene.addItem(unit);
        auto* merge = scene.mergeProducts(a->products().front(), b->products().front(), "stable-merge");
        require(merge, "Merge extension fixture failed");
        merge->setManualMergeY(430);
        merge->setProductName("合流精矿");
        const QPointF cPosition = c->pos();
        scene.setSceneRect(scene.itemsBoundingRect().adjusted(-150, -150, 150, 150));
        QGraphicsView view(&scene); view.resize(1100, 850); view.show(); view.activateWindow();
        view.fitInView(scene.sceneRect(), Qt::KeepAspectRatio); QApplication::processEvents();
        drag(view, merge, merge->outputEndPosition() + QPointF(0, 2), productPoint(c->products().front()));
        require(merge->products().size() == 3 && c->products().front()->mergeJunction() == merge
            && !c->products().front()->isVisible() && c->pos() == cPosition,
            "Dragging merged output onto a free product did not extend the existing merge");
        require(merge->id() == "stable-merge" && merge->manualMergeY() == 430,
            "Extending merge changed its stream identity or manual route");
        auto* label = dynamic_cast<QGraphicsSimpleTextItem*>(merge->childItems().value(0));
        require(label && label->isVisible() && label->text() == "合流精矿", "Extending merge lost its product label");
        drag(view, d->products().front(), productPoint(d->products().front()),
             merge->outputEndPosition() + QPointF(0, 2));
        require(merge->products().size() == 4 && d->products().front()->mergeJunction() == merge,
            "Reverse product-to-merge drag stopped working");
        const QPainterPath originalRoute = merge->path();
        drag(view, merge, merge->outputEndPosition() + QPointF(0, 2), {-600, -150});
        require(merge->products().size() == 4 && merge->path() == originalRoute && label->isVisible(),
            "Empty drop did not restore the merge path and name");
        drag(view, merge, merge->outputEndPosition() + QPointF(0, 2), productPoint(a->products().back()));
        require(merge->products().size() == 4 && a->products().back()->isAvailable(),
            "Merge accepted another product from an existing source unit");
        auto* occupied = outside->products().back();
        require(scene.connectProductDirect(occupied, downstream->inputLine()), "Occupied target fixture failed");
        drag(view, merge, merge->outputEndPosition() + QPointF(0, 2), occupied->sourceAnchorScenePosition() + QPointF(0, 12));
        require(merge->products().size() == 4 && occupied->targetUnit() == downstream
            && downstream->inputLine()->sourceProduct() == occupied && merge->isAvailable(),
            "Rejected merge extension broke an occupied product connection");
        drag(view, merge, merge->outputEndPosition() + QPointF(0, 2), mergeTarget->inputAnchorScenePosition());
        require(merge->targetUnit() == mergeTarget && mergeTarget->inputLine()->sourceMerge() == merge,
            "Merge-to-input dragging stopped working after extension");
        require(!scene.addProductToMerge(outside->products().front(), merge)
            && merge->targetUnit() == mergeTarget, "Connected merge was modified by extension");
        qInfo() << "Merged-output extension interaction checks passed";
        return 0;
    } catch (const std::exception& error) {
        qCritical() << error.what(); return 1;
    }
}
