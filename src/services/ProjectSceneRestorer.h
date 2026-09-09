#pragma once

#include "services/ProjectSerializationData.h"

namespace afs {
class FlowsheetScene;

class ProjectSceneRestorer final {
public:
    ProjectSceneRestorer() = delete;
    static bool restore(const project_serialization::ProjectData& data,
                        FlowsheetScene& scene, QString* errorMessage = nullptr);
};
} // namespace afs
