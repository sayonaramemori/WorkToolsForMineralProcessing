#pragma once

#include "annotations/AnnotationTypes.h"
#include "services/FlowGroupSnapshot.h"
#include <memory>

class QGraphicsScene;

namespace afs {
class AnnotationManager;
class FlowsheetDocument;
class FlowsheetScene;

// An isolated rendering session. It owns no pointers into the open project.
class FlowTemplatePreviewSession final {
public:
    FlowTemplatePreviewSession(const flow_group::Snapshot& snapshot,
                               const AnnotationTextSettings& textSettings);
    ~FlowTemplatePreviewSession();
    QGraphicsScene* scene() const;
    void refreshAppearance(const AnnotationTextSettings& textSettings);

private:
    std::unique_ptr<FlowsheetDocument> m_document;
    std::unique_ptr<FlowsheetScene> m_scene;
    std::unique_ptr<AnnotationManager> m_annotations;
};
}
