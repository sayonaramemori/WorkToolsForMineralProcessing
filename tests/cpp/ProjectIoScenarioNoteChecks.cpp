#include "ProjectIoChecks.h"
#include "document/FlowsheetDocument.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FlotationUnitItem.h"
#include "services/ProjectSerializer.h"
#include "services/ProjectUndoManager.h"
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace afs::tests {

int checkScenarioNotes() {
    FlowsheetScene scene;
    scene.addItem(new FlotationUnitItem({"note-unit", {0, 0}}));
    FlowsheetDocument document;
    AnnotationRecord original;
    original.id = "scenario-note"; original.kind = AnnotationKind::UserNote;
    original.ownerKind = AnnotationOwnerKind::Free; original.ownerId = original.id;
    original.text = "基准方案\n说明"; original.style = AnnotationStyle::Note;
    original.manualOffset = {20, 40}; original.manuallyPlaced = true;
    original.noteFontFamily = "DejaVu Serif"; original.notePointSize = 18;
    original.noteBold = true; original.noteBorderVisible = false;
    original.noteColor = QColor("#aa335577");
    document.setAnnotationRecord(original);
    const int copy = document.addScenario("复制方案", true);
    document.setCurrentScenario(copy);
    auto changed = document.annotationRecord(original.id);
    if (changed.text != original.text || changed.manualOffset != original.manualOffset
        || changed.noteColor != original.noteColor) return 110;
    changed.text = "复制方案说明"; changed.manualOffset = {90, -30};
    changed.notePointSize = 25; changed.noteBold = false;
    changed.noteBorderVisible = true; changed.visible = false;
    document.setAnnotationRecord(changed);
    const int empty = document.addScenario("空白方案", false);
    document.setCurrentScenario(empty);
    if (document.annotationRecords().contains(original.id)) return 111;
    document.setCurrentScenario(0);
    if (document.annotationRecord(original.id).text != original.text
        || document.annotationRecord(original.id).manualOffset != original.manualOffset) return 112;
    document.setCurrentScenario(copy);
    const auto bytes = ProjectSerializer::serialize(scene, document);
    const auto root = QJsonDocument::fromJson(bytes).object();
    if (!root.value("annotations").toArray().isEmpty()
        || root.value("scenarios").toArray()[copy].toObject().value("notes").toArray().size() != 1)
        return 113;
    FlowsheetScene loaded;
    FlowsheetDocument restored;
    QString error;
    if (!ProjectSerializer::deserialize(loaded, restored, bytes, &error)) {
        qCritical() << error; return 114;
    }
    if (restored.currentScenarioIndex() != copy
        || restored.annotationRecord(original.id).text != changed.text
        || restored.annotationRecord(original.id).visible
        || restored.annotationRecord(original.id).noteBold
        || !restored.annotationRecord(original.id).noteBorderVisible
        || restored.annotationRecord(original.id).manualOffset != changed.manualOffset
        || restored.annotationRecord(original.id).notePointSize != changed.notePointSize) return 115;
    ProjectUndoManager undo(loaded, restored, {}, {});
    undo.initialize();
    restored.removeAnnotationRecord(original.id);
    undo.undo();
    if (restored.annotationRecord(original.id).text != changed.text) return 116;
    undo.redo();
    if (restored.annotationRecords().contains(original.id)) return 117;
    restored.setCurrentScenario(0);
    const auto baseline = restored.annotationRecord(original.id);
    if (baseline.text != original.text || baseline.manualOffset != original.manualOffset
        || baseline.noteFontFamily != original.noteFontFamily
        || baseline.notePointSize != original.notePointSize || !baseline.noteBold
        || baseline.noteBorderVisible || baseline.noteColor != original.noteColor) return 118;
    restored.setCurrentScenario(empty);
    if (!restored.annotationRecords().isEmpty()) return 119;

    // Invalid scenario notes must be rejected before mutating the open project.
    const auto before = ProjectSerializer::serialize(loaded, restored);
    const auto reject = [&](const QJsonObject& invalid) {
        error.clear();
        return !ProjectSerializer::deserialize(loaded, restored,
                   QJsonDocument(invalid).toJson(), &error)
            && !error.isEmpty() && ProjectSerializer::serialize(loaded, restored) == before;
    };
    const auto note = root.value("scenarios").toArray()[0].toObject().value("notes").toArray()[0].toObject();
    const auto withNotes = [&](const QJsonValue& notes) {
        auto invalid = root;
        auto scenarios = invalid.value("scenarios").toArray();
        auto scenario = scenarios[0].toObject(); scenario.insert("notes", notes);
        scenarios[0] = scenario; invalid.insert("scenarios", scenarios); return invalid;
    };
    if (!reject(withNotes(QJsonValue::Null)) || !reject(withNotes(QJsonArray{note, note}))) return 120;
    for (const auto& field : {"text", "notePointSize", "noteColor", "ownerId", "offsetX"}) {
        auto invalid = note; invalid.remove(field);
        if (!reject(withNotes(QJsonArray{invalid}))) return 121;
    }
    auto invalid = note; invalid.insert("kind", static_cast<int>(AnnotationKind::StreamResult));
    if (!reject(withNotes(QJsonArray{invalid}))) return 122;
    invalid = note; invalid.insert("noteColor", "invalid color");
    if (!reject(withNotes(QJsonArray{invalid}))) return 123;
    auto shared = root; shared.insert("annotations", QJsonArray{note});
    if (!reject(shared)) return 124;
    return 0;
}

} // namespace afs::tests
