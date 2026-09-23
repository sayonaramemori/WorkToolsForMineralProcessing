#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include "annotations/AnnotationTypes.h"

class QGraphicsItem;

namespace afs {

class AnnotationItem;
class FlowsheetDocument;
class FlowsheetScene;
class ReagentAnnotationItem;

class AnnotationManager final : public QObject {
    Q_OBJECT
public:
    AnnotationManager(FlowsheetScene& scene, FlowsheetDocument& document, QObject* parent = nullptr);

    void synchronize();
    void refreshPositions();
    void refreshAppearance();
    void setAnnotationsVisible(bool visible);
    void setMetricVisible(ResultMetric metric, bool visible);
    void setProductNamesVisible(bool visible);
    void setMetricLabelMode(MetricLabelMode mode);
    void setMassUnit(MassUnit unit);
    void setCustomMassUnit(const QString& unit);
    void clearGraphicsItems();
    void refreshTextSettings();
    bool nudgeSelectedReagents(qreal verticalDelta);
    [[nodiscard]] bool isMetricVisible(ResultMetric metric) const { return m_settings.visible(metric); }
    [[nodiscard]] int annotationCount() const { return m_items.size(); }

private:
    FlowsheetScene& m_scene;
    FlowsheetDocument& m_document;
    QHash<QString, AnnotationItem*> m_items;
    QHash<QString, ReagentAnnotationItem*> m_reagentItems;
    QHash<QString, AnnotationItem*> m_noteItems;
    // Result cards hidden with Delete for the current calculation only.
    // This is intentionally not persisted in FlowsheetDocument.
    QSet<QString> m_temporarilyHiddenResults;
    bool m_visible{true};
    ResultAnnotationSettings m_settings;

    [[nodiscard]] bool anchorForStream(const QString& streamId, QPointF& anchor,
                                       QPointF& defaultOffset) const;
    void synchronizeReagents();
    void synchronizeNotes();
    void synchronizeDisplaySettings();
    bool eventFilter(QObject* watched, QEvent* event) override;
    [[nodiscard]] QString selectedLineStreamId(QGraphicsItem** owner = nullptr) const;
};

} // namespace afs
