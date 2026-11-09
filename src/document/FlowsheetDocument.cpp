#include "document/FlowsheetDocument.h"

#include <QSet>
#include <utility>

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

void FlowsheetDocument::replaceProjectData(
    QHash<QString, AnnotationRecord> annotations,
    QHash<QString, QString> productNames,
    AnnotationTextSettings annotationTextSettings,
    QStringList exportStreamOrder) {
    ExperimentScenario scenario{"scenario-1", "基准方案", {}, {}, std::nullopt};
    m_sharedAnnotations.clear();
    for (auto it = annotations.cbegin(); it != annotations.cend(); ++it) {
        if (it->kind == AnnotationKind::UserNote) scenario.noteAnnotations.insert(it.key(), it.value());
        else if (it->kind == AnnotationKind::Reagent)
            scenario.reagentAnnotations.insert(it.key(), it.value());
        else m_sharedAnnotations.insert(it.key(), it.value());
    }
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
