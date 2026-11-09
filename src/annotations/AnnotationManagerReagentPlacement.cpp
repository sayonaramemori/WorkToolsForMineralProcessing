#include "annotations/AnnotationManager.h"
#include "annotations/ReagentAnnotationItem.h"
#include "editor/FlowsheetScene.h"
#include <algorithm>
#include <limits>

namespace afs {
void AnnotationManager::revealPastedReagent(const QString& annotationId) {
    synchronizeReagents();
    auto* pasted = m_reagentItems.value(annotationId);
    if (!pasted) return;
    qreal bottom = -std::numeric_limits<qreal>::infinity();
    for (auto* item : m_reagentItems)
        if (item != pasted && item->isVisible() && item->record().ownerId == pasted->record().ownerId)
            bottom = std::max(bottom, item->sceneBoundingRect().bottom());
    const qreal delta = bottom + 8 - pasted->sceneBoundingRect().top();
    if (delta > 0) pasted->setPos(pasted->pos() + QPointF(0, delta));
    m_scene.clearSelection(); pasted->setSelected(true);
}
}
