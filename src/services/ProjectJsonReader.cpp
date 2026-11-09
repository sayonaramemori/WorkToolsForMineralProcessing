#include "services/ProjectJsonReader.h"
#include "services/ProjectJsonReadSupport.h"
#include "services/ProjectFormat.h"

#include <QJsonDocument>
#include <QJsonArray>

namespace afs {
namespace {

// Normalize the only supported older format at the read boundary. The document
// and the remaining readers always use scenario-local notes.
bool upgradeVersion23(QJsonObject& root, QString* error) {
    using project_serialization::json_read::setError;
    QJsonArray sharedAnnotations;
    QJsonArray notes;
    for (const auto& value : root.value("annotations").toArray()) {
        if (!value.isObject()) { setError(error, "标注记录无效"); return false; }
        if (value.toObject().value("kind") == QJsonValue(static_cast<int>(AnnotationKind::UserNote)))
            notes.append(value);
        else sharedAnnotations.append(value);
    }
    if (!root.value("scenarios").isArray()) {
        setError(error, "试验方案数据无效"); return false;
    }
    QJsonArray scenarios;
    for (const auto& value : root.value("scenarios").toArray()) {
        if (!value.isObject()) { setError(error, "试验方案记录无效"); return false; }
        auto scenario = value.toObject();
        if (scenario.contains("notes")) {
            setError(error, "v23 方案不能包含 v24 的独立文字字段"); return false;
        }
        scenario.insert("notes", notes);
        scenarios.append(scenario);
    }
    root.insert("annotations", sharedAnnotations);
    root.insert("scenarios", scenarios);
    root.insert("version", project_serialization::FormatVersion);
    return true;
}

} // namespace

bool ProjectJsonReader::parse(const QByteArray& contents,
                             project_serialization::ProjectData& data, QString* error) {
    using namespace project_serialization;
    using namespace project_serialization::json_read;
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(contents, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(error, QString("项目 JSON 无效：%1").arg(parseError.errorString()));
        return false;
    }
    auto root = document.object();
    if (root.value("format").toString() != "AutoFlotationSheet"
        || (root.value("version") != QJsonValue(FormatVersion)
            && root.value("version") != QJsonValue(LegacyFormatVersion))) {
        setError(error, QString("项目格式或版本不受支持，仅支持 v%1 和 v%2")
            .arg(LegacyFormatVersion).arg(FormatVersion));
        return false;
    }

    const auto units = root.value("units");
    const auto connections = root.value("connections");
    const auto productMerges = root.value("productMerges");
    const auto feedJunctions = root.value("feedJunctions");
    const auto annotations = root.value("annotations");
    const auto productNames = root.value("productNames");
    if (!units.isArray() || !connections.isArray() || !productMerges.isArray()
        || !feedJunctions.isArray() || !annotations.isArray()) {
        setError(error, "项目缺少必要的数据数组");
        return false;
    }
    if (!productNames.isArray()) {
        setError(error, "项目产品名称数据无效"); return false;
    }
    if (root.value("version") == QJsonValue(LegacyFormatVersion)
        && !upgradeVersion23(root, error)) return false;
    return readSettings(root, data, error)
        && readGeometry(root, data, error)
        && readSharedAnnotations(root, data, error)
        && readScenarios(root, data, error);
}

} // namespace afs
