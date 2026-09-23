#include "annotations/AnnotationContentFormatter.h"

#include <QStringList>

namespace afs {

QString AnnotationContentFormatter::formatStreamResult(
    const topology::StreamValue& value, const topology::ProductMetrics* overall,
    const ResultAnnotationSettings& settings,
    const QVector<ComponentDisplayValue>& components, const QString& productName) {
    QStringList lines;
    if (settings.showProductName && !productName.simplified().isEmpty())
        lines.append(QStringLiteral("产品  %1").arg(productName.simplified()));
    const int decimals = settings.decimalPlaces;
    const bool symbols = settings.labelMode == MetricLabelMode::Symbols;
    const QString massLabel = symbols ? QStringLiteral("m") : QStringLiteral("质量");
    const QString gradeLabel = symbols ? QStringLiteral("β") : QStringLiteral("品位");
    const QString yieldLabel = symbols ? QStringLiteral("γ") : QStringLiteral("产率");
    const QString recoveryLabel = symbols ? QStringLiteral("ε") : QStringLiteral("回收率");
    if (settings.showDryMass)
        lines.append(QString("%1  %2 %3").arg(massLabel)
            .arg(value.dryMass, 0, 'f', decimals)
            .arg(settings.massUnit == MassUnit::Custom
                     ? settings.customMassUnit : massUnitSymbol(settings.massUnit)));
    if (settings.showGrade) {
        if (components.isEmpty())
            lines.append(QString("%1  %2 %").arg(gradeLabel).arg(value.gradePercent(), 0, 'f', decimals));
        else for (const auto& component : components)
            lines.append(QString("%1 %2  %3 %").arg(gradeLabel, component.name)
                         .arg(component.gradePercent, 0, 'f', decimals));
    }
    if (settings.showOverallYield)
        lines.append(overall ? QString("%1  %2 %").arg(yieldLabel).arg(
                                   overall->massYieldPercent, 0, 'f', decimals)
                             : QString("%1  —").arg(yieldLabel));
    if (settings.showOverallRecovery) {
        if (components.isEmpty())
            lines.append(overall ? QString("%1  %2 %").arg(recoveryLabel).arg(
                                       overall->recoveryPercent, 0, 'f', decimals)
                                 : QString("%1  —").arg(recoveryLabel));
        else for (const auto& component : components)
            lines.append(component.overall
                ? QString("%1 %2  %3 %").arg(recoveryLabel, component.name)
                      .arg(component.overall->recoveryPercent, 0, 'f', decimals)
                : QString("%1 %2  —").arg(recoveryLabel, component.name));
    }
    return lines.join('\n');
}

} // namespace afs
