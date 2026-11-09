#include "graphics/CanvasInteraction.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/ProductLineItem.h"
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>

using namespace afs;
namespace {
class HostScene final : public QGraphicsScene, public CanvasGeometryHost, public CanvasConnectionHost {
public:
    int refreshed{0}, resized{0}, moved{0}, topology{0}, routes{0}, connected{0};
    mutable int hitTests{0};
    QPointF delta;
    InputLineItem* drop{nullptr};
    ProductLineItem* submitted{nullptr};
    bool aligned{false};
    void refreshConnections() override { ++refreshed; }
    void sourceWidthChanged(FlotationUnitItem*, double) override { ++resized; }
    void moveConnectedPeers(FlotationUnitItem*, const QPointF& value) override { ++moved; delta = value; }
    void notifyTopologyChanged() override { ++topology; }
    void notifyRouteChanged() override { ++routes; }
    InputLineItem* inputAtDropPosition(const QPointF&) const override {
        ++hitTests; return drop;
    }
    bool connectProduct(ProductLineItem* product, InputLineItem* input, bool align) override {
        submitted = product; aligned = align; if (input == drop) ++connected; return true;
    }
    bool connectMerge(MergeJunctionItem*, InputLineItem*, bool) override { return false; }
    MergeJunctionItem* mergeProducts(ProductLineItem*, ProductLineItem*, const QString&) override { return nullptr; }
    bool addProductToMerge(ProductLineItem*, MergeJunctionItem*) override { return false; }
};
}

bool runCanvasHostChecks() {
    HostScene scene;
    auto* source = new FlotationUnitItem({"port-source", {0, 0}});
    auto* target = new FlotationUnitItem({"port-target", {500, 500}});
    scene.addItem(source); scene.addItem(target);
    if (!source->adjustWidth(40) || scene.resized != 1
        || !source->adjustInputLength(10) || scene.refreshed != 1) return false;
    source->setPos(100, 50);
    if (scene.moved != 1 || scene.delta != QPointF(100, 50) || scene.refreshed != 2) return false;
    FlotationUnit splitData{"split", {0, 0}}; splitData.kind = UnitKind::BinarySplitter;
    auto* split = new FlotationUnitItem(splitData); scene.addItem(split);
    if (!split->setLeftSplitPercent(40) || scene.topology != 1) return false;
    auto* product = source->products().front(); scene.drop = target->inputLine();
    QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
    press.setButton(Qt::LeftButton); press.setButtons(Qt::LeftButton);
    scene.sendEvent(product, &press);
    QGraphicsSceneMouseEvent move(QEvent::GraphicsSceneMouseMove);
    move.setScenePos({-800, -800}); move.setPos(product->mapFromScene(move.scenePos()));
    move.setButtons(Qt::LeftButton); scene.sendEvent(product, &move);
    QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);
    release.setButton(Qt::LeftButton); release.setModifiers(Qt::ControlModifier);
    scene.sendEvent(product, &release);
    if (scene.hitTests != 1 || scene.connected != 1 || scene.submitted != product || !scene.aligned) return false;
    product->setManualRouteY(20);
    QKeyEvent reset(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
    scene.sendEvent(product, &reset);
    if (scene.routes != 1 || product->manualRouteY()) return false;
    // A transferred item must not retain callbacks into its previous scene.
    scene.removeItem(source);
    QGraphicsScene ordinary;
    ordinary.addItem(source);
    const int previousMoves = scene.moved, previousWidths = scene.resized;
    source->setPos(200, 100); source->adjustWidth(40);
    return scene.moved == previousMoves && scene.resized == previousWidths;
}
