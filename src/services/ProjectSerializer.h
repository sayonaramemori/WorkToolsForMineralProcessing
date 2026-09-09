#pragma once

#include <QByteArray>
#include <QString>

namespace afs {

class FlowsheetDocument;
class FlowsheetScene;

class ProjectSerializer final {
public:
    ProjectSerializer() = delete;

    static bool save(const FlowsheetScene& scene, const FlowsheetDocument& document,
                     const QString& filePath, QString* errorMessage = nullptr);
    [[nodiscard]] static QByteArray serialize(
        const FlowsheetScene& scene, const FlowsheetDocument& document);
    static bool load(FlowsheetScene& scene, FlowsheetDocument& document,
                     const QString& filePath, QString* errorMessage = nullptr);
    static bool deserialize(FlowsheetScene& scene, FlowsheetDocument& document,
                            const QByteArray& contents, QString* errorMessage = nullptr);
};

} // namespace afs
