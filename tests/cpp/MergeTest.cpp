#include "editor/FlowsheetScene.h"
#include "graphics/FlotationGeometry.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"

#include <QApplication>
#include <QLineF>
#include <QGraphicsSimpleTextItem>
#include <cmath>

using namespace afs;

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    FlowsheetScene scene;
    auto* first = new FlotationUnitItem({"first", {0, 0}});
    auto* second = new FlotationUnitItem({"second", {500, 100}});
    scene.addItem(first);
    scene.addItem(second);
    auto* firstProduct = first->products().at(1);
    auto* secondProduct = second->products().at(0);

    auto* junction = scene.mergeProducts(firstProduct, secondProduct);
    if (!junction) return 1;
    if (firstProduct->isVisible() || secondProduct->isVisible()) return 2;
    if (std::abs(junction->mergeX() - 250.0) > 0.001) return 3;
    if (std::abs(junction->mergeY() - 300.0) > 0.001) return 4;
    junction->setManualMergeY(375.0);
    if (!junction->manualMergeY() || std::abs(junction->mergeY() - 375.0) > 0.001)
        return 16;
    junction->setProductName("合流精矿");
    junction->setTextSettings({16, true, QColor("#8844cc")});
    auto* mergeLabel = dynamic_cast<QGraphicsSimpleTextItem*>(junction->childItems().value(0));
    if (!mergeLabel || mergeLabel->text() != "合流精矿" || !mergeLabel->isVisible()
        || mergeLabel->font().pointSize() != 16 || !mergeLabel->font().bold()
        || mergeLabel->brush().color() != QColor("#8844cc")) return 13;

    second->setPos(second->pos() + QPointF(0, 40));
    if (std::abs(junction->mergeY() - 375.0) > 0.001) return 5;
    junction->setManualMergeY(std::nullopt);
    if (std::abs(junction->mergeY() - 340.0) > 0.001) return 17;
    auto* downstream = new FlotationUnitItem({"downstream", {900, 700}});
    scene.addItem(downstream);
    const QPointF outputBeforeConnect = junction->outputEndPosition();
    if (!scene.connectMerge(junction, downstream->inputLine())) return 6;
    if (junction->targetUnit() != downstream
        || downstream->inputLine()->sourceMerge() != junction
        || downstream->inputLine()->isVisible()) return 7;
    const QPointF inputTop = downstream->mapToScene(QPointF(0, -FlotationGeometry::InputHeight));
    if (QLineF(inputTop, outputBeforeConnect).length() > 0.001) return 8;
    if (!scene.disconnectMerge(junction) || !junction->isAvailable()
        || !downstream->inputLine()->isVisible()) return 9;
    if (!scene.splitMerge(junction)) return 10;
    if (!firstProduct->isVisible() || !secondProduct->isVisible()) return 11;
    if (firstProduct->isMerged() || secondProduct->isMerged()) return 12;

    auto* third = new FlotationUnitItem({"third", {900, 180}});
    scene.addItem(third);
    auto* threeWay = scene.mergeProducts(first->products().at(0), second->products().at(1));
    if (!threeWay || !scene.addProductToMerge(third->products().at(0), threeWay)
        || threeWay->products().size() != 3 || third->products().at(0)->isVisible()) return 14;
    if (!scene.splitMerge(threeWay) || !third->products().at(0)->isAvailable()) return 15;
    return 0;
}
