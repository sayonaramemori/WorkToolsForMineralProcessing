#include "services/ProjectJsonReader.h"
#include "services/ProjectJsonReadSupport.h"
#include "services/ProjectFormat.h"

#include <QJsonDocument>

namespace afs {

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
    const auto root = document.object();
    if (root.value("format").toString() != "AutoFlotationSheet"
        || root.value("version") != QJsonValue(FormatVersion)) {
        setError(error, QString("项目格式或版本不受支持，仅支持 v%1").arg(FormatVersion));
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
    return readSettings(root, data, error)
        && readGeometry(root, data, error)
        && readSharedAnnotations(root, data, error)
        && readScenarios(root, data, error);
}

} // namespace afs
