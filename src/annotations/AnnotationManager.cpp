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
#include <utility>

namespace afs {

AnnotationManager::AnnotationManager(FlowsheetScene& scene, FlowsheetDocument& document, QObject* parent)
    : QObject(parent), m_scene(scene), m_document(document) {
    loadDisplayPreferences();
    connect(&m_document, &FlowsheetDocument::calculationChanged, this, [this] {
        // A new calculation (or invalidation) starts a fresh display session.
        m_temporarilyHiddenResults.clear();
        synchronize();
    });
    connect(&m_scene, &FlowsheetScene::geometryChanged, this, &AnnotationManager::refreshPositions);
    connect(&m_document, &FlowsheetDocument::annotationTextSettingsChanged,
            this, &AnnotationManager::refreshTextSettings);
    connect(&m_document, &FlowsheetDocument::productNameChanged,
            this, [this] { synchronize(); });
    connect(&m_document, &FlowsheetDocument::currentScenarioChanged, this, [this] {
        synchronizeReagents(); synchronize();
    });
    m_scene.installEventFilter(this);
    synchronizeReagents();
    synchronizeNotes();
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
    synchronizeDisplaySettings();
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
        bool hasCalculatedGrade = false;
        const auto measurement = m_document.measurement(it.key());
        // Only complete mass/grade observations are consumed by the solver.
        // An unused partial input must not turn a derived result into a measurement.
        const auto measured = [&](const QString& componentId) {
            const auto grade = measurement.grade(componentId);
            return measurement.dryMass && grade
                && topology::StreamValue::fromMassAndGrade(*measurement.dryMass, *grade).has_value();
        };
        for (const auto& component : m_document.components()) {
            const auto componentResult = result->components.constFind(component.id);
            if (componentResult == result->components.cend()
                || !componentResult->values.contains(it.key())) continue;
            const auto metric = componentResult->relativeToExternalFeed.constFind(it.key());
            componentValues.append({component.name,
                componentResult->values.value(it.key()).gradePercent(),
                metric == componentResult->relativeToExternalFeed.cend() ? nullptr : &metric.value()});
            hasCalculatedGrade = hasCalculatedGrade || !measured(component.id);
        }
        const bool primaryMeasured = !m_document.components().isEmpty()
            && measured(m_document.components().front().id);
        if (componentValues.isEmpty()) hasCalculatedGrade = !primaryMeasured;
        const QString text = AnnotationContentFormatter::formatStreamResult(
            it.value(), overall, m_settings, componentValues, m_document.productName(it.key()));
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
        annotation->setCalculatedResultBorderWidth(m_calculatedResultBorderWidth);
        annotation->setCalculatedResultEmphasis(m_settings.distinguishValueSources
            && ((m_settings.showDryMass && !primaryMeasured)
                || (m_settings.showGrade && hasCalculatedGrade)));
        annotation->setAnchor(anchor, defaultOffset);
        annotation->setVisible(m_visible && m_settings.anyVisible()
            && annotation->record().visible
            && !m_temporarilyHiddenResults.contains(annotationId));
    }
    for (auto it = m_items.begin(); it != m_items.end(); ++it)
        if (!activeIds.contains(it.key())) it.value()->setVisible(false);
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

} // namespace afs
