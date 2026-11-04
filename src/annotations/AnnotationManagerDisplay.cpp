#include "annotations/AnnotationManager.h"
#include "annotations/AnnotationItem.h"
#include "annotations/ReagentAnnotationItem.h"
#include "document/FlowsheetDocument.h"
#include <QSettings>
#include <utility>

namespace afs {
namespace {
constexpr auto ValueSourcesPreference = "appearance/distinguishValueSources";
constexpr auto BorderWidthPreference = "appearance/calculatedResultBorderWidth";
}

void AnnotationManager::loadDisplayPreferences() {
    m_settings.distinguishValueSources = QSettings().value(ValueSourcesPreference, true).toBool();
    bool validWidth = false;
    const qreal storedWidth = QSettings().value(
        BorderWidthPreference, ResultBorderStyle::DefaultWidth).toDouble(&validWidth);
    m_calculatedResultBorderWidth = validWidth
        ? ResultBorderStyle::boundedWidth(storedWidth) : ResultBorderStyle::DefaultWidth;
}

void AnnotationManager::synchronizeDisplaySettings() {
    const auto& persisted = m_document.annotationTextSettings();
    m_settings.massUnit = persisted.massUnit;
    m_settings.customMassUnit = persisted.customMassUnit;
    m_settings.showDryMass = persisted.showDryMass;
    m_settings.showGrade = persisted.showGrade;
    m_settings.showOverallYield = persisted.showOverallYield;
    m_settings.showOverallRecovery = persisted.showOverallRecovery;
    m_settings.showProductName = persisted.showProductName;
    m_settings.labelMode = persisted.metricLabelMode;
    m_visible = persisted.resultAnnotationsVisible;
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
    // Text settings also carry the mass-unit label, which changes result-card
    // content rather than only font geometry.
    synchronize();
    refreshPositions();
}

void AnnotationManager::setAnnotationsVisible(bool visible) {
    auto persisted = m_document.annotationTextSettings();
    persisted.resultAnnotationsVisible = visible;
    m_document.setAnnotationTextSettings(persisted);
}

void AnnotationManager::setMetricVisible(ResultMetric metric, bool visible) {
    auto persisted = m_document.annotationTextSettings();
    switch (metric) {
    case ResultMetric::DryMass: persisted.showDryMass = visible; break;
    case ResultMetric::Grade: persisted.showGrade = visible; break;
    case ResultMetric::OverallYield: persisted.showOverallYield = visible; break;
    case ResultMetric::OverallRecovery: persisted.showOverallRecovery = visible; break;
    }
    m_document.setAnnotationTextSettings(persisted);
}

void AnnotationManager::setProductNamesVisible(bool visible) {
    auto persisted = m_document.annotationTextSettings();
    if (persisted.showProductName == visible) return;
    persisted.showProductName = visible;
    m_document.setAnnotationTextSettings(persisted);
}

void AnnotationManager::setDistinguishValueSources(bool enabled) {
    if (m_settings.distinguishValueSources == enabled) return;
    m_settings.distinguishValueSources = enabled;
    QSettings().setValue(ValueSourcesPreference, enabled);
    synchronize();
    emit valueSourcesVisibilityChanged(enabled);
}

void AnnotationManager::setCalculatedResultBorderWidth(qreal width) {
    width = ResultBorderStyle::boundedWidth(width);
    if (m_calculatedResultBorderWidth == width) return;
    m_calculatedResultBorderWidth = width;
    QSettings().setValue(BorderWidthPreference, width);
    for (auto* item : m_items) item->setCalculatedResultBorderWidth(width);
}

void AnnotationManager::setMetricLabelMode(MetricLabelMode mode) {
    auto persisted = m_document.annotationTextSettings();
    if (persisted.metricLabelMode == mode) return;
    persisted.metricLabelMode = mode;
    m_document.setAnnotationTextSettings(persisted);
}

void AnnotationManager::setMassUnit(MassUnit unit) {
    auto settings = m_document.annotationTextSettings();
    if (settings.massUnit == unit) return;
    settings.massUnit = unit;
    m_document.setAnnotationTextSettings(std::move(settings));
}

void AnnotationManager::setCustomMassUnit(const QString& unit) {
    auto settings = m_document.annotationTextSettings();
    settings.massUnit = MassUnit::Custom;
    settings.customMassUnit = unit;
    m_document.setAnnotationTextSettings(std::move(settings));
}

} // namespace afs
