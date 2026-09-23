#include "core/FlotationUnit.h"
#include "editor/FlowsheetScene.h"
#include "graphics/FlotationGeometry.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/ProductLineItem.h"

#include <QApplication>
#include <cmath>

using namespace afs;

namespace {
bool closePoint(const QPointF& a, const QPointF& b) {
    return std::abs(a.x() - b.x()) < 0.001 && std::abs(a.y() - b.y()) < 0.001;
}
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    FlowsheetScene scene;
    auto* upper = new FlotationUnitItem({"upper", {0, 0}});
    auto* lower = new FlotationUnitItem({"lower", {500, 400}});
    scene.addItem(upper);
    scene.addItem(lower);
    auto* product = upper->products().at(1);

    if (!scene.connectProduct(product, lower->inputLine())) return 1;
    if (!product->isConnected() || lower->inputLine()->isVisible()) return 2;
    if (!closePoint(lower->pos(), QPointF(500, 400))) return 3;

    const QPointF lowerBeforeMove = lower->pos();
    upper->setPos(upper->pos() + QPointF(25, 30));
    if (!closePoint(lower->pos(), lowerBeforeMove + QPointF(25, 30))) return 4;

    const QPointF upperBeforeResize = upper->pos();
    const QPointF lowerBeforeResize = lower->pos();
    if (!upper->adjustWidth(40)) return 5;
    if (!closePoint(upper->pos(), upperBeforeResize)) return 6;
    if (!closePoint(lower->pos(), lowerBeforeResize + QPointF(20, 0))) return 7;

    const QPointF lowerBeforeLengthChange = lower->pos();
    if (!scene.adjustConnectionLength(product, 40)) return 8;
    if (!closePoint(upper->pos(), upperBeforeResize)) return 9;
    if (!closePoint(lower->pos(), lowerBeforeLengthChange + QPointF(0, 40))) return 10;

    product->setManualRouteY(260.0);
    if (!product->manualRouteY() || std::abs(*product->manualRouteY() - 260.0) > 0.001
        || product->path().elementCount() < 2) return 14;
    for (int index = 1; index < product->path().elementCount(); ++index) {
        const auto previous = product->path().elementAt(index - 1);
        const auto current = product->path().elementAt(index);
        if (std::abs(previous.x - current.x) > 0.001
            && std::abs(previous.y - current.y) > 0.001) return 15;
    }
    product->setManualRouteY(std::nullopt);
    if (product->manualRouteY()) return 16;

    if (!scene.disconnectProduct(product)) return 11;
    if (product->isConnected() || !lower->inputLine()->isVisible()) return 12;
    const QPointF detachedPosition = lower->pos();
    upper->setPos(upper->pos() + QPointF(15, 10));
    if (!closePoint(lower->pos(), detachedPosition)) return 13;

    FlowsheetScene threeProductScene;
    FlotationUnit threeProductData{"three", {0, 0}};
    threeProductData.kind = UnitKind::ThreeProductFlotation;
    auto* threeProduct = new FlotationUnitItem(threeProductData);
    auto* downstream = new FlotationUnitItem({"downstream", {500, 400}});
    threeProductScene.addItem(threeProduct);
    threeProductScene.addItem(downstream);
    if (threeProduct->products().size() != 3
        || threeProduct->products()[1]->side() != ProductSide::Middle
        || !threeProductScene.connectProduct(
            threeProduct->products()[1], downstream->inputLine())) return 17;
    if (!threeProduct->products()[1]->isConnected()
        || downstream->inputLine()->sourceProduct() != threeProduct->products()[1]
        || !closePoint(downstream->pos(), QPointF(500, 400))) return 18;
    auto* middleProduct = threeProduct->products()[1];
    middleProduct->setManualRouteY(240.0);
    threeProduct->setPos(threeProduct->pos() + QPointF(0, -120));
    if (!middleProduct->manualRouteY()
        || std::abs(*middleProduct->manualRouteY() - 240.0) > 0.001
        || !closePoint(downstream->pos(), QPointF(500, 280))) return 23;

    FlowsheetScene alignScene;
    auto* alignSource = new FlotationUnitItem({"align-source", {0, 0}});
    auto* alignTarget = new FlotationUnitItem({"align-target", {500, 400}});
    alignScene.addItem(alignSource);
    alignScene.addItem(alignTarget);
    if (!alignScene.connectProduct(alignSource->products().at(1), alignTarget->inputLine(), true)
        || !closePoint(alignTarget->pos(), QPointF(180, 222))) return 22;

    const double originalTerminalLength = product->terminalLength();
    if (!product->adjustTerminalLength(4000.0)
        || std::abs(product->terminalLength() - (originalTerminalLength + 4000.0)) > 0.001
        || !product->adjustTerminalLength(4000.0)
        || std::abs(product->terminalLength() - 5000.0) > 0.001
        || !product->adjustTerminalLength(-10000.0)
        || std::abs(product->terminalLength() - 30.0) > 0.001) return 19;
    if (!threeProductScene.removeUnit(threeProduct)
        || !downstream->inputLine()->isVisible()
        || downstream->inputLine()->sourceProduct()) return 20;

    // Every downstream unit follows the same connection contract: ordinary
    // linking preserves its position; Ctrl alignment moves it to the product
    // endpoint, including a storage pool.
    FlowsheetScene poolScene;
    auto* poolSource = new FlotationUnitItem({"pool-source", {0, 0}});
    FlotationUnit poolData{"pool", {740, 460}};
    poolData.kind = UnitKind::TailingsPool;
    auto* pool = new FlotationUnitItem(poolData);
    poolScene.addItem(poolSource);
    poolScene.addItem(pool);
    const QPointF poolPosition = pool->pos();
    if (!poolScene.connectProduct(poolSource->products().at(1), pool->inputLine())
        || pool->pos() != poolPosition
        || !poolSource->products().at(1)->isConnected()
        || poolSource->products().at(1)->brush().style() != Qt::NoBrush
        || poolSource->products().at(1)->path().elementCount() < 4) return 21;
    if (!poolScene.disconnectProduct(poolSource->products().at(1))) return 24;
    const QPointF alignedPoolInput = poolSource->products().at(1)->unconnectedEndScenePosition();
    if (!poolScene.connectProduct(poolSource->products().at(1), pool->inputLine(), true)
        || !closePoint(pool->mapToScene(QPointF(0, -FlotationGeometry::InputHeight)),
                       alignedPoolInput)) return 25;
    return 0;
}
