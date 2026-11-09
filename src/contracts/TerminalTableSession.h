#pragma once
#include "document/ComponentDefinition.h"
#include "document/ExperimentScenario.h"
#include "document/StreamDescriptor.h"
#include <QObject>
#include <QSet>
#include <variant>

namespace afs {
struct TerminalTableState {
    QVector<StreamDescriptor> streams;
    QSet<QString> editableStreamIds;
    QHash<QString, StreamMeasurement> measurements;
    QHash<QString, QString> productNames;
    QVector<ComponentDefinition> components;
    CalculationMode mode{CalculationMode::Strict};
    std::optional<topology::CalculationResult> calculation;

    QStringList componentIds() const {
        QStringList ids;
        for (const auto& component : components) ids.append(component.id);
        return ids;
    }
    const topology::CalculationResult* result() const {
        return calculation ? &*calculation : nullptr;
    }
};

enum class MeasurementField { DryMass, Grade, MassStdDev, GradeStdDev };
struct EditTableMeasurement {
    QString streamId, componentId;
    MeasurementField field;
    QString text;
};
struct EditTableProductName { QString streamId, name; };
struct EditTableComponents { QStringList names; };
struct EditTableMode { CalculationMode mode; };
using TableCommand = std::variant<EditTableMeasurement, EditTableProductName,
                                  EditTableComponents, EditTableMode>;
enum class TableChange { All, Measurement, ProductName, Result };

// UI-facing port: immutable read state and synchronous, validated commands.
// It does not expose a document, scene or concrete application controller.
class TerminalTableSession : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual const TerminalTableState& snapshot() const = 0;
    virtual bool submit(const TableCommand& command) = 0;
signals:
    void stateChanged(const afs::TerminalTableState& state, const QString& streamId, afs::TableChange change);
};
} // namespace afs
