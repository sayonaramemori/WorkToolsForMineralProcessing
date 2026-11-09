#include "document/FlowsheetDocument.h"
#include <algorithm>
#include <utility>

namespace afs {

void FlowsheetDocument::setAnnotationTextSettings(AnnotationTextSettings settings) {
    settings.pointSize = std::clamp(settings.pointSize, 7, 36);
    settings.customMassUnit = settings.customMassUnit.simplified().left(16);
    if (settings.massUnit == MassUnit::Custom && settings.customMassUnit.isEmpty())
        settings.massUnit = MassUnit::Gram;
    if (m_annotationTextSettings.pointSize == settings.pointSize
        && m_annotationTextSettings.bold == settings.bold
        && m_annotationTextSettings.color == settings.color
        && m_annotationTextSettings.massUnit == settings.massUnit
        && m_annotationTextSettings.customMassUnit == settings.customMassUnit
        && m_annotationTextSettings.resultAnnotationsVisible == settings.resultAnnotationsVisible
        && m_annotationTextSettings.showDryMass == settings.showDryMass
        && m_annotationTextSettings.showGrade == settings.showGrade
        && m_annotationTextSettings.showOverallYield == settings.showOverallYield
        && m_annotationTextSettings.showOverallRecovery == settings.showOverallRecovery
        && m_annotationTextSettings.showProductName == settings.showProductName
        && m_annotationTextSettings.metricLabelMode == settings.metricLabelMode) return;
    m_annotationTextSettings = std::move(settings);
    emit projectChanged();
    emit annotationTextSettingsChanged();
}

AnnotationRecord FlowsheetDocument::annotationRecord(const QString& annotationId) const {
    const auto note = currentScenario().noteAnnotations.constFind(annotationId);
    if (note != currentScenario().noteAnnotations.cend()) return note.value();
    const auto reagent = currentScenario().reagentAnnotations.constFind(annotationId);
    return reagent != currentScenario().reagentAnnotations.cend()
        ? reagent.value() : m_sharedAnnotations.value(annotationId);
}

QHash<QString, AnnotationRecord> FlowsheetDocument::annotationRecords() const {
    auto result = m_sharedAnnotations;
    for (auto it = currentScenario().noteAnnotations.cbegin();
         it != currentScenario().noteAnnotations.cend(); ++it) result.insert(it.key(), it.value());
    for (auto it = currentScenario().reagentAnnotations.cbegin();
         it != currentScenario().reagentAnnotations.cend(); ++it) result.insert(it.key(), it.value());
    return result;
}

void FlowsheetDocument::setAnnotationRecord(AnnotationRecord record) {
    if (record.kind == AnnotationKind::Reagent)
        currentScenario().reagentAnnotations.insert(record.id, std::move(record));
    else if (record.kind == AnnotationKind::UserNote)
        currentScenario().noteAnnotations.insert(record.id, std::move(record));
    else m_sharedAnnotations.insert(record.id, std::move(record));
    emit projectChanged();
}

void FlowsheetDocument::removeAnnotationRecord(const QString& annotationId) {
    currentScenario().reagentAnnotations.remove(annotationId);
    currentScenario().noteAnnotations.remove(annotationId);
    m_sharedAnnotations.remove(annotationId);
    emit projectChanged();
}

} // namespace afs
