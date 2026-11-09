#include "services/FlowTemplateStore.h"
#include "services/FlowGroupSnapshotOperations.h"
#include "services/ProjectSerializer.h"
#include "document/FlowsheetDocument.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FlotationUnitItem.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>
namespace afs {
namespace {
void fail(QString* error, const QString& text) { if (error) *error = text; }
bool validName(const QString& name) { return !name.trimmed().isEmpty() && name.trimmed().size() <= 80; }
QJsonObject read(const QString& path, const QString& id, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { fail(error, "无法读取模板：" + file.errorString()); return {}; }
    QJsonParseError parse; const auto document = QJsonDocument::fromJson(file.readAll(), &parse);
    const auto object = document.object();
    if (parse.error != QJsonParseError::NoError || object.value("format") != "AutoFlotationSheetTemplate"
        || object.value("version") != 1 || object.value("id").toString() != id
        || !validName(object.value("name").toString()) || !object.value("project").isObject()) {
        fail(error, "模板内容无效或格式不受支持"); return {};
    }
    return object;
}
bool write(const QString& path, const QJsonObject& object, QString* error) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) { fail(error, file.errorString()); return false; }
    const auto bytes = QJsonDocument(object).toJson();
    if (file.write(bytes) != bytes.size() || !file.commit()) { fail(error, file.errorString()); return false; }
    return true;
}
}
FlowTemplateStore::FlowTemplateStore(QString directory) : m_directory(std::move(directory)) {
    if (m_directory.isEmpty()) m_directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/flow-templates";
}
QString FlowTemplateStore::path(const QString& id) const {
    const QUuid uuid(id);
    if (uuid.isNull() || uuid.toString(QUuid::WithoutBraces) != id) return {};
    return QDir(m_directory).filePath(id + ".aft.json");
}
QVector<FlowTemplateInfo> FlowTemplateStore::list(QString* error) const {
    QVector<FlowTemplateInfo> items; int invalid = 0;
    for (const auto& file : QDir(m_directory).entryList({"*.aft.json"}, QDir::Files)) {
        const auto id = file.chopped(9); const auto object = read(path(id), id, nullptr);
        if (object.isEmpty()) { ++invalid; continue; }
        items.append({id, object.value("name").toString(), static_cast<int>(object.value("project").toObject().value("units").toArray().size())});
    }
    std::sort(items.begin(), items.end(), [](const auto& a, const auto& b) { return a.name.localeAwareCompare(b.name) < 0; });
    if (invalid) fail(error, QString("有 %1 个模板无法读取").arg(invalid));
    return items;
}
QString FlowTemplateStore::save(const QString& name, const flow_group::Snapshot& snapshot, QString* error) const {
    if (!validName(name) || snapshot.units.isEmpty()) { fail(error, "请选中至少一个设备，并输入 1～80 字的模板名称"); return {}; }
    FlowsheetScene scene; FlowsheetDocument document; int counter = 0;
    const QPointF offset = -snapshot.units.front().position;
    const auto pasted = flow_group::paste(snapshot, offset, scene, document, [&] { return QString::number(++counter); });
    if (pasted.units != snapshot.units.size()) { fail(error, "无法创建完整模板"); return {}; }
    const auto project = QJsonDocument::fromJson(ProjectSerializer::serialize(scene, document)).object();
    const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!QDir().mkpath(m_directory)) { fail(error, "无法创建模板库目录"); return {}; }
    if (!write(path(id), {{"format", "AutoFlotationSheetTemplate"}, {"version", 1}, {"id", id},
                          {"name", name.trimmed()}, {"project", project}}, error)) return {};
    return id;
}
std::optional<flow_group::Snapshot> FlowTemplateStore::load(const QString& id, QString* error) const {
    if (path(id).isEmpty()) { fail(error, "模板 ID 无效"); return std::nullopt; }
    const auto object = read(path(id), id, error); if (object.isEmpty()) return std::nullopt;
    FlowsheetScene scene; FlowsheetDocument document;
    if (!ProjectSerializer::deserialize(scene, document, QJsonDocument(object.value("project").toObject()).toJson(), error)) return std::nullopt;
    for (auto* item : scene.items()) if (dynamic_cast<FlotationUnitItem*>(item)) item->setSelected(true);
    flow_group::Snapshot snapshot; const auto copied = flow_group::capture(scene, document, snapshot);
    if (!copied.units) { fail(error, "模板中没有设备"); return std::nullopt; }
    for (const auto& record : document.annotationRecords()) if (record.kind == AnnotationKind::UserNote) snapshot.notes.append(record);
    std::sort(snapshot.notes.begin(), snapshot.notes.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    return snapshot;
}
bool FlowTemplateStore::rename(const QString& id, const QString& name, QString* error) const {
    if (path(id).isEmpty() || !validName(name)) { fail(error, "模板 ID 或名称无效"); return false; }
    auto object = read(path(id), id, error); if (object.isEmpty()) return false;
    object.insert("name", name.trimmed()); return write(path(id), object, error);
}
bool FlowTemplateStore::remove(const QString& id, QString* error) const {
    if (path(id).isEmpty() || !QFile::remove(path(id))) { fail(error, "无法删除模板"); return false; }
    return true;
}
}
