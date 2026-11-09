#include "app/FlowTemplatePreviewSession.h"
#include "annotations/AnnotationManager.h"
#include "document/FlowsheetDocument.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"
#include "services/FlowGroupSnapshotOperations.h"
#include <QApplication>
#include <QPalette>

namespace afs {
FlowTemplatePreviewSession::FlowTemplatePreviewSession(const flow_group::Snapshot& snapshot,
                                                       const AnnotationTextSettings& textSettings)
    : m_document(std::make_unique<FlowsheetDocument>()), m_scene(std::make_unique<FlowsheetScene>()) {
    m_document->setAnnotationTextSettings(textSettings);
    int nextId = 0;
    flow_group::paste(snapshot, {}, *m_scene, *m_document, [&] { return QString::number(++nextId); });
    m_scene->clearSelection();
    m_annotations = std::make_unique<AnnotationManager>(*m_scene, *m_document);
    m_annotations->synchronize();
    // The preview does not need the annotation editing/context-menu filter.
    m_scene->removeEventFilter(m_annotations.get());
    refreshAppearance(textSettings);
    for (auto* item : m_scene->items()) {
        item->setFlag(QGraphicsItem::ItemIsMovable, false);
        item->setFlag(QGraphicsItem::ItemIsSelectable, false);
        item->setFlag(QGraphicsItem::ItemIsFocusable, false);
        item->setAcceptedMouseButtons(Qt::NoButton);
    }
}

FlowTemplatePreviewSession::~FlowTemplatePreviewSession() {
    m_scene->blockSignals(true);
    m_annotations->clearGraphicsItems();
}

QGraphicsScene* FlowTemplatePreviewSession::scene() const { return m_scene.get(); }

void FlowTemplatePreviewSession::refreshAppearance(const AnnotationTextSettings& textSettings) {
    m_document->setAnnotationTextSettings(textSettings);
    for (auto* item : m_scene->items()) {
        if (auto* product = dynamic_cast<ProductLineItem*>(item)) {
            product->setTextSettings(textSettings);
            product->setProductName(m_document->productName(product->streamId()));
        } else if (auto* merge = dynamic_cast<MergeJunctionItem*>(item)) {
            merge->setTextSettings(textSettings);
            merge->setProductName(m_document->productName(merge->outputStreamId()));
        }
    }
    m_scene->setBackgroundBrush(QApplication::palette().color(QPalette::Base));
    m_scene->refreshAppearance();
    m_annotations->refreshAppearance();
}
}
