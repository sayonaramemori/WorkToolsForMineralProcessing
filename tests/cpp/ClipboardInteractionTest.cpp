#include "app/MainWindow.h"
#include "annotations/AnnotationItem.h"
#include "annotations/AnnotationManager.h"
#include "document/FlowsheetDocument.h"
#include "editor/CanvasView.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FlotationUnitItem.h"
#include "services/FlowsheetCalculationService.h"
#include "services/ProjectSerializer.h"
#include "services/ProjectUndoManager.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QLineEdit>
#include <QSettings>
#include <QSignalSpy>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <stdexcept>

using namespace afs;

void runReagentClipboardChecks(const QString& directory);
void runCopyMoveInteractionChecks();

namespace {
void require(bool condition, const QString& message) {
    if (!condition) throw std::runtime_error(message.toStdString());
}

int noteCount(const FlowsheetDocument& document) {
    int count = 0;
    for (const auto& record : document.annotationRecords())
        if (record.kind == AnnotationKind::UserNote) ++count;
    return count;
}

AnnotationItem* noteItem(FlowsheetScene& scene, const QString& id) {
    for (auto* item : scene.items())
        if (auto* note = dynamic_cast<AnnotationItem*>(item); note && note->record().id == id)
            return note;
    return nullptr;
}

QString selectedNoteId(const FlowsheetScene& scene) {
    for (auto* item : scene.selectedItems())
        if (auto* note = dynamic_cast<AnnotationItem*>(item);
            note && note->record().kind == AnnotationKind::UserNote) return note->record().id;
    return {};
}

int unitCount(const FlowsheetScene& scene) {
    int count = 0;
    for (auto* item : scene.items()) if (dynamic_cast<FlotationUnitItem*>(item)) ++count;
    return count;
}

void expectCopy(const AnnotationRecord& source, const AnnotationRecord& copy, QPointF offset) {
    require(!copy.id.isEmpty() && copy.id != source.id, "Pasted note must have a fresh ID");
    require(copy.text == source.text && copy.kind == AnnotationKind::UserNote
        && copy.style == source.style && copy.ownerKind == source.ownerKind && copy.ownerId == source.ownerId
        && copy.noteFontFamily == source.noteFontFamily && copy.notePointSize == source.notePointSize
        && copy.noteBold == source.noteBold && copy.noteBorderVisible == source.noteBorderVisible
        && copy.noteColor == source.noteColor && copy.visible == source.visible,
        "Pasted note lost text or appearance");
    require(copy.manuallyPlaced && copy.manualOffset == source.manualOffset + offset,
            "Pasted note has the wrong position");
}

void click(CanvasView& canvas, QGraphicsItem& item, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    canvas.centerOn(&item);
    QApplication::processEvents();
    const QPointF local = dynamic_cast<FlotationUnitItem*>(&item)
        ? QPointF(60, 12) : QPointF(12, 12);
    QTest::mouseClick(canvas.viewport(), Qt::LeftButton, modifiers,
                      canvas.mapFromScene(item.mapToScene(local)));
    QApplication::processEvents();
    require(item.isSelected(), "Canvas did not select the item");
}

void shortcut(CanvasView& canvas, Qt::Key key) {
    canvas.setFocus();
    QTest::keyClick(canvas.viewport(), key, Qt::ControlModifier);
    QApplication::processEvents();
}

void action(MainWindow& window, const QString& text) {
    for (auto* candidate : window.findChildren<QAction*>()) {
        if (candidate->text() != text) continue;
        require(candidate->isEnabled(), "Action is disabled: " + text);
        candidate->trigger(); QApplication::processEvents(); return;
    }
    require(false, "Missing action: " + text);
}

void testClipboard(const QString& directory) {
    MainWindow window;
    window.resize(1600, 1000); window.show(); window.activateWindow();
    QApplication::processEvents();
    auto* canvas = dynamic_cast<CanvasView*>(window.centralWidget());
    auto* scene = qobject_cast<FlowsheetScene*>(canvas->scene());
    auto* document = window.findChild<FlowsheetDocument*>();
    auto* annotations = window.findChild<AnnotationManager*>();
    auto* undo = window.findChild<ProjectUndoManager*>();
    FlotationUnitItem* originalUnit = nullptr;
    for (auto* item : scene->items())
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item)) originalUnit = unit;
    require(originalUnit, "Missing initial unit");
    const QString unitId = originalUnit->unit().id;
    document->setDryMass(unitId + ":left", 30);
    document->setGradePercent(unitId + ":left", 2);
    document->setDryMass(unitId + ":right", 70);
    document->setGradePercent(unitId + ":right", 1);
    document->setCalculationResult(FlowsheetCalculationService::calculate(*scene, *document));
    require(document->calculationResult()->fullySolved, "Fixture calculation failed");

    AnnotationRecord original{"note-1", AnnotationKind::UserNote, AnnotationStyle::Note,
        AnnotationOwnerKind::Free, "canvas", {-350, -300}, true, true};
    original.text = "粗选流程\n文字复制测试";
    original.noteFontFamily = "DejaVu Serif";
    original.notePointSize = 20; original.noteBold = true;
    original.noteBorderVisible = false; original.noteColor = QColor("#884466");
    auto plain = original;
    plain.id = "note-3"; plain.text = "普通文字"; plain.manualOffset = {200, -300};
    plain.noteFontFamily.clear(); plain.notePointSize = 0;
    plain.noteBold = false; plain.noteBorderVisible = true; plain.noteColor = {};
    document->setAnnotationRecord(original); document->setAnnotationRecord(plain);
    annotations->synchronize(); undo->initialize();

    qInfo() << "Text-only shortcuts, repeated paste and immutable clipboard contents";
    QSignalSpy topology(scene, &FlowsheetScene::topologyChanged);
    QSignalSpy calculations(document, &FlowsheetDocument::calculationChanged);
    click(*canvas, *noteItem(*scene, original.id));
    shortcut(*canvas, Qt::Key_C);
    require(window.statusBar()->currentMessage().contains("已复制"), "Copy shortcut did not run");
    shortcut(*canvas, Qt::Key_V);
    const QString firstId = selectedNoteId(*scene);
    require(noteCount(*document) == 3 && unitCount(*scene) == 1, "Text-only paste copied the wrong objects");
    expectCopy(original, document->annotationRecord(firstId), {40, 40});
    require(noteItem(*scene, firstId) && noteItem(*scene, firstId)->isSelected(), "Pasted note was not rendered and selected");
    require(document->annotationRecord(plain.id).text == plain.text, "Pasted ID overwrote another note");
    undo->markClean();

    auto edited = original; edited.text = "源文字已修改";
    document->setAnnotationRecord(edited); annotations->synchronize();
    action(window, "粘贴对象 (Ctrl+V)");
    const QString secondId = selectedNoteId(*scene);
    require(noteCount(*document) == 4 && secondId != firstId, "Repeated paste reused a note ID");
    expectCopy(original, document->annotationRecord(secondId), {80, 80});
    require(topology.isEmpty() && calculations.isEmpty() && document->calculationResult()->fullySolved,
            "Text-only paste invalidated material-balance results");

    qInfo() << "Undo/redo and project round trip retain pasted note records";
    action(window, "撤销");
    require(noteCount(*document) == 3 && !noteItem(*scene, secondId), "Undo did not remove pasted note");
    action(window, "重做");
    require(noteCount(*document) == 4 && noteItem(*scene, secondId), "Redo did not restore pasted note");
    expectCopy(original, document->annotationRecord(secondId), {80, 80});

    qInfo() << "Calculated result cards are not copied as custom text";
    AnnotationItem* resultCard = nullptr;
    for (auto* item : scene->items())
        if (auto* annotation = dynamic_cast<AnnotationItem*>(item);
            annotation && annotation->record().kind == AnnotationKind::StreamResult && annotation->isVisible())
            resultCard = annotation;
    require(resultCard, "Missing calculated result card");
    click(*canvas, *resultCard);
    shortcut(*canvas, Qt::Key_C); shortcut(*canvas, Qt::Key_V);
    require(noteCount(*document) == 4 && unitCount(*scene) == 1,
            "A result card was duplicated as free text");

    qInfo() << "Mixed unit/text selection and ordinary unit-only copy";
    originalUnit = nullptr;
    for (auto* item : scene->items())
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item); unit && unit->unit().id == unitId)
            originalUnit = unit;
    require(originalUnit, "Undo lost the source unit");
    const QPointF sourceUnitPosition = originalUnit->pos();
    click(*canvas, *noteItem(*scene, plain.id));
    click(*canvas, *originalUnit, Qt::ControlModifier);
    action(window, "复制选中对象 (Ctrl+C)");
    action(window, "粘贴对象 (Ctrl+V)");
    const QString mixedId = selectedNoteId(*scene);
    require(noteCount(*document) == 5 && unitCount(*scene) == 2 && scene->selectedItems().size() == 2,
            "Mixed selection did not paste both objects");
    expectCopy(plain, document->annotationRecord(mixedId), {40, 40});
    FlotationUnitItem* clonedUnit = nullptr;
    for (auto* item : scene->selectedItems())
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item)) clonedUnit = unit;
    require(clonedUnit && clonedUnit->pos() == sourceUnitPosition + QPointF(40, 40),
            "Text and unit did not use the same paste offset");
    click(*canvas, *originalUnit);
    shortcut(*canvas, Qt::Key_C); shortcut(*canvas, Qt::Key_V);
    require(unitCount(*scene) == 3 && noteCount(*document) == 5, "Unit-only copy included unselected text");

    qInfo() << "Several selected notes preserve their relative positions";
    const auto firstCopy = document->annotationRecord(firstId);
    click(*canvas, *noteItem(*scene, firstId));
    click(*canvas, *noteItem(*scene, plain.id), Qt::ControlModifier);
    shortcut(*canvas, Qt::Key_C); shortcut(*canvas, Qt::Key_V);
    require(noteCount(*document) == 7 && unitCount(*scene) == 3 && scene->selectedItems().size() == 2,
            "Multiple selected notes were not pasted together");
    QSet<QString> copiedTexts;
    for (auto* item : scene->selectedItems()) {
        auto* note = dynamic_cast<AnnotationItem*>(item);
        require(note && note->record().kind == AnnotationKind::UserNote, "Unexpected pasted object");
        const auto record = document->annotationRecord(note->record().id);
        expectCopy(record.text == firstCopy.text ? firstCopy : plain, record, {40, 40});
        copiedTexts.insert(record.text);
    }
    require(copiedTexts == QSet<QString>{firstCopy.text, plain.text}, "Multiple-note paste omitted a note");

    const QString path = directory + "/pasted-notes.afs.json";
    QString error;
    require(ProjectSerializer::save(*scene, *document, path, &error), error);
    FlowsheetScene restored;
    FlowsheetDocument restoredDocument;
    require(ProjectSerializer::load(restored, restoredDocument, path, &error), error);
    require(noteCount(restoredDocument) == 7, "Project round trip lost pasted notes");
    expectCopy(original, restoredDocument.annotationRecord(firstId), {40, 40});
    expectCopy(original, restoredDocument.annotationRecord(secondId), {80, 80});
    expectCopy(plain, restoredDocument.annotationRecord(mixedId), {40, 40});

    qInfo() << "Canvas shortcuts leave ordinary text-field clipboard behavior intact";
    QLineEdit editor(&window);
    editor.setText("表格与输入框的普通文字"); editor.show(); editor.setFocus(); editor.selectAll();
    QTest::keyClick(&editor, Qt::Key_C, Qt::ControlModifier);
    require(QApplication::clipboard()->text() == editor.text(), "Canvas copy intercepted a text editor");
    editor.clear();
    QTest::keyClick(&editor, Qt::Key_V, Qt::ControlModifier);
    require(editor.text() == "表格与输入框的普通文字" && noteCount(*document) == 7,
            "Canvas paste intercepted a text editor");
    // Keep the pasted notes selected through MainWindow destruction. Scene
    // selection callbacks must not access panels already torn down.
}
} // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("AutoFlotationSheetTests");
    QCoreApplication::setApplicationName("ClipboardInteraction");
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
    try {
        testClipboard(directory.path());
        runReagentClipboardChecks(directory.path());
        runCopyMoveInteractionChecks();
        qInfo() << "All clipboard interaction checks passed";
    } catch (const std::exception& error) {
        qCritical().noquote() << error.what(); return 1;
    }
    return 0;
}
