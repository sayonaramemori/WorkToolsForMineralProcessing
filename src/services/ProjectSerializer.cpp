#include "services/ProjectSerializer.h"
#include "adapters/CanvasTopologyBuilder.h"
#include "document/FlowsheetDocument.h"
#include "editor/FlowsheetScene.h"
#include "services/FlowsheetCalculationService.h"
#include "services/ProjectJsonReader.h"
#include "services/ProjectJsonWriter.h"
#include "services/ProjectSceneRestorer.h"
#include "services/ProjectSerializationData.h"

#include <QFile>
#include <QSaveFile>

namespace afs {
namespace {

constexpr qint64 kMaximumProjectBytes = 64 * 1024 * 1024;

void setError(QString* destination, const QString& message) {
    if (destination) *destination = message;
}


} // namespace

QByteArray ProjectSerializer::serialize(const FlowsheetScene& scene,
                                        const FlowsheetDocument& document) {
    return ProjectJsonWriter::serialize(scene, document);
}

bool ProjectSerializer::save(const FlowsheetScene& scene, const FlowsheetDocument& document,
                             const QString& filePath, QString* errorMessage) {
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(errorMessage, QString("无法写入项目文件：%1").arg(file.errorString())); return false;
    }
    if (file.write(serialize(scene, document)) < 0 || !file.commit()) {
        setError(errorMessage, QString("保存项目失败：%1").arg(file.errorString())); return false;
    }
    return true;
}

bool ProjectSerializer::load(FlowsheetScene& scene, FlowsheetDocument& document,
                             const QString& filePath, QString* errorMessage) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(errorMessage, QString("无法读取项目文件：%1").arg(file.errorString())); return false;
    }
    if (file.size() > kMaximumProjectBytes) {
        setError(errorMessage, "项目文件过大"); return false;
    }
    return deserialize(scene, document, file.readAll(), errorMessage);
}

bool ProjectSerializer::deserialize(FlowsheetScene& scene, FlowsheetDocument& document,
                                    const QByteArray& contents, QString* errorMessage) {
    if (contents.size() > kMaximumProjectBytes) {
        setError(errorMessage, "项目快照过大"); return false;
    }
    project_serialization::ProjectData data;
    if (!ProjectJsonReader::parse(contents, data, errorMessage)) return false;

    // Build once in an isolated scene so malformed cross-references cannot
    // destroy the project currently open in the application.
    FlowsheetScene validationScene;
    QString validationError;
    if (!ProjectSceneRestorer::restore(data, validationScene, &validationError)) {
        setError(errorMessage, QString("项目连接关系无效：%1").arg(validationError)); return false;
    }
    if (!ProjectSceneRestorer::restore(data, scene, errorMessage)) return false;
    document.replaceProjectData(data.measurements, data.annotations, data.productNames,
                                data.annotationTextSettings, data.exportStreamOrder);
    document.setComponents(data.components);
    if (!data.scenarios.isEmpty())
        document.replaceScenarios(std::move(data.scenarios), data.currentScenarioIndex);
    if (!data.calculatedScenarioIds.isEmpty()) {
        const int requestedScenario = document.currentScenarioIndex();
        const auto snapshot = CanvasTopologyBuilder::build(scene);
        for (int index = 0; index < document.scenarios().size(); ++index) {
            if (!data.calculatedScenarioIds.contains(document.scenarios()[index].id)) continue;
            document.setCurrentScenario(index);
            document.setCalculationResult(
                FlowsheetCalculationService::calculate(snapshot, document));
        }
        document.setCurrentScenario(requestedScenario);
    }
    scene.notifyTopologyChanged();
    return true;
}

} // namespace afs
