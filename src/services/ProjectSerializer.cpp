#include "services/ProjectSerializer.h"
#include "adapters/CanvasTopologyBuilder.h"
#include "annotations/AnnotationTypes.h"
#include "document/FlowsheetDocument.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FeedJunctionItem.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"
#include "topology/OpenCircuitCalculator.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QSignalBlocker>
#include <algorithm>
#include <cmath>

namespace afs {
namespace {

constexpr int kFormatVersion = 8;
constexpr qint64 kMaximumProjectBytes = 64 * 1024 * 1024;

struct UnitData {
    FlotationUnit unit;
    QHash<QString, double> terminalLengths;
};
struct DirectConnectionData {
    QString sourceStreamId;
    QString targetUnitId;
    std::optional<double> routeY;
};
struct ProductMergeData {
    QString id;
    QVector<QString> streamIds;
    QString targetUnitId;
    std::optional<double> mergeY;
};
struct FeedJunctionData {
    QString id;
    QVector<QString> sourceTypes;
    QVector<QString> sourceIds;
    QVector<std::optional<double>> routeXs;
    QVector<std::optional<double>> routeYs;
    QString targetUnitId;
    QString processSourceType;
    QString processSourceId;
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

void setError(QString* destination, const QString& message) {
    if (destination) *destination = message;
}

QString unitKindKey(UnitKind kind) {
    switch (kind) {
    case UnitKind::Flotation: return "flotation";
    case UnitKind::BinarySplitter: return "binary-splitter";
    case UnitKind::ThreeProductFlotation: return "three-product-flotation";
    }
    return {};
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

bool parseProject(const QByteArray& contents, ProjectData& data, QString* error) {
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
        if (pointSize < 7 || pointSize > 36 || !bold.isBool()
            || (!colorValue.isUndefined() && !colorValue.isString())) {
            setError(error, "项目标注字号、粗体或颜色无效"); return false;
        }
        QColor color;
        if (!colorValue.toString().isEmpty()) {
            color = QColor(colorValue.toString());
            if (!color.isValid()) { setError(error, "项目标注颜色无效"); return false; }
        }
        data.annotationTextSettings = {pointSize, bold.toBool(), color};
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
        if (!readPercentMap(object, "gradePercents", item.gradePercents)
            || !readPercentMap(object, "componentSharePercents", item.componentSharePercents))
            valid = false;
        if (item.gradePercent && !item.gradePercents.contains(DefaultComponentId))
            item.gradePercents.insert(DefaultComponentId, *item.gradePercent);
        if (item.componentSharePercent
            && !item.componentSharePercents.contains(DefaultComponentId))
            item.componentSharePercents.insert(DefaultComponentId, *item.componentSharePercent);
        if (!valid || (item.dryMass && *item.dryMass < 0.0)
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
                if (!readPercentMap(measurementObject, "gradePercents", measurement.gradePercents)
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

ProductLineItem* findProduct(const QHash<QString, ProductLineItem*>& products, const QString& id) {
    return products.value(id, nullptr);
}

bool applyProject(const ProjectData& data, FlowsheetScene& scene, QString* error) {
    const QSignalBlocker blocker(&scene);
    scene.clear();
    QHash<QString, FlotationUnitItem*> units;
    QHash<QString, ProductLineItem*> products;
    QHash<QString, MergeJunctionItem*> merges;
    for (const auto& item : data.units) {
        auto* unit = new FlotationUnitItem(item.unit);
        scene.addItem(unit);
        units.insert(item.unit.id, unit);
        for (auto* product : unit->products()) {
            products.insert(product->streamId(), product);
            const QString key = productSideSuffix(product->side()).mid(1);
            if (item.terminalLengths.contains(key))
                product->setTerminalLength(item.terminalLengths.value(key));
        }
    }
    for (const auto& item : data.connections) {
        auto* source = findProduct(products, item.sourceStreamId);
        auto* target = units.value(item.targetUnitId, nullptr);
        if (!source || !target || !scene.connectProductDirect(source, target->inputLine())) {
            setError(error, QString("无法重建普通连接：%1").arg(item.sourceStreamId)); return false;
        }
        source->setManualRouteY(item.routeY);
    }
    for (const auto& item : data.productMerges) {
        auto* first = findProduct(products, item.streamIds[0]);
        auto* second = findProduct(products, item.streamIds[1]);
        if (merges.contains(item.id) || units.contains(item.id) || !first || !second) {
            setError(error, QString("无法重建产品合流：%1").arg(item.id)); return false;
        }
        auto* merge = scene.mergeProducts(first, second, item.id);
        if (!merge) { setError(error, QString("产品合流状态冲突：%1").arg(item.id)); return false; }
        merges.insert(item.id, merge);
        merge->setManualMergeY(item.mergeY);
        for (int index = 2; index < item.streamIds.size(); ++index) {
            if (!scene.addProductToMerge(findProduct(products, item.streamIds[index]), merge)) {
                setError(error, QString("无法重建产品合流支路：%1").arg(item.streamIds[index]));
                return false;
            }
        }
        if (!item.targetUnitId.isEmpty()) {
            auto* target = units.value(item.targetUnitId, nullptr);
            if (!target || !scene.connectMergeDirect(merge, target->inputLine())) {
                setError(error, QString("无法重建合流产品下游连接：%1").arg(item.id));
                return false;
            }
        }
    }
    QSet<QString> feedIds;
    for (const auto& item : data.feedJunctions) {
        auto* target = units.value(item.targetUnitId, nullptr);
        if (!target || units.contains(item.id) || merges.contains(item.id) || feedIds.contains(item.id)) {
            setError(error, QString("无法重建入料回流：%1").arg(item.id)); return false;
        }
        FeedJunctionItem* junction = nullptr;
        if (item.processSourceType == "product") {
            if (!scene.connectProductDirect(
                    findProduct(products, item.processSourceId), target->inputLine())) {
                setError(error, QString("无法恢复入料汇合的上游产品：%1")
                                    .arg(item.processSourceId)); return false;
            }
        } else if (item.processSourceType == "merge") {
            if (!scene.connectMergeDirect(
                    merges.value(item.processSourceId, nullptr), target->inputLine())) {
                setError(error, QString("无法恢复入料汇合的上游合流：%1")
                                    .arg(item.processSourceId)); return false;
            }
        }
        for (int index = 0; index < item.sourceIds.size(); ++index) {
            const QString connectionId = index == 0 ? item.id : QString{};
            if (item.sourceTypes[index] == "product") {
                junction = scene.connectRecycle(
                    findProduct(products, item.sourceIds[index]), target->inputLine(), connectionId);
            } else {
                junction = scene.connectMergedProduct(
                    merges.value(item.sourceIds[index], nullptr), target->inputLine(), connectionId);
            }
            if (!junction) {
                setError(error, QString("入料汇合来源状态冲突：%1").arg(item.sourceIds[index]));
                return false;
            }
            if (index < item.routeXs.size() && item.routeXs[index]) {
                const QString routeId = item.sourceTypes[index] == "product"
                    ? item.sourceIds[index] : item.sourceIds[index] + ":output";
                junction->setManualRouteX(routeId, item.routeXs[index]);
            }
            if (index < item.routeYs.size() && item.routeYs[index]) {
                const QString routeId = item.sourceTypes[index] == "product"
                    ? item.sourceIds[index] : item.sourceIds[index] + ":output";
                junction->setManualRouteY(routeId, item.routeYs[index]);
            }
        }
        feedIds.insert(item.id);
    }
    scene.resetGeneratedIds();
    scene.refreshConnections();
    return true;
}

QJsonObject measurementObject(const QString& streamId, const StreamMeasurement& value) {
    QJsonObject object{{"streamId", streamId}};
    object.insert("dryMass", value.dryMass ? QJsonValue(*value.dryMass) : QJsonValue::Null);
    object.insert("gradePercent", value.gradePercent ? QJsonValue(*value.gradePercent) : QJsonValue::Null);
    object.insert("dryMassSharePercent", value.dryMassSharePercent
        ? QJsonValue(*value.dryMassSharePercent) : QJsonValue::Null);
    object.insert("componentSharePercent", value.componentSharePercent
        ? QJsonValue(*value.componentSharePercent) : QJsonValue::Null);
    QJsonObject grades;
    for (auto it = value.gradePercents.cbegin(); it != value.gradePercents.cend(); ++it)
        grades.insert(it.key(), it.value());
    object.insert("gradePercents", grades);
    QJsonObject shares;
    for (auto it = value.componentSharePercents.cbegin();
         it != value.componentSharePercents.cend(); ++it) shares.insert(it.key(), it.value());
    object.insert("componentSharePercents", shares);
    return object;
}

} // namespace

bool ProjectSerializer::save(const FlowsheetScene& scene, const FlowsheetDocument& document,
                             const QString& filePath, QString* errorMessage) {
    QVector<FlotationUnitItem*> units;
    QVector<MergeJunctionItem*> merges;
    QVector<FeedJunctionItem*> feeds;
    for (auto* item : scene.items()) {
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item)) units.append(unit);
        else if (auto* merge = dynamic_cast<MergeJunctionItem*>(item)) merges.append(merge);
        else if (auto* feed = dynamic_cast<FeedJunctionItem*>(item)) feeds.append(feed);
    }
    std::sort(units.begin(), units.end(), [](auto* a, auto* b) { return a->unit().id < b->unit().id; });
    std::sort(merges.begin(), merges.end(), [](auto* a, auto* b) { return a->id() < b->id(); });
    std::sort(feeds.begin(), feeds.end(), [](auto* a, auto* b) { return a->id() < b->id(); });

    QJsonArray unitArray;
    QJsonArray connectionArray;
    for (auto* unit : units) {
        const auto& value = unit->unit();
        QJsonObject unitObject{{"id", value.id}, {"x", value.position.x()},
            {"y", value.position.y()}, {"width", value.width}, {"bodyHeight", value.bodyHeight},
            {"kind", unitKindKey(value.kind)}};
        if (value.kind == UnitKind::BinarySplitter)
            unitObject.insert("leftSplitPercent", value.leftSplitPercent);
        QJsonObject terminalLengths;
        for (auto* product : unit->products())
            if (product->terminalLengthOverride())
                terminalLengths.insert(productSideSuffix(product->side()).mid(1),
                                       *product->terminalLengthOverride());
        if (!terminalLengths.isEmpty()) unitObject.insert("terminalLengths", terminalLengths);
        unitArray.append(unitObject);
        for (auto* product : unit->products()) {
            if (product->targetUnit()) {
                QJsonObject connection{{"source", product->streamId()},
                                       {"targetUnit", product->targetUnit()->unit().id}};
                if (product->manualRouteY()) connection.insert("routeY", *product->manualRouteY());
                connectionArray.append(connection);
            }
        }
    }
    QJsonArray mergeArray;
    for (auto* merge : merges) {
        QJsonArray branches;
        for (auto* product : merge->products()) branches.append(product->streamId());
        QJsonObject object{{"id", merge->id()}, {"branches", branches}};
        if (merge->targetUnit()) object.insert("targetUnit", merge->targetUnit()->unit().id);
        if (merge->manualMergeY()) object.insert("mergeY", *merge->manualMergeY());
        mergeArray.append(object);
    }
    QJsonArray feedArray;
    for (auto* feed : feeds) {
        QJsonArray sources;
        for (auto* product : feed->recycleProducts()) {
            QJsonObject source{{"type", "product"}, {"source", product->streamId()}};
            if (feed->manualRouteXs().contains(product->streamId()))
                source.insert("routeX", feed->manualRouteXs().value(product->streamId()));
            if (feed->manualRouteYs().contains(product->streamId()))
                source.insert("routeY", feed->manualRouteYs().value(product->streamId()));
            sources.append(source);
        }
        for (auto* merge : feed->recycleMerges()) {
            QJsonObject source{{"type", "merge"}, {"source", merge->id()}};
            if (feed->manualRouteXs().contains(merge->outputStreamId()))
                source.insert("routeX", feed->manualRouteXs().value(merge->outputStreamId()));
            if (feed->manualRouteYs().contains(merge->outputStreamId()))
                source.insert("routeY", feed->manualRouteYs().value(merge->outputStreamId()));
            sources.append(source);
        }
        QJsonObject object{{"id", feed->id()}, {"sources", sources},
                           {"targetUnit", feed->targetUnit()->unit().id}};
        if (feed->processProduct()) {
            object.insert("processSourceType", "product");
            object.insert("processSource", feed->processProduct()->streamId());
        } else if (feed->processMerge()) {
            object.insert("processSourceType", "merge");
            object.insert("processSource", feed->processMerge()->id());
        }
        feedArray.append(object);
    }
    QJsonArray measurementArray;
    QStringList measurementIds = document.measurements().keys();
    std::sort(measurementIds.begin(), measurementIds.end());
    for (const auto& id : measurementIds)
        measurementArray.append(measurementObject(id, document.measurements().value(id)));
    QJsonArray annotationArray;
    QStringList annotationIds = document.annotationRecords().keys();
    std::sort(annotationIds.begin(), annotationIds.end());
    for (const auto& id : annotationIds) {
        const auto value = document.annotationRecords().value(id);
        if (value.kind == AnnotationKind::Reagent) continue;
        annotationArray.append(QJsonObject{{"id", value.id},
            {"kind", static_cast<int>(value.kind)}, {"style", static_cast<int>(value.style)},
            {"ownerKind", static_cast<int>(value.ownerKind)}, {"ownerId", value.ownerId},
            {"offsetX", value.manualOffset.x()}, {"offsetY", value.manualOffset.y()},
            {"manuallyPlaced", value.manuallyPlaced}, {"visible", value.visible},
            {"text", value.text}, {"dosage", value.dosage},
            {"dosageUnit", value.dosageUnit}, {"note", value.note},
            {"noteFontFamily", value.noteFontFamily},
            {"notePointSize", value.notePointSize}, {"noteBold", value.noteBold},
            {"noteBorderVisible", value.noteBorderVisible}});
    }
    QJsonArray scenarioArray;
    for (const auto& scenario : document.scenarios()) {
        QJsonArray scenarioMeasurements;
        QStringList ids = scenario.measurements.keys(); std::sort(ids.begin(), ids.end());
        for (const auto& id : ids)
            scenarioMeasurements.append(measurementObject(id, scenario.measurements.value(id)));
        QJsonArray reagents;
        QStringList reagentIds = scenario.reagentAnnotations.keys();
        std::sort(reagentIds.begin(), reagentIds.end());
        for (const auto& id : reagentIds) {
            const auto& record = scenario.reagentAnnotations[id];
            reagents.append(QJsonObject{{"id", record.id}, {"ownerId", record.ownerId},
                {"text", record.text}, {"dosage", record.dosage},
                {"unit", record.dosageUnit}, {"note", record.note},
                {"offsetY", record.manualOffset.y()},
                {"manuallyPlaced", record.manuallyPlaced}});
        }
        scenarioArray.append(QJsonObject{{"id", scenario.id}, {"name", scenario.name},
            {"calculated", scenario.calculationResult.has_value()},
            {"measurements", scenarioMeasurements}, {"reagents", reagents}});
    }
    QJsonArray productNameArray;
    QStringList namedStreamIds = document.productNames().keys();
    std::sort(namedStreamIds.begin(), namedStreamIds.end());
    for (const auto& id : namedStreamIds)
        productNameArray.append(QJsonObject{{"streamId", id},
                                            {"name", document.productNames().value(id)}});
    const auto& textSettings = document.annotationTextSettings();
    QJsonArray componentArray;
    for (const auto& component : document.components())
        componentArray.append(QJsonObject{{"id", component.id}, {"name", component.name}});
    QJsonArray exportOrderArray;
    for (const auto& streamId : document.exportStreamOrder()) exportOrderArray.append(streamId);
    const QJsonObject annotationTextStyleObject{
        {"pointSize", textSettings.pointSize}, {"bold", textSettings.bold},
        {"color", textSettings.color.isValid() ? textSettings.color.name(QColor::HexArgb)
                                                 : QString()}};
    const QJsonObject root{{"format", "AutoFlotationSheet"}, {"version", kFormatVersion},
        {"units", unitArray}, {"connections", connectionArray},
        {"productMerges", mergeArray}, {"feedJunctions", feedArray},
        {"measurements", measurementArray}, {"productNames", productNameArray},
        {"components", componentArray},
        {"exportStreamOrder", exportOrderArray},
        {"annotationTextStyle", annotationTextStyleObject},
        {"scenarios", scenarioArray}, {"currentScenario", document.currentScenarioIndex()},
        {"annotations", annotationArray}};

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(errorMessage, QString("无法写入项目文件：%1").arg(file.errorString())); return false;
    }
    if (file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0 || !file.commit()) {
        setError(errorMessage, QString("保存项目失败：%1").arg(file.errorString())); return false;
    }
    return true;
}

bool ProjectSerializer::load(FlowsheetScene& scene, FlowsheetDocument& document,
                             const QString& filePath, QString* errorMessage) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(errorMessage, QString("无法读取项目文件：%1").arg(file.errorString())); return false;
    }
    if (file.size() > kMaximumProjectBytes) {
        setError(errorMessage, "项目文件过大"); return false;
    }
    ProjectData data;
    if (!parseProject(file.readAll(), data, errorMessage)) return false;

