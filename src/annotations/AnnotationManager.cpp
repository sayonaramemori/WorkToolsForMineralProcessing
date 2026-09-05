#include "annotations/AnnotationManager.h"
#include "annotations/AnnotationContentFormatter.h"
#include "annotations/AnnotationItem.h"
#include "annotations/ReagentAnnotationItem.h"
#include "document/FlowsheetDocument.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/FeedJunctionItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"

#include <QSet>
#include <QGraphicsSceneContextMenuEvent>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <algorithm>
#include <utility>

namespace afs {

AnnotationManager::AnnotationManager(FlowsheetScene& scene, FlowsheetDocument& document, QObject* parent)
    : QObject(parent), m_scene(scene), m_document(document) {
    connect(&m_document, &FlowsheetDocument::calculationChanged, this, &AnnotationManager::synchronize);
    connect(&m_scene, &FlowsheetScene::geometryChanged, this, &AnnotationManager::refreshPositions);
    connect(&m_document, &FlowsheetDocument::annotationTextSettingsChanged,
            this, &AnnotationManager::refreshTextSettings);
    connect(&m_document, &FlowsheetDocument::currentScenarioChanged, this, [this] {
        synchronizeReagents(); synchronize();
    });
    m_scene.installEventFilter(this);
    synchronizeReagents();
    synchronizeNotes();
}

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

bool AnnotationManager::anchorForStream(const QString& streamId, QPointF& anchor,
                                        QPointF& defaultOffset) const {
    for (auto* graphicsItem : m_scene.items()) {
        if (auto* feed = dynamic_cast<FeedJunctionItem*>(graphicsItem)) {
            if (feed->processProduct()
                && feed->processProduct()->streamId() == streamId) {
                anchor = feed->processAnnotationAnchor();
                defaultOffset = QPointF(14, -25);
                return true;
            }
            if (feed->processMerge()
                && feed->processMerge()->outputStreamId() == streamId) {
                anchor = feed->processAnnotationAnchor();
                defaultOffset = QPointF(14, -25);
                return true;
            }
            for (auto* product : feed->recycleProducts())
                if (product->streamId() == streamId) {
                    anchor = feed->sourceAnnotationAnchor(streamId);
                    defaultOffset = QPointF(14, -25);
                    return true;
                }
            for (auto* merge : feed->recycleMerges())
                if (merge->outputStreamId() == streamId) {
                    anchor = feed->sourceAnnotationAnchor(streamId);
                    defaultOffset = QPointF(14, -25);
                    return true;
                }
            if (feed->externalFeedStreamId() == streamId) {
                anchor = feed->externalFeedAnchor();
                defaultOffset = QPointF(14, -25);
                return true;
            }
            if (feed->outputStreamId() == streamId) {
                anchor = feed->outputAnchor();
                defaultOffset = QPointF(14, -25);
                return true;
            }
        }
        if (auto* product = dynamic_cast<ProductLineItem*>(graphicsItem)) {
            if (product->streamId() != streamId) continue;
            if (product->feedJunction()) continue;
            if (auto* junction = product->mergeJunction()) {
                const QPointF start = product->sourceAnchorScenePosition();
                anchor = QPointF(product->unconnectedEndScenePosition().x(),
                                 (start.y() + junction->mergeY()) / 2.0);
            } else {
                const QPainterPath path = product->path();
                anchor = product->mapToScene(path.pointAtPercent(0.52));
            }
            defaultOffset = product->side() == ProductSide::Left ? QPointF(-145, -25)
                : product->side() == ProductSide::Middle ? QPointF(-60, -25)
                                                         : QPointF(14, -25);
            return true;
        }
        if (auto* merge = dynamic_cast<MergeJunctionItem*>(graphicsItem)) {
            if (merge->outputStreamId() != streamId) continue;
            if (merge->feedJunction()) continue;
            anchor = merge->mapToScene(QPointF(merge->mergeX(), merge->mergeY() + 25.0));
            defaultOffset = QPointF(14, -25);
            return true;
        }
        if (auto* input = dynamic_cast<InputLineItem*>(graphicsItem)) {
            if (input->sourceProduct()) continue;
            const QString feedId = input->unit()->unit().id + ":feed";
            if (feedId != streamId) continue;
            anchor = input->mapToScene(input->line().center());
            defaultOffset = QPointF(14, -25);
            return true;
        }
    }
    return false;
}

void AnnotationManager::synchronize() {
    synchronizeReagents();
    synchronizeNotes();
    const auto* result = m_document.calculationResult();
    if (!result || result->values.isEmpty()) {
        for (auto* item : m_items) item->setVisible(false);
        return;
    }

    QSet<QString> activeIds;
    for (auto it = result->values.cbegin(); it != result->values.cend(); ++it) {
        QPointF anchor;
        QPointF defaultOffset;
        if (!anchorForStream(it.key(), anchor, defaultOffset)) continue;
        const QString annotationId = "result:" + it.key();
        activeIds.insert(annotationId);
        auto* annotation = m_items.value(annotationId, nullptr);
        const auto metricsIt = result->relativeToExternalFeed.constFind(it.key());
        const topology::ProductMetrics* overall = metricsIt == result->relativeToExternalFeed.cend()
            ? nullptr : &metricsIt.value();
        QVector<ComponentDisplayValue> componentValues;
        for (const auto& component : m_document.components()) {
            const auto componentResult = result->components.constFind(component.id);
            if (componentResult == result->components.cend()
                || !componentResult->values.contains(it.key())) continue;
            const auto metric = componentResult->relativeToExternalFeed.constFind(it.key());
            componentValues.append({component.name,
                componentResult->values.value(it.key()).gradePercent(),
                metric == componentResult->relativeToExternalFeed.cend() ? nullptr : &metric.value()});
        }
        const QString text = AnnotationContentFormatter::formatStreamResult(
            it.value(), overall, m_settings, componentValues);
        if (!annotation) {
            auto record = m_document.annotationRecord(annotationId);
            if (record.id.isEmpty()) {
                record = {annotationId, AnnotationKind::StreamResult, AnnotationStyle::ResultCard,
                          AnnotationOwnerKind::Stream, it.key(), {}, false, true};
                m_document.setAnnotationRecord(record);
            }
            annotation = new AnnotationItem(record, text);
            annotation->setTextSettings(m_document.annotationTextSettings());
            m_scene.addItem(annotation);
            m_items.insert(annotationId, annotation);
            connect(annotation, &AnnotationItem::placementEdited, &m_document,
                    &FlowsheetDocument::setAnnotationRecord);
        } else {
            annotation->setText(text);
        }
        annotation->setAnchor(anchor, defaultOffset);
        annotation->setVisible(m_visible && m_settings.anyVisible() && annotation->record().visible);
    }
    for (auto it = m_items.begin(); it != m_items.end(); ++it)
        if (!activeIds.contains(it.key())) it.value()->setVisible(false);
}

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

void AnnotationManager::refreshPositions() {
    for (auto it = m_items.begin(); it != m_items.end(); ++it) {
        QPointF anchor;
        QPointF defaultOffset;
        if (anchorForStream(it.value()->record().ownerId, anchor, defaultOffset))
            it.value()->setAnchor(anchor, defaultOffset);
    }
    synchronizeReagents();
    synchronizeNotes();
}

void AnnotationManager::refreshAppearance() {
    for (auto* item : m_items) item->refreshAppearance();
    for (auto* item : m_reagentItems) item->refreshAppearance();
    for (auto* item : m_noteItems) item->refreshAppearance();
}

void AnnotationManager::refreshTextSettings() {
    for (auto* item : m_items)
        item->setTextSettings(m_document.annotationTextSettings());
    for (auto* item : m_reagentItems)
        item->setTextSettings(m_document.annotationTextSettings());
    for (auto* item : m_noteItems)
        item->setTextSettings(m_document.annotationTextSettings());
    refreshPositions();
}

void AnnotationManager::clearGraphicsItems() {
    for (auto* item : std::as_const(m_items)) {
        m_scene.removeItem(item);
        delete item;
    }
    m_items.clear();
    for (auto* item : std::as_const(m_reagentItems)) {
        m_scene.removeItem(item); delete item;
    }
    m_reagentItems.clear();
    for (auto* item : std::as_const(m_noteItems)) {
        m_scene.removeItem(item); delete item;
    }
    m_noteItems.clear();
}

void AnnotationManager::setAnnotationsVisible(bool visible) {
    m_visible = visible;
    for (auto* item : m_items) item->setVisible(visible && m_settings.anyVisible() && item->record().visible
        && m_document.calculationResult()
        && m_document.calculationResult()->values.contains(item->record().ownerId));
}

void AnnotationManager::setMetricVisible(ResultMetric metric, bool visible) {
    switch (metric) {
    case ResultMetric::DryMass: m_settings.showDryMass = visible; break;
    case ResultMetric::Grade: m_settings.showGrade = visible; break;
    case ResultMetric::OverallYield: m_settings.showOverallYield = visible; break;
    case ResultMetric::OverallRecovery: m_settings.showOverallRecovery = visible; break;
    }
    synchronize();
}

void AnnotationManager::setMetricLabelMode(MetricLabelMode mode) {
    if (m_settings.labelMode == mode) return;
    m_settings.labelMode = mode;
    synchronize();
}

} // namespace afs
