#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <optional>

namespace afs {

struct ComponentMeasurement {
    std::optional<double> gradePercent;
    std::optional<double> gradeStdDev;
    std::optional<double> sharePercent;
};

struct StreamMeasurement {
    std::optional<double> dryMass;
    std::optional<double> dryMassSharePercent;
    std::optional<double> dryMassStdDev;
    QHash<QString, ComponentMeasurement> components;

    [[nodiscard]] bool complete() const {
        return dryMass.has_value() && grade(QStringLiteral("component-1")).has_value();
    }
    [[nodiscard]] std::optional<double> grade(const QString& componentId) const {
        const auto found = components.constFind(componentId);
        return found == components.cend() ? std::nullopt : found->gradePercent;
    }
    [[nodiscard]] std::optional<double> componentShare(const QString& componentId) const {
        const auto found = components.constFind(componentId);
        return found == components.cend() ? std::nullopt : found->sharePercent;
    }
    [[nodiscard]] std::optional<double> gradeStdDev(const QString& componentId) const {
        const auto found = components.constFind(componentId);
        return found == components.cend() ? std::nullopt : found->gradeStdDev;
    }
    [[nodiscard]] bool completeFor(const QStringList& componentIds) const {
        if (!dryMass) return false;
        for (const auto& id : componentIds) if (!grade(id)) return false;
        return true;
    }
};

} // namespace afs
