#include "core/FlotationUnit.h"
#include "editor/FlowsheetScene.h"
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
    if (!closePoint(lower->pos(), QPointF(180, 222))) return 3;

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

    if (!scene.disconnectProduct(product)) return 11;
    if (product->isConnected() || !lower->inputLine()->isVisible()) return 12;
    const QPointF detachedPosition = lower->pos();
    upper->setPos(upper->pos() + QPointF(15, 10));
    if (!closePoint(lower->pos(), detachedPosition)) return 13;
    return 0;
}
