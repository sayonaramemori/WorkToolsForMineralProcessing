#pragma once

#include "services/ProjectSerializationData.h"

#include <QJsonObject>
#include <QJsonValue>

namespace afs::project_serialization::json_read {

// Internal parsing helpers. Each section only fills ProjectData and never
// accesses the live scene or document.
void setError(QString* destination, const QString& message);
bool finiteNumber(const QJsonValue& value, double& result);
bool readRequiredString(const QJsonObject& object, const char* key, QString& result);
std::optional<double> readOptionalNumber(const QJsonObject& object, const char* key, bool& valid);

bool readAnnotationRecord(const QJsonObject& object, AnnotationRecord& item, QString* error);

bool readSettings(const QJsonObject& root, ProjectData& data, QString* error);
bool readGeometry(const QJsonObject& root, ProjectData& data, QString* error);
bool readSharedAnnotations(const QJsonObject& root, ProjectData& data, QString* error);
bool readScenarios(const QJsonObject& root, ProjectData& data, QString* error);

} // namespace afs::project_serialization::json_read
