#include "document/FlowsheetDocument.h"

#include <algorithm>
#include <QSet>

namespace afs {

FlowsheetDocument::FlowsheetDocument(QObject* parent) : QObject(parent) {
    m_scenarios.append({"scenario-1", "基准方案", {}, {}, std::nullopt,
                        CalculationMode::Strict});
}

QString FlowsheetDocument::productName(const QString& streamId) const {
    return m_productNames.value(streamId);
}

void FlowsheetDocument::setExportStreamOrder(QStringList streamIds) {
    QStringList normalized;
    for (auto id : streamIds) {
        id = id.simplified().left(160);
        if (!id.isEmpty() && !normalized.contains(id)) normalized.append(std::move(id));
    }
    if (m_exportStreamOrder == normalized) return;
    m_exportStreamOrder = std::move(normalized);
    emit projectChanged();
}

void FlowsheetDocument::setProductName(const QString& streamId, QString name) {
    name = name.simplified().left(120);
    if (m_productNames.value(streamId) == name) return;
    if (name.isEmpty()) m_productNames.remove(streamId);
    else m_productNames.insert(streamId, std::move(name));
    emit projectChanged();
    emit productNameChanged(streamId);
}

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
    const auto reagent = currentScenario().reagentAnnotations.constFind(annotationId);
    return reagent != currentScenario().reagentAnnotations.cend()
        ? reagent.value() : m_sharedAnnotations.value(annotationId);
}

QHash<QString, AnnotationRecord> FlowsheetDocument::annotationRecords() const {
    auto result = m_sharedAnnotations;
    for (auto it = currentScenario().reagentAnnotations.cbegin();
         it != currentScenario().reagentAnnotations.cend(); ++it) result.insert(it.key(), it.value());
    return result;
}

void FlowsheetDocument::setAnnotationRecord(AnnotationRecord record) {
    if (record.kind == AnnotationKind::Reagent)
        currentScenario().reagentAnnotations.insert(record.id, std::move(record));
    else m_sharedAnnotations.insert(record.id, std::move(record));
    emit projectChanged();
}

void FlowsheetDocument::removeAnnotationRecord(const QString& annotationId) {
    currentScenario().reagentAnnotations.remove(annotationId);
    m_sharedAnnotations.remove(annotationId);
    emit projectChanged();
}

void FlowsheetDocument::replaceProjectData(
    QHash<QString, AnnotationRecord> annotations,
    QHash<QString, QString> productNames,
    AnnotationTextSettings annotationTextSettings,
    QStringList exportStreamOrder) {
    ExperimentScenario scenario{"scenario-1", "基准方案", {}, {}, std::nullopt};
    m_sharedAnnotations = std::move(annotations);
    m_scenarios = {std::move(scenario)};
    m_currentScenarioIndex = 0;
    m_nextScenarioId = 2;
    m_productNames = std::move(productNames);
    m_exportStreamOrder = std::move(exportStreamOrder);
    m_annotationTextSettings = std::move(annotationTextSettings);
    emit calculationChanged();
    emit productNameChanged({});
    emit annotationTextSettingsChanged();
    emit scenariosChanged();
    emit currentScenarioChanged();
    emit calculationModeChanged();
}

void FlowsheetDocument::removeUnknownStreams(const QSet<QString>& validStreamIds) {
    bool changed = false;
    for (auto it = m_productNames.begin(); it != m_productNames.end();) {
        if (!validStreamIds.contains(it.key())) {
            it = m_productNames.erase(it); changed = true;
        } else ++it;
    }
    for (auto& scenario : m_scenarios) {
        bool scenarioChanged = false;
        for (auto it = scenario.measurements.begin(); it != scenario.measurements.end();) {
            if (!validStreamIds.contains(it.key())) {
                it = scenario.measurements.erase(it); changed = scenarioChanged = true;
            } else ++it;
        }
        for (auto it = scenario.reagentAnnotations.begin();
             it != scenario.reagentAnnotations.end();) {
            if (it->ownerKind == AnnotationOwnerKind::Stream
                && !validStreamIds.contains(it->ownerId)) {
                it = scenario.reagentAnnotations.erase(it); changed = scenarioChanged = true;
            } else ++it;
        }
        if (scenarioChanged) scenario.calculationResult.reset();
    }
    for (auto it = m_sharedAnnotations.begin(); it != m_sharedAnnotations.end();) {
        if (it->ownerKind == AnnotationOwnerKind::Stream
            && !validStreamIds.contains(it->ownerId)) {
            it = m_sharedAnnotations.erase(it); changed = true;
        } else ++it;
    }
    if (!changed) return;
    emit projectChanged();
    emit productNameChanged({});
    emit scenariosChanged();
    emit calculationChanged();
    emit measurementChanged({});
}

} // namespace afs
