#include "services/ProjectJsonReadSupport.h"
#include <QJsonArray>
#include <QSet>

namespace afs::project_serialization::json_read {

namespace {

bool readPercentMap(const QJsonObject& object, const char* key, QHash<QString, double>& result) {
    const auto value = object.value(QLatin1String(key));
    if (!value.isObject()) return false;
    const auto values = value.toObject();
    for (auto it = values.begin(); it != values.end(); ++it) {
        double number = 0.0;
        if (it.key().isEmpty() || !finiteNumber(it.value(), number)
            || number < 0.0 || number > 100.0) return false;
        result.insert(it.key(), number);
    }
    return true;
}

bool readPositiveMap(const QJsonObject& object, const char* key,
                     QHash<QString, double>& result) {
    const auto value = object.value(QLatin1String(key));
    if (!value.isObject()) return false;
    const auto values = value.toObject();
    for (auto it = values.begin(); it != values.end(); ++it) {
        double number = 0.0;
        if (it.key().isEmpty() || !finiteNumber(it.value(), number) || number <= 0.0)
            return false;
        result.insert(it.key(), number);
    }
    return true;
}

bool readMeasurement(const QJsonObject& object, StreamMeasurement& measurement) {
    bool valid = true;
    if (!object.contains("dryMass") || !object.contains("dryMassStdDev")
        || !object.contains("dryMassSharePercent")) return false;
    measurement.dryMass = readOptionalNumber(object, "dryMass", valid);
    measurement.dryMassStdDev = readOptionalNumber(object, "dryMassStdDev", valid);
    measurement.dryMassSharePercent = readOptionalNumber(object, "dryMassSharePercent", valid);
    QHash<QString, double> grades, gradeStdDevs, componentShares;
    if (!readPercentMap(object, "gradePercents", grades)
        || !readPositiveMap(object, "gradeStdDevs", gradeStdDevs)
        || !readPercentMap(object, "componentSharePercents", componentShares)) return false;
    for (auto it = grades.cbegin(); it != grades.cend(); ++it)
        measurement.components[it.key()].gradePercent = it.value();
    for (auto it = gradeStdDevs.cbegin(); it != gradeStdDevs.cend(); ++it)
        measurement.components[it.key()].gradeStdDev = it.value();
    for (auto it = componentShares.cbegin(); it != componentShares.cend(); ++it)
        measurement.components[it.key()].sharePercent = it.value();
    return valid && (!measurement.dryMass || *measurement.dryMass >= 0.0)
        && (!measurement.dryMassStdDev || *measurement.dryMassStdDev > 0.0)
        && (!measurement.dryMassSharePercent
            || (*measurement.dryMassSharePercent >= 0.0
                && *measurement.dryMassSharePercent <= 100.0));
}

} // namespace

bool readScenarios(const QJsonObject& root, ProjectData& data, QString* error) {
    const auto scenarios = root.value("scenarios");
    if (!scenarios.isArray() || scenarios.toArray().isEmpty()) {
        setError(error, "试验方案数据无效"); return false;
    }
    QSet<QString> scenarioIds;
    for (const auto& scenarioValue : scenarios.toArray()) {
        if (!scenarioValue.isObject()) { setError(error, "试验方案记录无效"); return false; }
        const auto object = scenarioValue.toObject();
        ExperimentScenario scenario;
        if (!readRequiredString(object, "id", scenario.id)
            || !readRequiredString(object, "name", scenario.name)
            || scenario.name.size() > 80 || scenarioIds.contains(scenario.id)
            || !object.value("measurements").isArray()
            || !object.value("reagents").isArray()) {
            setError(error, "试验方案名称、ID或数据数组无效"); return false;
        }
        const auto calculated = object.value("calculated");
        if (!calculated.isBool()) {
            setError(error, "试验方案计算状态无效"); return false;
        }
        scenarioIds.insert(scenario.id);
        const QString mode = object.value("calculationMode").toString();
        if (mode != "strict" && mode != "reconciliation") {
            setError(error, "试验方案计算模式无效"); return false;
        }
        scenario.calculationMode = mode == "reconciliation"
            ? CalculationMode::DataReconciliation : CalculationMode::Strict;
        if (calculated.toBool()) data.calculatedScenarioIds.insert(scenario.id);
        for (const auto& measurementValue : object.value("measurements").toArray()) {
            if (!measurementValue.isObject()) { setError(error, "方案实测数据无效"); return false; }
            const auto measurementObject = measurementValue.toObject();
            QString streamId;
            if (!readRequiredString(measurementObject, "streamId", streamId)
                || scenario.measurements.contains(streamId)) {
                setError(error, "方案实测物流 ID 无效或重复"); return false;
            }
            StreamMeasurement measurement;
            if (!readMeasurement(measurementObject, measurement)) {
                setError(error, "方案实测数据范围无效"); return false;
            }
            scenario.measurements.insert(streamId, measurement);
        }
        for (const auto& reagentValue : object.value("reagents").toArray()) {
            if (!reagentValue.isObject()) { setError(error, "方案药剂标注无效"); return false; }
            const auto reagentObject = reagentValue.toObject();
            AnnotationRecord record;
            if (!readRequiredString(reagentObject, "id", record.id)
                || !readRequiredString(reagentObject, "ownerId", record.ownerId)
                || !reagentObject.value("text").isString()
                || reagentObject.value("text").toString().size() > 120
                || !reagentObject.value("dosage").isString()
                || !reagentObject.value("unit").isString()
                || !reagentObject.value("note").isString()
                || scenario.reagentAnnotations.contains(record.id)) {
                setError(error, "方案药剂标注字段无效"); return false;
            }
            record.text = reagentObject.value("text").toString();
            record.dosage = reagentObject.value("dosage").toString();
            record.dosageUnit = reagentObject.value("unit").toString();
            record.note = reagentObject.value("note").toString();
            double offsetY = 0.0;
            const auto manuallyPlaced = reagentObject.value("manuallyPlaced");
            if (!finiteNumber(reagentObject.value("offsetY"), offsetY)
                || !manuallyPlaced.isBool()) {
                setError(error, "方案药剂标注位置无效"); return false;
            }
            record.manualOffset = QPointF(0.0, offsetY);
            record.manuallyPlaced = manuallyPlaced.toBool();
            if (record.dosage.size() > 40 || record.dosageUnit.size() > 24
                || record.note.size() > 120
                || (record.text.isEmpty() && record.dosage.isEmpty() && record.note.isEmpty())) {
                setError(error, "方案药剂用量或备注无效"); return false;
            }
            record.kind = AnnotationKind::Reagent;
            record.style = AnnotationStyle::PlainText;
            record.ownerKind = AnnotationOwnerKind::Stream;
            scenario.reagentAnnotations.insert(record.id, record);
        }
        data.scenarios.append(std::move(scenario));
    }
    data.currentScenarioIndex = root.value("currentScenario").toInt(-1);
    if (data.currentScenarioIndex < 0 || data.currentScenarioIndex >= data.scenarios.size()) {
        setError(error, "当前试验方案索引无效"); return false;
    }
    return true;
}

} // namespace afs::project_serialization::json_read
