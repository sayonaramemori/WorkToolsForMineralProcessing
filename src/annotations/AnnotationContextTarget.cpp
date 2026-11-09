#include "annotations/AnnotationContextTarget.h"
#include "annotations/AnnotationItem.h"
#include "annotations/ReagentAnnotationItem.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FeedJunctionItem.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"
#include <QHash>

namespace afs {
AnnotationContextTarget annotationContextTarget(const FlowsheetScene& scene, const QPointF& position) {
    QHash<QString, QGraphicsItem*> matches;
    QGraphicsItem* firstLine = nullptr;
    bool ambiguous = false;
    for (auto* hit : scene.items(position)) {
        for (auto* item = hit; item; item = item->parentItem()) {
            if (auto* reagent = dynamic_cast<ReagentAnnotationItem*>(item))
                return {reagent, {}, reagent->record().id, false};
            if (dynamic_cast<AnnotationItem*>(item)) return {item, {}, {}, false};
            QString id;
            if (auto* product = dynamic_cast<ProductLineItem*>(item)) id = product->streamId();
            else if (auto* input = dynamic_cast<InputLineItem*>(item)) id = input->unit()->unit().id + ":feed";
            else if (auto* merge = dynamic_cast<MergeJunctionItem*>(item)) id = merge->streamAtScenePosition(position);
            else if (auto* feed = dynamic_cast<FeedJunctionItem*>(item)) id = feed->streamAtScenePosition(position);
            else continue;
            if (!firstLine) firstLine = item;
            if (id.isEmpty()) ambiguous = true;
            else matches.insert(id, item);
            break;
        }
    }
    if (!ambiguous && matches.size() == 1) return {matches.cbegin().value(), matches.cbegin().key(), {}, true};
    return {firstLine, {}, {}, firstLine != nullptr};
}
}
