#include "services/ProjectJsonReadSupport.h"
#include <QJsonArray>
#include <QSet>

namespace afs::project_serialization::json_read {

bool readSettings(const QJsonObject& root, ProjectData& data, QString* error) {
    const auto components = root.value("components");
    const auto exportStreamOrder = root.value("exportStreamOrder");
    const auto annotationTextStyle = root.value("annotationTextStyle");

    {
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
    {
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
    {
        if (!annotationTextStyle.isObject()) {
            setError(error, "项目标注文字样式无效"); return false;
        }
        const auto style = annotationTextStyle.toObject();
        const int pointSize = style.value("pointSize").toInt(-1);
        const auto bold = style.value("bold");
        const auto colorValue = style.value("color");
        const auto massUnitValue = style.value("massUnit");
        const auto visibleValue = style.value("resultAnnotationsVisible");
        const auto dryMassValue = style.value("showDryMass");
        const auto gradeValue = style.value("showGrade");
        const auto yieldValue = style.value("showOverallYield");
        const auto recoveryValue = style.value("showOverallRecovery");
        const auto productNameValue = style.value("showProductName");
        const auto labelModeValue = style.value("metricLabelMode");
        if (pointSize < 7 || pointSize > 36 || !bold.isBool()
            || !colorValue.isString() || !massUnitValue.isString()
            || !visibleValue.isBool() || !dryMassValue.isBool() || !gradeValue.isBool()
            || !yieldValue.isBool() || !recoveryValue.isBool() || !productNameValue.isBool()
            || !labelModeValue.isString()) {
            setError(error, "项目标注字号、粗体或颜色无效"); return false;
        }
        QColor color;
        if (!colorValue.toString().isEmpty()) {
            color = QColor(colorValue.toString());
            if (!color.isValid()) { setError(error, "项目标注颜色无效"); return false; }
        }
        MassUnit massUnit = MassUnit::Gram;
        QString customMassUnit;
        const QString massUnitKey = massUnitValue.toString();
        if (massUnitKey == "kg") massUnit = MassUnit::Kilogram;
        else if (massUnitKey == "t") massUnit = MassUnit::Tonne;
        else if (massUnitKey != "g") {
            customMassUnit = massUnitKey.simplified();
            if (customMassUnit.isEmpty() || customMassUnit.size() > 16) {
                setError(error, "项目自定义质量单位无效"); return false;
            }
            massUnit = MassUnit::Custom;
        }
        const QString labelMode = labelModeValue.toString();
        if (labelMode != "chinese" && labelMode != "symbols") {
            setError(error, "项目指标标签模式无效"); return false;
        }
        data.annotationTextSettings = {pointSize, bold.toBool(), color, massUnit, customMassUnit,
            visibleValue.toBool(), dryMassValue.toBool(), gradeValue.toBool(),
            yieldValue.toBool(), recoveryValue.toBool(), productNameValue.toBool(),
            labelMode == "symbols" ? MetricLabelMode::Symbols : MetricLabelMode::Chinese};
    }
    return true;
}

bool readSharedAnnotations(const QJsonObject& root, ProjectData& data, QString* error) {
    const auto productNames = root.value("productNames");
    const auto annotations = root.value("annotations");

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
            || kind == static_cast<int>(AnnotationKind::Reagent)
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
        if (!text.isString() || text.toString().size() > maximumTextLength
            || !object.value("dosage").isString() || !object.value("dosageUnit").isString()
            || !object.value("note").isString()) {
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
        const auto noteColor = object.value("noteColor");
        if (!fontFamily.isString() || fontFamily.toString().size() > 120
            || pointSizeValue.toInt(-1) < 0 || pointSizeValue.toInt() > 72
            || !noteBold.isBool() || !borderVisible.isBool() || !noteColor.isString()) {
            setError(error, "自定义文字样式无效"); return false;
        }
        QColor parsedNoteColor;
        if (!noteColor.toString().isEmpty()) {
            parsedNoteColor = QColor(noteColor.toString());
            if (!parsedNoteColor.isValid()) {
                setError(error, "自定义文字颜色无效"); return false;
            }
        }
        item.noteFontFamily = fontFamily.toString();
        item.notePointSize = pointSizeValue.toInt();
        item.noteBold = noteBold.toBool();
        item.noteBorderVisible = borderVisible.toBool();
        item.noteColor = parsedNoteColor;
        data.annotations.insert(item.id, std::move(item));
    }
    return true;
}

} // namespace afs::project_serialization::json_read
