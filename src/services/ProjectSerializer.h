#pragma once

#include <QString>

namespace afs {

class FlowsheetDocument;
class FlowsheetScene;

class ProjectSerializer final {
public:
    ProjectSerializer() = delete;

    static bool save(const FlowsheetScene& scene, const FlowsheetDocument& document,
                     const QString& filePath, QString* errorMessage = nullptr);
    static bool load(FlowsheetScene& scene, FlowsheetDocument& document,
                     const QString& filePath, QString* errorMessage = nullptr);
};

} // namespace afs
