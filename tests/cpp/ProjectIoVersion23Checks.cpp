#include "ProjectIoChecks.h"
#include "document/FlowsheetDocument.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FlotationUnitItem.h"
#include "services/ProjectFormat.h"
#include "services/ProjectSerializer.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace afs::tests {

int checkVersion23Compatibility() {
    // Keep a real v23 project unchanged so the compatibility test does not
    // depend solely on the current writer generating an older-format fixture.
    QDir root(QCoreApplication::applicationDirPath()); root.cdUp(); root.cdUp();
    FlowsheetScene scene;
    FlowsheetDocument document;
    QString error;
    if (!ProjectSerializer::load(scene, document, root.filePath("tests/v23-project.afs.json"), &error)) {
        qCritical() << "v23 fixture load failed:" << error; return 130;
    }
    if (document.scenarios()[document.currentScenarioIndex()].noteAnnotations.size() != 3) return 131;

    // Exercise migration of shared text into multiple independent scenarios,
    // while retaining the shared result annotation and per-scenario reagent.
    AnnotationRecord note;
    note.id = "legacy-note"; note.ownerId = note.id;
    note.kind = AnnotationKind::UserNote; note.ownerKind = AnnotationOwnerKind::Free;
    note.style = AnnotationStyle::Note; note.text = "v23 共享文字\n第二行";
    note.manualOffset = {-40, 80}; note.manuallyPlaced = true;
    note.noteFontFamily = "DejaVu Serif"; note.notePointSize = 19;
    note.noteColor = QColor("#aa225588"); note.noteBold = true; note.noteBorderVisible = false;
    document.setAnnotationRecord(note);
    AnnotationRecord result;
    result.id = "legacy-result"; result.ownerId = "1:left";
    result.kind = AnnotationKind::StreamResult; result.manualOffset = {25, 35};
    document.setAnnotationRecord(result);
    AnnotationRecord reagent;
    reagent.id = "legacy-reagent"; reagent.kind = AnnotationKind::Reagent;
    reagent.ownerId = "1:left"; reagent.text = "捕收剂"; reagent.dosage = "20";
    reagent.dosageUnit = "g/t"; reagent.note = "保留备注";
    reagent.manualOffset = {0, 24}; reagent.manuallyPlaced = true;
    document.setAnnotationRecord(reagent);
    const int copy = document.addScenario("复制方案", true);
    document.setCurrentScenario(copy);
    auto legacy = QJsonDocument::fromJson(ProjectSerializer::serialize(scene, document)).object();
    auto scenarios = legacy.value("scenarios").toArray();
    auto shared = legacy.value("annotations").toArray();
    const auto notes = scenarios[0].toObject().value("notes").toArray();
    for (const auto& value : notes) shared.append(value);
    for (int index = 0; index < scenarios.size(); ++index) {
        auto scenario = scenarios[index].toObject(); scenario.remove("notes"); scenarios[index] = scenario;
    }
    legacy.insert("annotations", shared); legacy.insert("scenarios", scenarios);
    legacy.insert("version", project_serialization::LegacyFormatVersion);
    if (!ProjectSerializer::deserialize(scene, document, QJsonDocument(legacy).toJson(), &error)) {
        qCritical() << "v23 migration failed:" << error; return 132;
    }
    const auto record = document.annotationRecord(note.id);
    if (document.currentScenarioIndex() != copy
        || record.text != note.text || record.manualOffset != note.manualOffset
        || record.noteFontFamily != note.noteFontFamily || record.notePointSize != note.notePointSize
        || record.noteColor != note.noteColor || !record.noteBold || record.noteBorderVisible
        || document.annotationRecord(result.id).manualOffset != result.manualOffset
        || document.annotationRecord(reagent.id).dosage != reagent.dosage) return 133;
    auto changed = record; changed.text = "副本专用"; changed.manualOffset += QPointF(50, 20);
    document.setAnnotationRecord(changed);
    document.setCurrentScenario(0);
    if (document.annotationRecord(note.id).text != note.text
        || document.annotationRecord(note.id).manualOffset != note.manualOffset) return 134;
    document.removeAnnotationRecord(note.id);
    document.setCurrentScenario(copy);
    if (document.annotationRecord(note.id).text != changed.text) return 135;

    // Saving an imported project always emits v24; reopening retains the split.
    const auto saved = ProjectSerializer::serialize(scene, document);
    const auto upgraded = QJsonDocument::fromJson(saved).object();
    if (upgraded.value("version").toInt() != project_serialization::FormatVersion) return 136;
    for (const auto& value : upgraded.value("annotations").toArray())
        if (value.toObject().value("kind").toInt() == static_cast<int>(AnnotationKind::UserNote)) return 137;
    if (!ProjectSerializer::deserialize(scene, document, saved, &error)
        || document.annotationRecord(note.id).text != changed.text) return 138;
    document.setCurrentScenario(0);
    if (document.annotationRecords().contains(note.id)) return 139;

    const auto before = ProjectSerializer::serialize(scene, document);
    const auto reject = [&](const QJsonObject& invalid) {
        error.clear();
        return !ProjectSerializer::deserialize(scene, document, QJsonDocument(invalid).toJson(), &error)
            && !error.isEmpty() && ProjectSerializer::serialize(scene, document) == before;
    };
    auto noNotes = legacy;
    auto onlyResults = QJsonArray{};
    for (const auto& value : shared)
        if (value.toObject().value("kind").toInt() != static_cast<int>(AnnotationKind::UserNote))
            onlyResults.append(value);
    noNotes.insert("annotations", onlyResults);
    FlowsheetScene emptyScene; FlowsheetDocument emptyDocument;
    if (!ProjectSerializer::deserialize(emptyScene, emptyDocument, QJsonDocument(noNotes).toJson(), &error)
        || !emptyDocument.scenarios()[emptyDocument.currentScenarioIndex()].noteAnnotations.isEmpty()) return 140;
    auto invalid = legacy; invalid.insert("version", "23");
    if (!reject(invalid)) return 141;
    invalid = legacy; invalid.insert("version", 23.5);
    if (!reject(invalid)) return 142;
    invalid = legacy;
    auto duplicate = shared; duplicate.append(notes[0]); invalid.insert("annotations", duplicate);
    if (!reject(invalid)) return 143;
    invalid = legacy;
    auto malformed = notes[0].toObject(); malformed.insert("noteColor", "bad-color");
    auto malformedShared = shared;
    for (int index = 0; index < malformedShared.size(); ++index)
        if (malformedShared[index].toObject().value("id") == malformed.value("id"))
            malformedShared[index] = malformed;
    invalid.insert("annotations", malformedShared);
    if (!reject(invalid)) return 144;
    invalid = legacy;
    auto mixedScenarios = scenarios; auto mixed = mixedScenarios[0].toObject();
    mixed.insert("notes", notes); mixedScenarios[0] = mixed; invalid.insert("scenarios", mixedScenarios);
    if (!reject(invalid)) return 145;
    invalid = legacy; invalid.insert("annotations", QJsonArray{42});
    if (!reject(invalid)) return 146;
    return 0;
}

} // namespace afs::tests
