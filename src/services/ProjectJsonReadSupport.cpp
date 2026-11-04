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

} // namespace afs::project_serialization::json_read
