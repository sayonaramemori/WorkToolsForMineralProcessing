#pragma once

#include "annotations/AnnotationTypes.h"
#include "core/FlotationUnit.h"
#include "document/ComponentDefinition.h"
#include "document/ExperimentScenario.h"
#include "document/StreamMeasurement.h"

#include <QHash>
#include <QSet>
#include <QStringList>
#include <QVector>
#include <optional>

namespace afs::project_serialization {

struct UnitData { FlotationUnit unit; QHash<QString, double> terminalLengths; };
struct DirectConnectionData {
    QString sourceStreamId; QString targetUnitId; std::optional<double> routeY;
};
struct ProductMergeData {
    QString id; QVector<QString> streamIds; QString targetUnitId;
    std::optional<double> mergeY;
};
struct FeedJunctionData {
    QString id; QVector<QString> sourceTypes; QVector<QString> sourceIds;
    QVector<std::optional<double>> routeXs; QVector<std::optional<double>> routeYs;
    QString targetUnitId; QString processSourceType; QString processSourceId;
    bool hasExternalFeed{false};
};
struct ProjectData {
    QVector<UnitData> units;
    QVector<DirectConnectionData> connections;
    QVector<ProductMergeData> productMerges;
    QVector<FeedJunctionData> feedJunctions;
    QHash<QString, StreamMeasurement> measurements;
    QHash<QString, QString> productNames;
    QHash<QString, AnnotationRecord> annotations;
    QVector<ExperimentScenario> scenarios;
    QSet<QString> calculatedScenarioIds;
    int currentScenarioIndex{0};
    AnnotationTextSettings annotationTextSettings;
    QVector<ComponentDefinition> components{{DefaultComponentId, DefaultComponentName}};
    QStringList exportStreamOrder;
};

} // namespace afs::project_serialization
