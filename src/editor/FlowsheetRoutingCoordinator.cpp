#include "editor/FlowsheetRoutingCoordinator.h"

#include "graphics/items/FeedJunctionItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"

#include <QGraphicsScene>

namespace afs {

void FlowsheetRoutingCoordinator::refreshPaths(QGraphicsScene& scene) {
    for (auto* item : scene.items()) {
        if (auto* product = dynamic_cast<ProductLineItem*>(item)) product->updatePath();
        if (auto* input = dynamic_cast<InputLineItem*>(item)) input->refreshAppearance();
        if (auto* junction = dynamic_cast<MergeJunctionItem*>(item)) junction->updatePath();
    }
    // Feed junction paths depend on all preceding product/merge endpoints.
    for (auto* item : scene.items())
        if (auto* junction = dynamic_cast<FeedJunctionItem*>(item)) junction->updatePath();
}

void FlowsheetRoutingCoordinator::refreshAppearance(QGraphicsScene& scene) {
    for (auto* item : scene.items()) {
        if (auto* product = dynamic_cast<ProductLineItem*>(item)) product->refreshAppearance();
        if (auto* input = dynamic_cast<InputLineItem*>(item)) input->refreshAppearance();
        if (auto* junction = dynamic_cast<MergeJunctionItem*>(item)) junction->refreshAppearance();
        if (auto* junction = dynamic_cast<FeedJunctionItem*>(item)) junction->refreshAppearance();
        item->update();
    }
    scene.update();
}

} // namespace afs
