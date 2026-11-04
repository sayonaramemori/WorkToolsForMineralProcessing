#include "services/ProjectUnitKindCodec.h"
#include "services/ProjectJsonReadSupport.h"
#include <QJsonArray>
#include <QSet>

namespace afs::project_serialization::json_read {

bool readGeometry(const QJsonObject& root, ProjectData& data, QString* error) {
    const auto units = root.value("units");
    const auto connections = root.value("connections");
    const auto productMerges = root.value("productMerges");
    const auto feedJunctions = root.value("feedJunctions");

    QSet<QString> unitIds;
    for (const auto& value : units.toArray()) {
        if (!value.isObject()) { setError(error, "浮选单元记录无效"); return false; }
        const auto object = value.toObject();
        UnitData item;
        double x = 0, y = 0;
        double width = 0.0, bodyHeight = 0.0;
        if (!readRequiredString(object, "id", item.unit.id) || item.unit.id.contains(':')) {
            setError(error, "浮选单元 ID 无效");
            return false;
        }
        if (unitIds.contains(item.unit.id)) {
            setError(error, QString("项目包含重复的浮选单元 ID：“%1”").arg(item.unit.id));
            return false;
        }
        if (!finiteNumber(object.value("x"), x) || !finiteNumber(object.value("y"), y)
            || !finiteNumber(object.value("width"), width) || width <= 0.0
            || !finiteNumber(object.value("bodyHeight"), bodyHeight) || bodyHeight <= 0.0) {
            const QString unitName = item.unit.id.isEmpty() ? QStringLiteral("<未命名>")
                                                              : item.unit.id;
            setError(error, QString("浮选单元“%1”的位置或尺寸无效；"
                                    "请检查 x、y、width、bodyHeight 字段")
                                .arg(unitName));
            return false;
        }
        unitIds.insert(item.unit.id);
        item.unit.position = {x, y};
        item.unit.width = width;
        item.unit.bodyHeight = bodyHeight;
        const auto kind = object.value("kind");
        {
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
        const auto poolLabel = object.value("poolLabel");
        const auto inputLength = object.value("inputLength");
        if (!inputLength.isUndefined()) {
            double length = 0.0;
            if (!finiteNumber(inputLength, length) || length < 30.0 || length > 5000.0) {
                setError(error, "入料线长度无效"); return false;
            }
            item.unit.inputLength = length;
        }
        const auto diameterValue = object.value("ballMillDiameter");
        if (!diameterValue.isUndefined()) {
            double diameter = 0.0;
            if (item.unit.kind != UnitKind::BallMill || !finiteNumber(diameterValue, diameter)
                || diameter < 40.0 || diameter > 720.0) {
                setError(error, "球磨机直径无效"); return false;
            }
            item.unit.ballMillDiameter = diameter;
        }
        if (!poolLabel.isUndefined()) {
            const QString label = poolLabel.isString() ? poolLabel.toString().simplified() : QString{};
            if (!isStoragePool(item.unit.kind) || !poolLabel.isString() || label.isEmpty()
                || label.size() > 48) {
                setError(error, "贮池名称无效"); return false;
            }
            item.unit.poolLabel = label;
        }
        const auto terminalLengths = object.value("terminalLengths");
        if (!terminalLengths.isUndefined()) {
            if (!terminalLengths.isObject()) {
                setError(error, "终端产品线长度记录无效"); return false;
            }
            const auto lengths = terminalLengths.toObject();
            const QStringList allowedKeys = hasMiddleProduct(item.unit.kind)
                ? QStringList{"left", "middle", "right"}
                : isTerminalStoragePool(item.unit.kind) ? QStringList{}
                : isNonSeparatingUnit(item.unit.kind) ? QStringList{"right"}
                : QStringList{"left", "right"};
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
        if (!branches.isArray()) {
            setError(error, "产品合流支路数组无效"); return false;
        }
        QSet<QString> ids;
        for (const auto& branch : branches.toArray()) {
            if (!branch.isString() || branch.toString().isEmpty()
                || ids.contains(branch.toString())) {
                setError(error, "产品合流支路无效或重复"); return false;
            }
            ids.insert(branch.toString()); item.streamIds.append(branch.toString());
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
        if (!sources.isArray()) {
            setError(error, "入料汇合来源数组无效"); return false;
        }
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
        const auto externalFeed = object.value("hasExternalFeed");
        if (!externalFeed.isBool()) {
            setError(error, "入料汇合的外部入料标记无效"); return false;
        }
        item.hasExternalFeed = externalFeed.toBool();
        data.feedJunctions.append(std::move(item));
    }
    return true;
}

} // namespace afs::project_serialization::json_read
