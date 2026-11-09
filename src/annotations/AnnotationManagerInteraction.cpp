#include "annotations/AnnotationManager.h"
#include "annotations/AnnotationItem.h"
#include "annotations/ReagentAnnotationItem.h"
#include "document/FlowsheetDocument.h"
#include "editor/FlowsheetScene.h"

#include <QGraphicsSceneContextMenuEvent>
#include <QKeyEvent>

namespace afs {

bool AnnotationManager::eventFilter(QObject* watched, QEvent* event) {
    if (watched != &m_scene) return QObject::eventFilter(watched, event);
    if (event->type() == QEvent::GraphicsSceneContextMenu)
        return showContextMenu(static_cast<QGraphicsSceneContextMenuEvent*>(event));
    if (event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() != Qt::Key_Delete) return false;
        bool removed = false;
        for (auto* selected : m_scene.selectedItems()) {
            if (auto* note = dynamic_cast<AnnotationItem*>(selected);
                note && note->record().kind == AnnotationKind::UserNote) {
                const QString id = note->record().id;
                m_document.removeAnnotationRecord(id);
                m_noteItems.remove(id);
                m_scene.removeItem(note); delete note; removed = true;
                continue;
            }
            if (auto* result = dynamic_cast<AnnotationItem*>(selected);
                result && result->record().kind == AnnotationKind::StreamResult) {
                m_temporarilyHiddenResults.insert(result->record().id);
                result->setSelected(false);
                result->setVisible(false);
                removed = true;
                continue;
            }
            auto* reagent = dynamic_cast<ReagentAnnotationItem*>(selected);
            if (!reagent) continue;
            const QString id = reagent->record().id;
            m_document.removeAnnotationRecord(id);
            m_reagentItems.remove(id);
            m_scene.removeItem(reagent); delete reagent; removed = true;
        }
        return removed;
    }
    return false;
}

bool AnnotationManager::nudgeSelectedReagents(qreal verticalDelta) {
    if (qFuzzyIsNull(verticalDelta)) return false;
    bool moved = false;
    for (auto* selected : m_scene.selectedItems()) {
        auto* reagent = dynamic_cast<ReagentAnnotationItem*>(selected);
        if (!reagent) continue;
        reagent->setPos(reagent->pos() + QPointF(0.0, verticalDelta));
        moved = true;
    }
    return moved;
}

} // namespace afs