    // Build once in an isolated scene so malformed cross-references cannot
    // destroy the project currently open in the application.
    FlowsheetScene validationScene;
    QString validationError;
    if (!applyProject(data, validationScene, &validationError)) {
        setError(errorMessage, QString("项目连接关系无效：%1").arg(validationError)); return false;
    }
    if (!applyProject(data, scene, errorMessage)) return false;
    if (!data.scenarios.isEmpty() && !data.calculatedScenarioIds.isEmpty()) {
        const auto snapshot = CanvasTopologyBuilder::build(scene);
        for (auto& scenario : data.scenarios) {
            if (!data.calculatedScenarioIds.contains(scenario.id)) continue;
            topology::CalculationResult combined;
            bool first = true;
            for (const auto& component : data.components) {
                QHash<topology::StreamId, topology::StreamValue> knownValues;
                QHash<topology::StreamId, topology::BranchAllocation> allocations;
                for (const auto& stream : snapshot.productStreams) {
                    if (!stream.mergeBranch) continue;
                    const auto measurement = scenario.measurements.value(stream.streamId);
                    const auto share = measurement.componentShare(component.id);
                    if (measurement.dryMassSharePercent && share)
                        allocations.insert(stream.streamId,
                                           {*measurement.dryMassSharePercent, *share});
                    else if (measurement.dryMassSharePercent || share)
                        allocations.insert(stream.streamId, {-1.0, -1.0});
                }
                for (const auto& stream : snapshot.reportStreams) {
                    const auto measurement = scenario.measurements.value(stream.streamId);
                    const auto grade = measurement.grade(component.id);
                    if (!measurement.dryMass || !grade) continue;
                    if (auto value = topology::StreamValue::fromMassAndGrade(
                            *measurement.dryMass, *grade)) knownValues.insert(stream.streamId, *value);
                }
                auto result = topology::OpenCircuitCalculator::calculate(
                    snapshot.graph, knownValues, allocations);
                if (first) { combined = result; first = false; }
                combined.components.insert(component.id, {result.values,
                    result.flotationPerformance, result.relativeToExternalFeed,
                    result.complete, result.fullySolved});
                combined.complete = combined.complete && result.complete;
                combined.fullySolved = combined.fullySolved && result.fullySolved;
            }
            scenario.calculationResult = std::move(combined);
        }
    }
    document.replaceProjectData(data.measurements, data.annotations, data.productNames,
                                data.annotationTextSettings, data.exportStreamOrder);
    document.setComponents(data.components);
    if (!data.scenarios.isEmpty())
        document.replaceScenarios(std::move(data.scenarios), data.currentScenarioIndex);
    scene.notifyTopologyChanged();
    return true;
}

} // namespace afs
