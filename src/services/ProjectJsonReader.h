#pragma once

#include "services/ProjectSerializationData.h"

#include <QByteArray>

namespace afs {
class ProjectJsonReader final {
public:
    ProjectJsonReader() = delete;
    static bool parse(const QByteArray& contents,
                      project_serialization::ProjectData& data,
                      QString* errorMessage = nullptr);
};
} // namespace afs
