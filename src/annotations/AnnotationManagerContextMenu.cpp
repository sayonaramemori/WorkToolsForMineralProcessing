#include "annotations/AnnotationManager.h"
#include "annotations/AnnotationContextTarget.h"
#include "annotations/AnnotationItem.h"
#include "annotations/ReagentAnnotationItem.h"
#include "document/FlowsheetDocument.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FeedJunctionItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"
#include <QAction>
#include <QGraphicsSceneContextMenuEvent>
#include <QMenu>

namespace afs {
bool AnnotationManager::showContextMenu(QGraphicsSceneContextMenuEvent* context) {
    const auto target = annotationContextTarget(m_scene, context->scenePos());
    auto* owner = target.item;
    const QString streamId = target.streamId;
    if (owner) {
        m_scene.clearSelection(); owner->setSelected(true);
        if (auto* feed = dynamic_cast<FeedJunctionItem*>(owner)) {
            if (!feed->selectSourceStream(streamId)) feed->selectSourceStream({});
            feed->selectAnnotationStreamAt(context->scenePos());
        }
        if (auto* merge = dynamic_cast<MergeJunctionItem*>(owner)) merge->selectAnnotationStreamAt(context->scenePos());
    }
    QMenu menu;
    menu.setToolTipsVisible(true);
    auto* copy = target.reagentId.isEmpty() ? nullptr : menu.addAction(tr("复制药剂标注"));
    if (copy) copy->setObjectName("copyReagentAnnotationAction");
    auto* paste = target.line ? menu.addAction(tr("粘贴药剂标注")) : nullptr;
    if (paste) {
        paste->setObjectName("pasteReagentAnnotationAction");
        paste->setData(streamId); paste->setEnabled(m_reagentPasteAvailable && !streamId.isEmpty());
        paste->setToolTip(!m_reagentPasteAvailable ? tr("请先右键药剂标注并选择复制")
            : streamId.isEmpty() ? tr("请右键一段可以明确区分的物流线或支路") : tr("粘贴到当前右键位置的物流"));
    }
    auto* addReagent = streamId.isEmpty() ? nullptr : menu.addAction(tr("添加药剂标注"));
    auto* addNote = menu.addAction(tr("添加自定义文字"));
    QAction* resetRoute = nullptr;
    if (!streamId.isEmpty()) {
        if (auto* product = dynamic_cast<ProductLineItem*>(owner); product && product->manualRouteY())
            resetRoute = menu.addAction(tr("恢复自动布线"));
        else if (auto* merge = dynamic_cast<MergeJunctionItem*>(owner); merge && merge->manualMergeY())
            resetRoute = menu.addAction(tr("恢复自动布线"));
        else if (auto* feed = dynamic_cast<FeedJunctionItem*>(owner);
                 feed && (!feed->manualRouteXs().isEmpty() || !feed->manualRouteYs().isEmpty()))
            resetRoute = menu.addAction(tr("恢复全部支路自动布线"));
    }
    if (resetRoute) resetRoute->setObjectName("resetAnnotationRouteAction");
    auto* selected = menu.exec(context->screenPos());
    if (!selected) return true;
    if (selected == copy) { emit reagentCopyRequested(target.reagentId); return true; }
    if (selected == paste) { emit reagentPasteRequested(streamId); return true; }
    if (selected == resetRoute && m_scene.items().contains(owner)) {
        if (auto* product = dynamic_cast<ProductLineItem*>(owner)) product->setManualRouteY(std::nullopt);
        else if (auto* merge = dynamic_cast<MergeJunctionItem*>(owner)) merge->setManualMergeY(std::nullopt);
        else if (auto* feed = dynamic_cast<FeedJunctionItem*>(owner)) feed->resetManualRoutes();
        m_scene.notifyRouteChanged();
    } else if (selected == addReagent) {
        int next = 1;
        while (m_document.annotationRecords().contains(QString("reagent-%1").arg(next))) ++next;
        AnnotationRecord record{QString("reagent-%1").arg(next), AnnotationKind::Reagent,
            AnnotationStyle::PlainText, AnnotationOwnerKind::Stream, streamId, {}, false, true};
        if (!ReagentAnnotationItem::editRecord(nullptr, record, tr("添加药剂标注"))) return true;
        m_document.setAnnotationRecord(record); synchronizeReagents();
        if (auto* item = m_reagentItems.value(record.id)) { m_scene.clearSelection(); item->setSelected(true); }
    } else if (selected == addNote) {
        int next = 1;
        while (m_document.annotationRecords().contains(QString("note-%1").arg(next))) ++next;
        AnnotationRecord record{QString("note-%1").arg(next), AnnotationKind::UserNote,
            AnnotationStyle::Note, AnnotationOwnerKind::Free, QStringLiteral("canvas"),
            context->scenePos(), true, true};
        if (!AnnotationItem::editUserNote(nullptr, record, m_document.annotationTextSettings(), tr("添加自定义文字"))) return true;
        m_document.setAnnotationRecord(record); synchronizeNotes();
        if (auto* item = m_noteItems.value(record.id)) { m_scene.clearSelection(); item->setSelected(true); }
    }
    return true;
}
}
