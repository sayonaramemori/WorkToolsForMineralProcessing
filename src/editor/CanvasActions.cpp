#include "editor/CanvasActions.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/FeedJunctionItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"

namespace afs {

ResizeSelectionResult CanvasActions::resizeSelection(FlowsheetScene& scene, double delta) {
    ResizeSelectionResult result;
    for (auto* selected : scene.selectedItems()) {
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(selected)) {
            if (unit->adjustWidth(delta)) ++result.resizedUnits;
            result.lastUnitWidth = unit->unit().width;
        } else if (auto* product = dynamic_cast<ProductLineItem*>(selected)) {
            const bool resized = product->isConnected()
                ? scene.adjustConnectionLength(product, delta)
                : product->adjustTerminalLength(delta);
            if (resized) {
                ++result.resizedConnections;
                if (!product->isConnected()) scene.notifyRouteChanged();
            }
        }
    }
    return result;
}

int CanvasActions::disconnectSelection(FlowsheetScene& scene) {
    int disconnected = 0;
    const auto selected = scene.selectedItems();
    QList<FeedJunctionItem*> feedJunctions;
    QList<MergeJunctionItem*> mergeJunctions;
    QList<ProductLineItem*> products;
    for (auto* item : selected) {
        if (auto* junction = dynamic_cast<FeedJunctionItem*>(item)) feedJunctions.append(junction);
        else if (auto* junction = dynamic_cast<MergeJunctionItem*>(item)) mergeJunctions.append(junction);
        else if (auto* product = dynamic_cast<ProductLineItem*>(item)) products.append(product);
    }
    // Feed junctions own references to merge outputs, so always detach them
    // before splitting any selected product merge.
    for (auto* junction : feedJunctions)
        if (scene.disconnectRecycle(junction)) ++disconnected;
    for (auto* junction : mergeJunctions)
        if (junction->isConnected() ? scene.disconnectMerge(junction)
                                    : scene.splitMerge(junction)) ++disconnected;
    for (auto* product : products)
        if (product->isConnected() && scene.disconnectProduct(product)) ++disconnected;
    return disconnected;
}

} // namespace afs
