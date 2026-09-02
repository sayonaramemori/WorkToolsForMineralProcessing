#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <optional>

namespace afs {

struct StreamMeasurement {
    std::optional<double> dryMass;
    std::optional<double> gradePercent;
    std::optional<double> dryMassSharePercent;
    std::optional<double> componentSharePercent;
    QHash<QString, double> gradePercents;
    QHash<QString, double> componentSharePercents;

    [[nodiscard]] bool complete() const { return dryMass.has_value() && gradePercent.has_value(); }
    [[nodiscard]] std::optional<double> grade(const QString& componentId) const {
        const auto found = gradePercents.constFind(componentId);
        if (found != gradePercents.cend()) return found.value();
        return componentId == QStringLiteral("component-1") ? gradePercent : std::nullopt;
    }
    [[nodiscard]] std::optional<double> componentShare(const QString& componentId) const {
        const auto found = componentSharePercents.constFind(componentId);
        if (found != componentSharePercents.cend()) return found.value();
        return componentId == QStringLiteral("component-1") ? componentSharePercent : std::nullopt;
    }
    [[nodiscard]] bool completeFor(const QStringList& componentIds) const {
        if (!dryMass) return false;
        for (const auto& id : componentIds) if (!grade(id)) return false;
        return true;
    }
};

} // namespace afs
