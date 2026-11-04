#include "annotations/AnnotationManager.h"
#include "annotations/AnnotationItem.h"
#include "annotations/ReagentAnnotationItem.h"
#include "document/FlowsheetDocument.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/FeedJunctionItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"

#include <QGraphicsSceneContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>

namespace afs {

QString AnnotationManager::selectedLineStreamId(QGraphicsItem** owner) const {
    for (auto* item : m_scene.selectedItems()) {
        if (auto* product = dynamic_cast<ProductLineItem*>(item)) {
            if (owner) *owner = product; return product->streamId();
        }
        if (auto* merge = dynamic_cast<MergeJunctionItem*>(item)) {
            if (owner) *owner = merge; return merge->outputStreamId();
        }
        if (auto* input = dynamic_cast<InputLineItem*>(item)) {
            if (owner) *owner = input; return input->unit()->unit().id + ":feed";
        }
        if (auto* feed = dynamic_cast<FeedJunctionItem*>(item)) {
            if (owner) *owner = feed;
            return feed->selectedSourceStreamId().isEmpty()
                ? feed->outputStreamId() : feed->selectedSourceStreamId();
        }
    }
    return {};
}

bool AnnotationManager::eventFilter(QObject* watched, QEvent* event) {
    if (watched != &m_scene) return QObject::eventFilter(watched, event);
    if (event->type() == QEvent::GraphicsSceneContextMenu) {
        auto* context = static_cast<QGraphicsSceneContextMenuEvent*>(event);
        QGraphicsItem* owner = nullptr;
        QString streamId = selectedLineStreamId(&owner);
        // QGraphicsItem's default right-button handling can clear the previous
        // selection before the scene receives its context-menu event. Resolve
        // the line under the pointer and restore its selection in that case.
        if (streamId.isEmpty()) {
            for (auto* hit : m_scene.items(context->scenePos())) {
                QGraphicsItem* candidate = hit;
                while (candidate) {
                    if (auto* product = dynamic_cast<ProductLineItem*>(candidate)) {
                        streamId = product->streamId(); owner = product; break;
                    }
                    if (auto* merge = dynamic_cast<MergeJunctionItem*>(candidate)) {
                        streamId = merge->outputStreamId(); owner = merge; break;
                    }
                    if (auto* input = dynamic_cast<InputLineItem*>(candidate)) {
                        streamId = input->unit()->unit().id + ":feed"; owner = input; break;
                    }
                    if (auto* feed = dynamic_cast<FeedJunctionItem*>(candidate)) {
                        streamId = feed->outputStreamId(); owner = feed; break;
                    }
                    candidate = candidate->parentItem();
                }
                if (!streamId.isEmpty()) break;
            }
            if (owner) {
                m_scene.clearSelection();
                owner->setSelected(true);
            }
        }
        QMenu menu;
        auto* addReagent = streamId.isEmpty() ? nullptr : menu.addAction(tr("添加药剂标注"));
        auto* addNote = menu.addAction(tr("添加自定义文字"));
        QAction* resetRoute = nullptr;
        if (auto* product = dynamic_cast<ProductLineItem*>(owner); product && product->manualRouteY())
            resetRoute = menu.addAction(tr("恢复自动布线"));
        else if (auto* merge = dynamic_cast<MergeJunctionItem*>(owner); merge && merge->manualMergeY())
            resetRoute = menu.addAction(tr("恢复自动布线"));
        else if (auto* feed = dynamic_cast<FeedJunctionItem*>(owner);
                 feed && (!feed->manualRouteXs().isEmpty() || !feed->manualRouteYs().isEmpty()))
            resetRoute = menu.addAction(tr("恢复全部支路自动布线"));
        auto* selectedAction = menu.exec(context->screenPos());
        if (!selectedAction) return true;
        if (selectedAction == resetRoute) {
            if (auto* product = dynamic_cast<ProductLineItem*>(owner))
                product->setManualRouteY(std::nullopt);
            else if (auto* merge = dynamic_cast<MergeJunctionItem*>(owner))
                merge->setManualMergeY(std::nullopt);
            else if (auto* feed = dynamic_cast<FeedJunctionItem*>(owner)) {
                feed->resetManualRoutes();
            }
            m_scene.notifyRouteChanged();
        } else if (selectedAction == addReagent) {
            int next = 1;
            while (m_document.annotationRecords().contains(QString("reagent-%1").arg(next))) ++next;
            AnnotationRecord record{QString("reagent-%1").arg(next), AnnotationKind::Reagent,
                AnnotationStyle::PlainText, AnnotationOwnerKind::Stream, streamId, {}, false, true};
            if (!ReagentAnnotationItem::editRecord(nullptr, record, tr("添加药剂标注"))) return true;
            m_document.setAnnotationRecord(record);
            synchronizeReagents();
            if (auto* item = m_reagentItems.value(record.id)) {
                m_scene.clearSelection(); item->setSelected(true);
            }
        } else if (selectedAction == addNote) {
            int next = 1;
            while (m_document.annotationRecords().contains(QString("note-%1").arg(next))) ++next;
            AnnotationRecord record{QString("note-%1").arg(next), AnnotationKind::UserNote,
                AnnotationStyle::Note, AnnotationOwnerKind::Free, QStringLiteral("canvas"),
                context->scenePos(), true, true};
            if (!AnnotationItem::editUserNote(nullptr, record,
                    m_document.annotationTextSettings(), tr("添加自定义文字"))) return true;
            m_document.setAnnotationRecord(record);
            synchronizeNotes();
            if (auto* item = m_noteItems.value(record.id)) {
                m_scene.clearSelection(); item->setSelected(true);
            }
        }
        return true;
    }
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
