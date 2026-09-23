#include "document/FlowsheetDocument.h"

#include <algorithm>
#include <QSet>

namespace afs {

FlowsheetDocument::FlowsheetDocument(QObject* parent) : QObject(parent) {
    m_scenarios.append({"scenario-1", "基准方案", {}, {}, std::nullopt,
                        CalculationMode::Strict});
}

ExperimentScenario& FlowsheetDocument::currentScenario() { return m_scenarios[m_currentScenarioIndex]; }
const ExperimentScenario& FlowsheetDocument::currentScenario() const { return m_scenarios[m_currentScenarioIndex]; }

StreamMeasurement FlowsheetDocument::measurement(const QString& streamId) const {
    return currentScenario().measurements.value(streamId);
}

const QHash<QString, StreamMeasurement>& FlowsheetDocument::measurements() const {
    return currentScenario().measurements;
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

void FlowsheetDocument::setDryMass(const QString& streamId, std::optional<double> value) {
    auto& item = currentScenario().measurements[streamId];
    if (item.dryMass == value) return;
    item.dryMass = value;
    invalidateCalculation();
    emit projectChanged();
    emit measurementChanged(streamId);
}

void FlowsheetDocument::setDryMassStdDev(const QString& streamId,
                                         std::optional<double> value) {
    auto& item = currentScenario().measurements[streamId];
    if (item.dryMassStdDev == value) return;
    item.dryMassStdDev = value;
    invalidateCalculation(); emit projectChanged(); emit measurementChanged(streamId);
}

void FlowsheetDocument::setGradePercent(const QString& streamId, std::optional<double> value) {
    setGradePercent(streamId, QString::fromLatin1(DefaultComponentId), value);
}

void FlowsheetDocument::setGradePercent(const QString& streamId, const QString& componentId,
                                        std::optional<double> value) {
    auto& item = currentScenario().measurements[streamId];
    if (item.grade(componentId) == value) return;
    if (value) item.components[componentId].gradePercent = value;
    else if (item.components.contains(componentId)) {
        item.components[componentId].gradePercent.reset();
        if (!item.components[componentId].gradeStdDev && !item.components[componentId].sharePercent)
            item.components.remove(componentId);
    }
    invalidateCalculation();
    emit projectChanged();
    emit measurementChanged(streamId);
}

void FlowsheetDocument::setGradeStdDev(const QString& streamId, const QString& componentId,
                                       std::optional<double> value) {
    auto& item = currentScenario().measurements[streamId];
    if (item.gradeStdDev(componentId) == value) return;
    if (value) item.components[componentId].gradeStdDev = value;
    else if (item.components.contains(componentId)) {
        item.components[componentId].gradeStdDev.reset();
        if (!item.components[componentId].gradePercent && !item.components[componentId].sharePercent)
            item.components.remove(componentId);
    }
    invalidateCalculation(); emit projectChanged(); emit measurementChanged(streamId);
}

CalculationMode FlowsheetDocument::calculationMode() const {
    return currentScenario().calculationMode;
}

void FlowsheetDocument::setCalculationMode(CalculationMode mode) {
    if (currentScenario().calculationMode == mode) return;
    currentScenario().calculationMode = mode;
    invalidateCalculation(); emit projectChanged(); emit calculationModeChanged();
}

void FlowsheetDocument::setDryMassSharePercent(
    const QString& streamId, std::optional<double> value) {
    auto& item = currentScenario().measurements[streamId];
    if (item.dryMassSharePercent == value) return;
    item.dryMassSharePercent = value;
    invalidateCalculation(); emit projectChanged(); emit measurementChanged(streamId);
}

void FlowsheetDocument::setComponentSharePercent(
    const QString& streamId, std::optional<double> value) {
    setComponentSharePercent(streamId, QString::fromLatin1(DefaultComponentId), value);
}

void FlowsheetDocument::setComponentSharePercent(
    const QString& streamId, const QString& componentId, std::optional<double> value) {
    auto& item = currentScenario().measurements[streamId];
    if (item.componentShare(componentId) == value) return;
    if (value) item.components[componentId].sharePercent = value;
    else if (item.components.contains(componentId)) {
        item.components[componentId].sharePercent.reset();
        if (!item.components[componentId].gradePercent && !item.components[componentId].gradeStdDev)
            item.components.remove(componentId);
    }
    invalidateCalculation(); emit projectChanged(); emit measurementChanged(streamId);
}

QStringList FlowsheetDocument::componentIds() const {
    QStringList result;
    for (const auto& component : m_components) result.append(component.id);
    return result;
}

void FlowsheetDocument::setComponents(QVector<ComponentDefinition> components) {
    if (components.isEmpty())
        components.append(ComponentDefinition{DefaultComponentId, DefaultComponentName});
    QSet<QString> ids;
    QVector<ComponentDefinition> normalized;
    for (auto component : components) {
        component.id = component.id.simplified().left(80);
        component.name = component.name.simplified().left(40);
        if (component.id.isEmpty() || component.name.isEmpty() || ids.contains(component.id)) continue;
        ids.insert(component.id); normalized.append(std::move(component));
    }
    if (normalized.isEmpty())
        normalized.append(ComponentDefinition{DefaultComponentId, DefaultComponentName});
    if (m_components == normalized) return;
    m_components = std::move(normalized);
    invalidateAllCalculations(); emit projectChanged(); emit componentsChanged();
}

const topology::CalculationResult* FlowsheetDocument::calculationResult() const {
    const auto& result = currentScenario().calculationResult;
    return result ? &*result : nullptr;
}

void FlowsheetDocument::setCalculationResult(topology::CalculationResult result) {
    currentScenario().calculationResult = std::move(result);
    emit projectChanged();
    emit calculationChanged();
}

void FlowsheetDocument::invalidateCalculation() {
    if (!currentScenario().calculationResult) return;
    currentScenario().calculationResult.reset();
    emit calculationChanged();
}

void FlowsheetDocument::invalidateAllCalculations() {
    bool changed = false;
    for (auto& scenario : m_scenarios) {
        if (scenario.calculationResult) { scenario.calculationResult.reset(); changed = true; }
    }
    if (changed) emit calculationChanged();
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
    QHash<QString, StreamMeasurement> measurements,
    QHash<QString, AnnotationRecord> annotations,
    QHash<QString, QString> productNames,
    AnnotationTextSettings annotationTextSettings,
    QStringList exportStreamOrder) {
    ExperimentScenario scenario{"scenario-1", "基准方案", std::move(measurements), {}, std::nullopt};
    m_sharedAnnotations.clear();
    for (auto it = annotations.begin(); it != annotations.end(); ++it) {
        if (it.value().kind == AnnotationKind::Reagent)
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

QString FlowsheetDocument::currentScenarioName() const { return currentScenario().name; }

bool FlowsheetDocument::setCurrentScenario(int index) {
    if (index < 0 || index >= m_scenarios.size() || index == m_currentScenarioIndex) return false;
    m_currentScenarioIndex = index;
    emit projectChanged();
    emit currentScenarioChanged(); emit calculationChanged(); emit measurementChanged({});
    emit calculationModeChanged();
    return true;
}

int FlowsheetDocument::addScenario(QString name, bool copyCurrent) {
    name = name.simplified().left(80);
    if (name.isEmpty()) return -1;
    ExperimentScenario scenario;
    if (copyCurrent) scenario = currentScenario();
    scenario.id = QString("scenario-%1").arg(m_nextScenarioId++);
    scenario.name = std::move(name);
    m_scenarios.append(std::move(scenario));
    emit projectChanged();
    emit scenariosChanged();
    return m_scenarios.size() - 1;
}

bool FlowsheetDocument::renameScenario(int index, QString name) {
    name = name.simplified().left(80);
    if (index < 0 || index >= m_scenarios.size() || name.isEmpty()) return false;
    m_scenarios[index].name = std::move(name); emit projectChanged(); emit scenariosChanged(); return true;
}

bool FlowsheetDocument::removeScenario(int index) {
    if (m_scenarios.size() <= 1 || index < 0 || index >= m_scenarios.size()) return false;
    if (index < m_currentScenarioIndex) --m_currentScenarioIndex;
    m_scenarios.removeAt(index);
    m_currentScenarioIndex = std::min(m_currentScenarioIndex,
                                      static_cast<int>(m_scenarios.size()) - 1);
    emit projectChanged();
    emit scenariosChanged(); emit currentScenarioChanged(); emit calculationChanged(); emit measurementChanged({});
    emit calculationModeChanged();
    return true;
}

void FlowsheetDocument::replaceScenarios(QVector<ExperimentScenario> scenarios, int currentIndex) {
    if (scenarios.isEmpty()) return;
    m_scenarios = std::move(scenarios);
    m_currentScenarioIndex = std::clamp(currentIndex, 0,
                                        static_cast<int>(m_scenarios.size()) - 1);
    int maximum = 0;
    for (const auto& item : m_scenarios) {
        bool ok = false; const int value = item.id.mid(QString("scenario-").size()).toInt(&ok);
        if (ok) maximum = std::max(maximum, value);
    }
    m_nextScenarioId = maximum + 1;
    emit scenariosChanged(); emit currentScenarioChanged(); emit calculationChanged(); emit measurementChanged({});
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
