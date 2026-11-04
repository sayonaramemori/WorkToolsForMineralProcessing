#include "annotations/AnnotationManager.h"
#include "annotations/AnnotationItem.h"
#include "annotations/ReagentAnnotationItem.h"
#include "document/FlowsheetDocument.h"
#include "editor/FlowsheetScene.h"
#include <QSet>
#include <algorithm>

namespace afs {

void AnnotationManager::synchronizeNotes() {
    QSet<QString> active;
    const auto records = m_document.annotationRecords();
    for (auto it = records.cbegin(); it != records.cend(); ++it) {
        const auto& record = it.value();
        if (record.kind != AnnotationKind::UserNote || record.text.isEmpty()) continue;
        active.insert(record.id);
        auto* item = m_noteItems.value(record.id, nullptr);
        if (!item) {
            item = new AnnotationItem(record, record.text);
            m_scene.addItem(item); m_noteItems.insert(record.id, item);
            connect(item, &AnnotationItem::placementEdited, &m_document,
                    &FlowsheetDocument::setAnnotationRecord);
            connect(item, &AnnotationItem::recordEdited, &m_document,
                    &FlowsheetDocument::setAnnotationRecord);
        } else {
            item->setRecord(record);
        }
        item->setTextSettings(m_document.annotationTextSettings());
        item->setAnchor({}, record.manualOffset);
        item->setVisible(record.visible);
    }
    for (auto it = m_noteItems.begin(); it != m_noteItems.end();) {
        if (active.contains(it.key())) { ++it; continue; }
        auto* item = it.value(); it = m_noteItems.erase(it);
        m_scene.removeItem(item); delete item;
    }
}

void AnnotationManager::synchronizeReagents() {
    QSet<QString> active;
    const auto records = m_document.annotationRecords();
    QVector<AnnotationRecord> reagentRecords;
    for (auto it = records.cbegin(); it != records.cend(); ++it) {
        const auto& record = it.value();
        if (record.kind == AnnotationKind::Reagent
            && (!record.text.isEmpty() || !record.dosage.isEmpty() || !record.note.isEmpty()))
            reagentRecords.append(record);
    }
    std::sort(reagentRecords.begin(), reagentRecords.end(),
              [](const auto& a, const auto& b) {
                  if (a.ownerId != b.ownerId) return a.ownerId < b.ownerId;
                  return a.id < b.id;
              });
    QHash<QString, qreal> ownerStackOffsets;
    for (const auto& record : reagentRecords) {
        QPointF anchor, offset;
        if (!anchorForStream(record.ownerId, anchor, offset)) continue;
        active.insert(record.id);
        auto* item = m_reagentItems.value(record.id, nullptr);
        if (!item) {
            item = new ReagentAnnotationItem(record);
            m_scene.addItem(item); m_reagentItems.insert(record.id, item);
            connect(item, &ReagentAnnotationItem::recordEdited, &m_document,
                    &FlowsheetDocument::setAnnotationRecord);
        } else item->setRecord(record);
        item->setTextSettings(m_document.annotationTextSettings());
        const qreal stackOffset = ownerStackOffsets.value(record.ownerId, 0.0);
        item->setAnchor(anchor + QPointF(0.0, stackOffset));
        ownerStackOffsets.insert(record.ownerId,
                                 stackOffset + item->boundingRect().height() + 8.0);
        item->setVisible(record.visible);
    }
    for (auto it = m_reagentItems.begin(); it != m_reagentItems.end();) {
        if (active.contains(it.key())) { ++it; continue; }
        auto* item = it.value(); it = m_reagentItems.erase(it);
        m_scene.removeItem(item); delete item;
    }
}

} // namespace afs
