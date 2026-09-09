#include "services/ProjectJsonReader.h"
#include "annotations/AnnotationTypes.h"

#include <QColor>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <algorithm>
#include <cmath>

namespace afs {
namespace {
constexpr int kFormatVersion = 9;
using namespace project_serialization;
void setError(QString* destination, const QString& message) {
    if (destination) *destination = message;
}
std::optional<UnitKind> unitKindFromKey(const QString& key) {
    if (key == "flotation") return UnitKind::Flotation;
    if (key == "binary-splitter") return UnitKind::BinarySplitter;
    if (key == "three-product-flotation") return UnitKind::ThreeProductFlotation;
    return std::nullopt;
}

bool finiteNumber(const QJsonValue& value, double& result) {
    if (!value.isDouble()) return false;
    result = value.toDouble();
    return std::isfinite(result);
}

bool readRequiredString(const QJsonObject& object, const char* key, QString& result) {
    const auto value = object.value(QLatin1String(key));
    if (!value.isString() || value.toString().isEmpty()) return false;
    result = value.toString();
    return true;
}

std::optional<double> readOptionalNumber(
    const QJsonObject& object, const char* key, bool& valid) {
    const auto value = object.value(QLatin1String(key));
    if (value.isUndefined() || value.isNull()) return std::nullopt;
    double number = 0.0;
    if (!finiteNumber(value, number)) {
        valid = false;
        return std::nullopt;
    }
    return number;
}

bool readPercentMap(const QJsonObject& object, const char* key, QHash<QString, double>& result) {
    const auto value = object.value(QLatin1String(key));
    if (value.isUndefined()) return true;
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
    if (value.isUndefined()) return true;
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

} // namespace

bool ProjectJsonReader::parse(const QByteArray& contents, ProjectData& data, QString* error) {
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(contents, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(error, QString("项目 JSON 无效：%1").arg(parseError.errorString()));
        return false;
    }
    const auto root = document.object();
    const int formatVersion = root.value("version").toInt(-1);
    if (root.value("format").toString() != "AutoFlotationSheet"
        || formatVersion < 1 || formatVersion > kFormatVersion) {
        setError(error, "项目格式或版本不受支持");
        return false;
    }

    const auto units = root.value("units");
    const auto connections = root.value("connections");
    const auto productMerges = root.value("productMerges");
    const auto feedJunctions = root.value("feedJunctions");
    const auto measurements = root.value("measurements");
    const auto annotations = root.value("annotations");
    const auto productNames = root.value("productNames");
    const auto annotationTextStyle = root.value("annotationTextStyle");
    const auto components = root.value("components");
    const auto exportStreamOrder = root.value("exportStreamOrder");
    if (!units.isArray() || !connections.isArray() || !productMerges.isArray()
        || !feedJunctions.isArray() || !measurements.isArray() || !annotations.isArray()) {
        setError(error, "项目缺少必要的数据数组");
        return false;
    }
    if (!productNames.isUndefined() && !productNames.isArray()) {
        setError(error, "项目产品名称数据无效"); return false;
    }
    if (!components.isUndefined()) {
        if (!components.isArray() || components.toArray().isEmpty()) {
            setError(error, "项目组分定义无效"); return false;
        }
        data.components.clear();
        QSet<QString> ids;
        for (const auto& value : components.toArray()) {
            if (!value.isObject()) { setError(error, "项目组分记录无效"); return false; }
            ComponentDefinition component;
            const auto object = value.toObject();
            if (!readRequiredString(object, "id", component.id)
                || !readRequiredString(object, "name", component.name)
                || component.id.size() > 80 || component.name.size() > 40
                || ids.contains(component.id)) {
                setError(error, "项目组分 ID 或名称无效"); return false;
            }
            ids.insert(component.id); data.components.append(std::move(component));
        }
    }
    if (!exportStreamOrder.isUndefined()) {
        if (!exportStreamOrder.isArray()) {
            setError(error, "Excel 导出顺序无效"); return false;
        }
        for (const auto& value : exportStreamOrder.toArray()) {
            if (!value.isString() || value.toString().isEmpty()
                || value.toString().size() > 160
                || data.exportStreamOrder.contains(value.toString())) {
                setError(error, "Excel 导出顺序包含无效或重复物流"); return false;
            }
            data.exportStreamOrder.append(value.toString());
        }
    }
    if (!annotationTextStyle.isUndefined()) {
        if (!annotationTextStyle.isObject()) {
            setError(error, "项目标注文字样式无效"); return false;
        }
        const auto style = annotationTextStyle.toObject();
        const int pointSize = style.value("pointSize").toInt(-1);
        const auto bold = style.value("bold");
        const auto colorValue = style.value("color");
        const auto massUnitValue = style.value("massUnit");
        if (pointSize < 7 || pointSize > 36 || !bold.isBool()
            || (!colorValue.isUndefined() && !colorValue.isString())
            || (!massUnitValue.isUndefined() && !massUnitValue.isString())) {
            setError(error, "项目标注字号、粗体或颜色无效"); return false;
        }
        QColor color;
        if (!colorValue.toString().isEmpty()) {
            color = QColor(colorValue.toString());
            if (!color.isValid()) { setError(error, "项目标注颜色无效"); return false; }
        }
        MassUnit massUnit = MassUnit::Gram;
        QString customMassUnit;
        const QString massUnitKey = massUnitValue.toString("g");
        if (massUnitKey == "kg") massUnit = MassUnit::Kilogram;
        else if (massUnitKey == "t") massUnit = MassUnit::Tonne;
        else if (massUnitKey != "g") {
            customMassUnit = massUnitKey.simplified();
            if (customMassUnit.isEmpty() || customMassUnit.size() > 16) {
                setError(error, "项目自定义质量单位无效"); return false;
            }
            massUnit = MassUnit::Custom;
        }
        data.annotationTextSettings = {
            pointSize, bold.toBool(), color, massUnit, customMassUnit};
    }

    QSet<QString> unitIds;
    for (const auto& value : units.toArray()) {
        if (!value.isObject()) { setError(error, "浮选单元记录无效"); return false; }
        const auto object = value.toObject();
        UnitData item;
        double x = 0, y = 0, width = 0, bodyHeight = 0;
        if (!readRequiredString(object, "id", item.unit.id) || item.unit.id.contains(':')
            || unitIds.contains(item.unit.id)
            || !finiteNumber(object.value("x"), x) || !finiteNumber(object.value("y"), y)
            || !finiteNumber(object.value("width"), width)
            || !finiteNumber(object.value("bodyHeight"), bodyHeight)
            || width <= 0.0 || bodyHeight <= 0.0) {
            setError(error, "浮选单元 ID、位置或尺寸无效");
            return false;
        }
        unitIds.insert(item.unit.id);
        item.unit.position = {x, y};
        item.unit.width = width;
        item.unit.bodyHeight = bodyHeight;
        const auto kind = object.value("kind");
        if (!kind.isUndefined()) {
            const auto parsedKind = kind.isString() ? unitKindFromKey(kind.toString()) : std::nullopt;
            if (!parsedKind) {
                setError(error, "流程单元类型无效"); return false;
            }
            item.unit.kind = *parsedKind;
            if (*parsedKind == UnitKind::BinarySplitter) {
                double split = 0.0;
                if (!finiteNumber(object.value("leftSplitPercent"), split)
                    || split <= 0.0 || split >= 100.0) {
                    setError(error, "二分流器比例无效"); return false;
                }
                item.unit.leftSplitPercent = split;
            }
        }
        const auto terminalLengths = object.value("terminalLengths");
        if (!terminalLengths.isUndefined()) {
            if (!terminalLengths.isObject()) {
                setError(error, "终端产品线长度记录无效"); return false;
            }
            const auto lengths = terminalLengths.toObject();
            const QStringList allowedKeys = item.unit.kind == UnitKind::ThreeProductFlotation
                ? QStringList{"left", "middle", "right"} : QStringList{"left", "right"};
            for (auto it = lengths.constBegin(); it != lengths.constEnd(); ++it) {
                double length = 0.0;
                if (!allowedKeys.contains(it.key()) || !finiteNumber(it.value(), length)
                    || length < 30.0 || length > 5000.0) {
                    setError(error, "终端产品线长度记录无效"); return false;
                }
                item.terminalLengths.insert(it.key(), length);
            }
        }
        data.units.append(std::move(item));
    }
    if (data.units.isEmpty()) { setError(error, "项目中没有浮选单元"); return false; }

    for (const auto& value : connections.toArray()) {
        if (!value.isObject()) { setError(error, "普通连接记录无效"); return false; }
        DirectConnectionData item;
        const auto object = value.toObject();
        if (!readRequiredString(object, "source", item.sourceStreamId)
            || !readRequiredString(object, "targetUnit", item.targetUnitId)) {
            setError(error, "普通连接缺少来源或目标"); return false;
        }
        bool routeValid = true;
        item.routeY = readOptionalNumber(object, "routeY", routeValid);
        if (!routeValid) { setError(error, "普通连接的手动路线无效"); return false; }
        data.connections.append(std::move(item));
    }
    for (const auto& value : productMerges.toArray()) {
        if (!value.isObject()) { setError(error, "产品合流记录无效"); return false; }
        ProductMergeData item;
        const auto object = value.toObject();
        if (!readRequiredString(object, "id", item.id)) {
            setError(error, "产品合流记录缺少有效端点"); return false;
        }
        const auto branches = object.value("branches");
        if (branches.isArray()) {
            QSet<QString> ids;
            for (const auto& branch : branches.toArray()) {
                if (!branch.isString() || branch.toString().isEmpty()
                    || ids.contains(branch.toString())) {
                    setError(error, "产品合流支路无效或重复"); return false;
                }
                ids.insert(branch.toString()); item.streamIds.append(branch.toString());
            }
        } else {
            QString first, second;
            if (!readRequiredString(object, "first", first)
                || !readRequiredString(object, "second", second) || first == second) {
                setError(error, "产品合流记录缺少有效端点"); return false;
            }
            item.streamIds = {first, second};
        }
        if (item.streamIds.size() < 2) {
            setError(error, "产品合流至少需要两条支路"); return false;
        }
        const auto target = object.value("targetUnit");
        if (!target.isUndefined() && !target.isNull()) {
            if (!target.isString() || target.toString().isEmpty()) {
                setError(error, "产品合流的下游单元无效"); return false;
            }
            item.targetUnitId = target.toString();
        }
        bool routeValid = true;
        item.mergeY = readOptionalNumber(object, "mergeY", routeValid);
        if (!routeValid) { setError(error, "产品合流的手动路线无效"); return false; }
        data.productMerges.append(std::move(item));
    }
    for (const auto& value : feedJunctions.toArray()) {
        if (!value.isObject()) { setError(error, "入料回流记录无效"); return false; }
        FeedJunctionData item;
        const auto object = value.toObject();
        if (!readRequiredString(object, "id", item.id)
            || !readRequiredString(object, "targetUnit", item.targetUnitId)) {
            setError(error, "入料回流记录缺少有效端点"); return false;
        }
        const auto sources = object.value("sources");
        if (sources.isArray()) {
            QSet<QString> endpoints;
            for (const auto& sourceValue : sources.toArray()) {
                if (!sourceValue.isObject()) {
                    setError(error, "入料汇合来源无效"); return false;
                }
                const auto sourceObject = sourceValue.toObject();
                QString type;
                QString source;
                if (!readRequiredString(sourceObject, "type", type)
                    || !readRequiredString(sourceObject, "source", source)
                    || (type != "product" && type != "merge")
                    || endpoints.contains(type + ':' + source)) {
                    setError(error, "入料汇合来源无效或重复"); return false;
                }
                endpoints.insert(type + ':' + source);
                item.sourceTypes.append(type);
                item.sourceIds.append(source);
                bool routeValid = true;
                item.routeXs.append(readOptionalNumber(sourceObject, "routeX", routeValid));
                item.routeYs.append(readOptionalNumber(sourceObject, "routeY", routeValid));
                if (!routeValid) { setError(error, "入料汇合支路路线无效"); return false; }
            }
        } else {
            QString type;
            QString source;
            if (!readRequiredString(object, "sourceType", type)
                || !readRequiredString(object, "source", source)
                || (type != "product" && type != "merge")) {
                setError(error, "入料回流记录缺少有效端点"); return false;
            }
            item.sourceTypes.append(type);
            item.sourceIds.append(source);
            item.routeXs.append(std::nullopt);
            item.routeYs.append(std::nullopt);
        }
        if (item.sourceIds.isEmpty()) {
            setError(error, "入料汇合至少需要一个附加来源"); return false;
        }
        const auto processType = object.value("processSourceType");
        const auto processSource = object.value("processSource");
        if (!processType.isUndefined() || !processSource.isUndefined()) {
            if (!processType.isString() || !processSource.isString()
                || processSource.toString().isEmpty()
                || (processType.toString() != "product"
                    && processType.toString() != "merge")) {
                setError(error, "入料汇合的正常上游端点无效"); return false;
            }
            item.processSourceType = processType.toString();
            item.processSourceId = processSource.toString();
        }
        data.feedJunctions.append(std::move(item));
    }

    for (const auto& value : measurements.toArray()) {
        if (!value.isObject()) { setError(error, "实测数据记录无效"); return false; }
        const auto object = value.toObject();
        QString streamId;
        bool valid = true;
        if (!readRequiredString(object, "streamId", streamId) || data.measurements.contains(streamId)) {
            setError(error, "实测数据物流 ID 无效或重复"); return false;
        }
        StreamMeasurement item;
        item.dryMass = readOptionalNumber(object, "dryMass", valid);
        item.gradePercent = readOptionalNumber(object, "gradePercent", valid);
        item.dryMassSharePercent = readOptionalNumber(object, "dryMassSharePercent", valid);
        item.componentSharePercent = readOptionalNumber(object, "componentSharePercent", valid);
        item.dryMassStdDev = readOptionalNumber(object, "dryMassStdDev", valid);
        if (!readPercentMap(object, "gradePercents", item.gradePercents)
            || !readPositiveMap(object, "gradeStdDevs", item.gradeStdDevs)
            || !readPercentMap(object, "componentSharePercents", item.componentSharePercents))
            valid = false;
        if (item.gradePercent && !item.gradePercents.contains(DefaultComponentId))
            item.gradePercents.insert(DefaultComponentId, *item.gradePercent);
        if (item.componentSharePercent
            && !item.componentSharePercents.contains(DefaultComponentId))
            item.componentSharePercents.insert(DefaultComponentId, *item.componentSharePercent);
        if (!valid || (item.dryMass && *item.dryMass < 0.0)
            || (item.dryMassStdDev && *item.dryMassStdDev <= 0.0)
            || (item.gradePercent && (*item.gradePercent < 0.0 || *item.gradePercent > 100.0))
            || (item.dryMassSharePercent && (*item.dryMassSharePercent < 0.0
                                             || *item.dryMassSharePercent > 100.0))
            || (item.componentSharePercent && (*item.componentSharePercent < 0.0
                                                || *item.componentSharePercent > 100.0))) {
            setError(error, QString("物流 %1 的实测数据无效").arg(streamId)); return false;
        }
        data.measurements.insert(streamId, item);
    }

    for (const auto& value : productNames.toArray()) {
        if (!value.isObject()) { setError(error, "产品名称记录无效"); return false; }
        const auto object = value.toObject();
        QString streamId;
        QString name;
        if (!readRequiredString(object, "streamId", streamId)
            || !readRequiredString(object, "name", name)
            || name.size() > 120 || data.productNames.contains(streamId)) {
            setError(error, "产品名称记录缺少物流、名称过长或存在重复"); return false;
        }
        data.productNames.insert(streamId, name);
    }

    for (const auto& value : annotations.toArray()) {
        if (!value.isObject()) { setError(error, "标注记录无效"); return false; }
        const auto object = value.toObject();
        AnnotationRecord item;
        double offsetX = 0, offsetY = 0;
        const int kind = object.value("kind").toInt(-1);
        const int style = object.value("style").toInt(-1);
        const int ownerKind = object.value("ownerKind").toInt(-1);
        if (!readRequiredString(object, "id", item.id) || data.annotations.contains(item.id)
            || !readRequiredString(object, "ownerId", item.ownerId)
            || kind < 0 || kind > static_cast<int>(AnnotationKind::UserNote)
            || style < 0 || style > static_cast<int>(AnnotationStyle::Warning)
            || ownerKind < 0 || ownerKind > static_cast<int>(AnnotationOwnerKind::Free)
            || !finiteNumber(object.value("offsetX"), offsetX)
            || !finiteNumber(object.value("offsetY"), offsetY)
            || !object.value("manuallyPlaced").isBool() || !object.value("visible").isBool()) {
            setError(error, "标注 ID、类型或位置无效"); return false;
        }
        item.kind = static_cast<AnnotationKind>(kind);
        item.style = static_cast<AnnotationStyle>(style);
        item.ownerKind = static_cast<AnnotationOwnerKind>(ownerKind);
        item.manualOffset = {offsetX, offsetY};
        item.manuallyPlaced = object.value("manuallyPlaced").toBool();
        item.visible = object.value("visible").toBool();
        const auto text = object.value("text");
        const int maximumTextLength = kind == static_cast<int>(AnnotationKind::UserNote) ? 500 : 120;
        if (!text.isUndefined() && (!text.isString()
                                    || text.toString().size() > maximumTextLength)) {
            setError(error, "标注文字无效或过长"); return false;
        }
        item.text = text.toString();
        item.dosage = object.value("dosage").toString();
        item.dosageUnit = object.value("dosageUnit").toString();
        item.note = object.value("note").toString();
        const auto fontFamily = object.value("noteFontFamily");
        const auto pointSizeValue = object.value("notePointSize");
        const auto noteBold = object.value("noteBold");
        const auto borderVisible = object.value("noteBorderVisible");
        if ((!fontFamily.isUndefined() && (!fontFamily.isString()
                                           || fontFamily.toString().size() > 120))
            || (!pointSizeValue.isUndefined()
                && (pointSizeValue.toInt(-1) < 0 || pointSizeValue.toInt() > 72))
            || (!noteBold.isUndefined() && !noteBold.isBool())
            || (!borderVisible.isUndefined() && !borderVisible.isBool())) {
            setError(error, "自定义文字样式无效"); return false;
        }
        item.noteFontFamily = fontFamily.toString();
        item.notePointSize = pointSizeValue.toInt(0);
        item.noteBold = noteBold.toBool(false);
        item.noteBorderVisible = borderVisible.toBool(true);
        data.annotations.insert(item.id, std::move(item));
    }
    const auto scenarios = root.value("scenarios");
    if (!scenarios.isUndefined()) {
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
            if (!calculated.isUndefined() && !calculated.isBool()) {
                setError(error, "试验方案计算状态无效"); return false;
            }
            scenarioIds.insert(scenario.id);
            const QString mode = object.value("calculationMode").toString("strict");
            if (mode != "strict" && mode != "reconciliation") {
                setError(error, "试验方案计算模式无效"); return false;
            }
            scenario.calculationMode = mode == "reconciliation"
                ? CalculationMode::DataReconciliation : CalculationMode::Strict;
            if (calculated.toBool(false)) data.calculatedScenarioIds.insert(scenario.id);
            for (const auto& measurementValue : object.value("measurements").toArray()) {
                if (!measurementValue.isObject()) { setError(error, "方案实测数据无效"); return false; }
                const auto measurementObject = measurementValue.toObject();
                QString streamId; bool valid = true;
                if (!readRequiredString(measurementObject, "streamId", streamId)
                    || scenario.measurements.contains(streamId)) {
                    setError(error, "方案实测物流 ID 无效或重复"); return false;
                }
                StreamMeasurement measurement;
                measurement.dryMass = readOptionalNumber(measurementObject, "dryMass", valid);
                measurement.gradePercent = readOptionalNumber(measurementObject, "gradePercent", valid);
                measurement.dryMassSharePercent = readOptionalNumber(
                    measurementObject, "dryMassSharePercent", valid);
                measurement.componentSharePercent = readOptionalNumber(
                    measurementObject, "componentSharePercent", valid);
                measurement.dryMassStdDev = readOptionalNumber(
                    measurementObject, "dryMassStdDev", valid);
                if (!readPercentMap(measurementObject, "gradePercents", measurement.gradePercents)
                    || !readPositiveMap(measurementObject, "gradeStdDevs",
                                        measurement.gradeStdDevs)
                    || !readPercentMap(measurementObject, "componentSharePercents",
                                       measurement.componentSharePercents)) valid = false;
                if (measurement.gradePercent
                    && !measurement.gradePercents.contains(DefaultComponentId))
                    measurement.gradePercents.insert(DefaultComponentId, *measurement.gradePercent);
                if (measurement.componentSharePercent
                    && !measurement.componentSharePercents.contains(DefaultComponentId))
                    measurement.componentSharePercents.insert(
                        DefaultComponentId, *measurement.componentSharePercent);
                if (!valid || (measurement.dryMass && *measurement.dryMass < 0)
                    || (measurement.dryMassStdDev && *measurement.dryMassStdDev <= 0)
                    || (measurement.gradePercent && (*measurement.gradePercent < 0
                                                     || *measurement.gradePercent > 100))
                    || (measurement.dryMassSharePercent
                        && (*measurement.dryMassSharePercent < 0
                            || *measurement.dryMassSharePercent > 100))
                    || (measurement.componentSharePercent
                        && (*measurement.componentSharePercent < 0
                            || *measurement.componentSharePercent > 100))) {
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
                    || scenario.reagentAnnotations.contains(record.id)) {
                    setError(error, "方案药剂标注字段无效"); return false;
                }
                record.text = reagentObject.value("text").toString();
                record.dosage = reagentObject.value("dosage").toString();
                record.dosageUnit = reagentObject.value("unit").toString();
                record.note = reagentObject.value("note").toString();
                bool offsetValid = true;
                const auto offsetY = readOptionalNumber(reagentObject, "offsetY", offsetValid);
                const auto manuallyPlaced = reagentObject.value("manuallyPlaced");
                if (!offsetValid || (!manuallyPlaced.isUndefined() && !manuallyPlaced.isBool())) {
                    setError(error, "方案药剂标注位置无效"); return false;
                }
                record.manualOffset = QPointF(0.0, offsetY.value_or(0.0));
                record.manuallyPlaced = manuallyPlaced.toBool(false);
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
        data.currentScenarioIndex = std::clamp(root.value("currentScenario").toInt(0),
                                               0, static_cast<int>(data.scenarios.size()) - 1);
    }
    return true;
}

} // namespace afs
