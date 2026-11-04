#include "ProjectIoChecks.h"
#include "document/FlowsheetDocument.h"
#include "editor/FlowsheetScene.h"
#include "services/ProjectSerializer.h"
#include "services/ProjectFormat.h"
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace afs::tests {

int checkProjectFormat(const FlowsheetScene& source, const FlowsheetDocument& sourceDocument,
                       FlowsheetScene& loaded, FlowsheetDocument& loadedDocument) {
    QString error;
    const auto currentProject = QJsonDocument::fromJson(
        ProjectSerializer::serialize(source, sourceDocument)).object();
    if (currentProject.value("version").toInt() != project_serialization::FormatVersion
        || currentProject.contains("measurements")) return 68;
    const auto rejectProject = [&](const QString& description, const QJsonObject& project) {
        const QByteArray before = ProjectSerializer::serialize(loaded, loadedDocument);
        error.clear();
        if (ProjectSerializer::deserialize(loaded, loadedDocument,
                QJsonDocument(project).toJson(), &error)
            || error.isEmpty()
            || ProjectSerializer::serialize(loaded, loadedDocument) != before) {
            qCritical() << "invalid project accepted or changed open project:" << description;
            return false;
        }
        return true;
    };
    for (int version = 1; version <= project_serialization::FormatVersion + 1; ++version) {
        if (version == project_serialization::FormatVersion) continue;
        auto project = currentProject;
        project.insert("version", version);
        if (!rejectProject(QString("version %1").arg(version), project)) return 69;
    }
    for (const auto& field : {"version", "components", "productNames", "exportStreamOrder",
                              "annotationTextStyle", "scenarios", "currentScenario"}) {
        auto project = currentProject;
        project.remove(field);
        if (!rejectProject(QString("missing %1").arg(field), project)) return 70;
    }
    for (const QJsonValue version : {QJsonValue("23"), QJsonValue(23.5), QJsonValue(QJsonValue::Null)}) {
        auto project = currentProject;
        project.insert("version", version);
        if (!rejectProject("invalid version type", project)) return 85;
    }
    for (int index : {-1, static_cast<int>(sourceDocument.scenarios().size())}) {
        auto project = currentProject;
        project.insert("currentScenario", index);
        if (!rejectProject("out-of-range scenario index", project)) return 85;
    }
    const auto rejectUnit = [&](const QString& description, QJsonObject unit) {
        auto project = currentProject;
        auto units = project.value("units").toArray();
        units[0] = unit;
        project.insert("units", units);
        return rejectProject(description, project);
    };
    const auto currentUnit = currentProject.value("units").toArray().first().toObject();
    auto numericIdUnit = currentUnit;
    numericIdUnit.insert("id", 10);
    if (!rejectUnit("numeric unit ID", numericIdUnit)) return 86;
    auto stringCoordinateUnit = currentUnit;
    stringCoordinateUnit.insert("x", "120");
    if (!rejectUnit("string coordinate", stringCoordinateUnit)) return 86;
    auto nestedPositionUnit = currentUnit;
    nestedPositionUnit.remove("x"); nestedPositionUnit.remove("y");
    nestedPositionUnit.insert("position", QJsonObject{{"x", 120}, {"y", -40}});
    if (!rejectUnit("nested position", nestedPositionUnit)) return 86;
    for (const auto& field : {"width", "bodyHeight", "kind"}) {
        auto unit = currentUnit;
        unit.remove(field);
        if (!rejectUnit(QString("missing %1").arg(field), unit)) return 86;
    }
    auto oldMergeProject = currentProject;
    auto mergeRecords = oldMergeProject.value("productMerges").toArray();
    auto oldMerge = mergeRecords.first().toObject();
    const auto branches = oldMerge.take("branches").toArray();
    oldMerge.insert("first", branches[0]); oldMerge.insert("second", branches[1]);
    mergeRecords[0] = oldMerge; oldMergeProject.insert("productMerges", mergeRecords);
    if (!rejectProject("first/second merge endpoints", oldMergeProject)) return 87;
    for (const auto& field : {"sources", "hasExternalFeed"}) {
        auto project = currentProject;
        auto junctions = project.value("feedJunctions").toArray();
        auto junction = junctions.first().toObject();
        junction.remove(field);
        junction.insert("sourceType", "merge"); junction.insert("source", "merge-17");
        junctions[0] = junction; project.insert("feedJunctions", junctions);
        if (!rejectProject(QString("missing junction %1").arg(field), project)) return 87;
    }
    for (const auto& field : {"gradePercents", "gradeStdDevs", "componentSharePercents",
                              "dryMass", "dryMassStdDev", "dryMassSharePercent"}) {
        auto project = currentProject;
        auto scenarios = project.value("scenarios").toArray();
        auto scenario = scenarios.first().toObject();
        auto measurements = scenario.value("measurements").toArray();
        auto measurement = measurements.first().toObject();
        measurement.remove(field);
        measurement.insert("gradePercent", 3.25);
        measurement.insert("componentSharePercent", 50.0);
        measurements[0] = measurement; scenario.insert("measurements", measurements);
        scenarios[0] = scenario; project.insert("scenarios", scenarios);
        if (!rejectProject(QString("missing measurement %1").arg(field), project)) return 88;
    }
    for (const auto& field : {"calculated", "calculationMode"}) {
        auto project = currentProject;
        auto scenarios = project.value("scenarios").toArray();
        auto scenario = scenarios.first().toObject();
        scenario.remove(field);
        scenarios[0] = scenario; project.insert("scenarios", scenarios);
        if (!rejectProject(QString("missing scenario %1").arg(field), project)) return 89;
    }
    auto sharedReagentProject = currentProject;
    auto sharedAnnotations = sharedReagentProject.value("annotations").toArray();
    auto sharedReagent = sharedAnnotations.first().toObject();
    sharedReagent.insert("kind", static_cast<int>(AnnotationKind::Reagent));
    sharedAnnotations[0] = sharedReagent;
    sharedReagentProject.insert("annotations", sharedAnnotations);
    if (!rejectProject("reagent outside scenario", sharedReagentProject)) return 89;
    return 0;
}

} // namespace afs::tests
