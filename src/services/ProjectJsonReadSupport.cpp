#include "services/ProjectJsonReadSupport.h"
#include <cmath>

namespace afs::project_serialization::json_read {

void setError(QString* destination, const QString& message) {
    if (destination) *destination = message;
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

bool readAnnotationRecord(const QJsonObject& object, AnnotationRecord& item, QString* error) {
    double offsetX = 0, offsetY = 0;
    const int kind = object.value("kind").toInt(-1);
    const int style = object.value("style").toInt(-1);
    const int ownerKind = object.value("ownerKind").toInt(-1);
    if (!readRequiredString(object, "id", item.id)
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
    return true;
}

} // namespace afs::project_serialization::json_read
