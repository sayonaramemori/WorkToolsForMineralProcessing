#pragma once

#include "annotations/AnnotationTypes.h"
#include "document/StreamMeasurement.h"
#include "topology/TopologyTypes.h"

#include <QHash>
#include <QString>
#include <optional>

namespace afs {

enum class CalculationMode { Strict, DataReconciliation };

struct ExperimentScenario {
    QString id;
    QString name;
    QHash<QString, StreamMeasurement> measurements;
    QHash<QString, AnnotationRecord> reagentAnnotations;
    std::optional<topology::CalculationResult> calculationResult;
    CalculationMode calculationMode{CalculationMode::Strict};
};

} // namespace afs
