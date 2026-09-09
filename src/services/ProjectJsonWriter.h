#pragma once

#include <QByteArray>

namespace afs {
class FlowsheetDocument;
class FlowsheetScene;

class ProjectJsonWriter final {
public:
    ProjectJsonWriter() = delete;
    [[nodiscard]] static QByteArray serialize(
        const FlowsheetScene& scene, const FlowsheetDocument& document);
};
} // namespace afs
