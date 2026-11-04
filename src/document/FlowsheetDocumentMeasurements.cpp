#include "document/FlowsheetDocument.h"
#include <QSet>
#include <utility>

namespace afs {

StreamMeasurement FlowsheetDocument::measurement(const QString& streamId) const {
    return currentScenario().measurements.value(streamId);
}

const QHash<QString, StreamMeasurement>& FlowsheetDocument::measurements() const {
    return currentScenario().measurements;
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

} // namespace afs
