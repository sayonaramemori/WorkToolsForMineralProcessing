#include "app/MainWindow.h"
#include "annotations/AnnotationItem.h"
#include "annotations/AnnotationManager.h"
#include "document/FlowsheetDocument.h"
#include "editor/CanvasView.h"
#include "editor/FlowsheetScene.h"
#include "services/ProjectUndoManager.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QTest>
#include <QTimer>
#include <stdexcept>

using namespace afs;
namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
AnnotationItem* findNote(FlowsheetScene& scene, const QString& id) {
    for (auto* item : scene.items())
        if (auto* note = dynamic_cast<AnnotationItem*>(item);
            note && note->record().kind == AnnotationKind::UserNote && note->record().id == id)
            return note;
    return nullptr;
}
}

void runScenarioNoteInteractionChecks() {
    MainWindow window; window.resize(1600, 1000); window.show(); window.activateWindow();
    QApplication::processEvents();
    auto* canvas = dynamic_cast<CanvasView*>(window.centralWidget());
    auto* scene = window.findChild<FlowsheetScene*>();
    auto* document = window.findChild<FlowsheetDocument*>();
    auto* manager = window.findChild<AnnotationManager*>();
    auto* undo = window.findChild<ProjectUndoManager*>();
    auto* selector = window.findChild<QComboBox*>("scenarioCombo");
    require(canvas && scene && document && manager && undo && selector, "Scenario note test setup failed");
    AnnotationRecord original;
    original.id = "independent-note"; original.ownerId = original.id;
    original.kind = AnnotationKind::UserNote; original.ownerKind = AnnotationOwnerKind::Free;
    original.style = AnnotationStyle::Note; original.text = "基准方案文字";
    original.manualOffset = {-100, -180}; original.manuallyPlaced = true;
    original.notePointSize = 14; original.noteBorderVisible = true;
    document->setAnnotationRecord(original); manager->synchronize();
    const int copy = document->addScenario("副本", true);
    selector->setCurrentIndex(copy); QApplication::processEvents();
    require(findNote(*scene, original.id) && document->annotationRecord(original.id).text == original.text,
            "Copied scenario did not display its copied note");
    undo->initialize();

    // Edit through the actual canvas double-click dialog and item signal path.
    canvas->centerOn(findNote(*scene, original.id)); canvas->setFocus();
    QApplication::processEvents();
    bool edited = false;
    QTimer::singleShot(0, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (!dialog) return;
        auto* text = dialog->findChild<QPlainTextEdit*>();
        auto* size = dialog->findChild<QSpinBox*>();
        if (!text || !size) { dialog->reject(); return; }
        text->setPlainText("副本专用文字"); size->setValue(24);
        for (auto* check : dialog->findChildren<QCheckBox*>()) {
            if (check->text() == "使用粗体") check->setChecked(true);
            if (check->text() == "显示边框和背景") check->setChecked(false);
        }
        edited = true; dialog->accept();
    });
    auto* item = findNote(*scene, original.id);
    const QPoint position = canvas->mapFromScene(item->mapToScene(QPointF(12, 12)));
    QTest::mouseDClick(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, position);
    QApplication::processEvents();
    require(edited && document->annotationRecord(original.id).text == "副本专用文字"
        && document->annotationRecord(original.id).notePointSize == 24,
        "Canvas editing did not update the current scenario note");
    window.activateWindow(); canvas->setFocus(); QApplication::processEvents();
    undo->commitPending();
    undo->undo();
    require(document->annotationRecord(original.id).text == original.text, "Note edit could not be undone");
    undo->redo();
    require(document->annotationRecord(original.id).text == "副本专用文字", "Note edit could not be redone");
    item = findNote(*scene, original.id);
    // Geometry changes use the same placementEdited signal as dragging.
    item->setPos(item->pos() + QPointF(70, 50));
    const auto moved = document->annotationRecord(original.id);
    require(moved.manualOffset == original.manualOffset + QPointF(70, 50), "Note move was not stored");
    selector->setCurrentIndex(0); QApplication::processEvents();
    item = findNote(*scene, original.id);
    require(item && item->record().text == original.text && item->pos() == original.manualOffset
        && item->record().notePointSize == 14 && !item->record().noteBold
        && item->record().noteBorderVisible, "Editing a copied note modified the original scenario");
    selector->setCurrentIndex(copy); QApplication::processEvents();
    item = findNote(*scene, original.id);
    require(item && item->record().text == moved.text && item->pos() == moved.manualOffset,
            "Switching scenarios lost edited text or position");
    scene->clearSelection(); item->setSelected(true); canvas->setFocus();
    QTest::keyClick(canvas->viewport(), Qt::Key_Delete); QApplication::processEvents();
    require(!findNote(*scene, original.id) && !document->annotationRecords().contains(original.id),
            "Delete did not remove the current scenario note");
    selector->setCurrentIndex(0); QApplication::processEvents();
    require(findNote(*scene, original.id), "Deleting copied note removed the original scenario note");

    // Clipboard survives scenario changes and pastes into the destination only.
    scene->clearSelection(); findNote(*scene, original.id)->setSelected(true); canvas->setFocus();
    QTest::keyClick(canvas->viewport(), Qt::Key_C, Qt::ControlModifier);
    QApplication::processEvents();
    const int empty = document->addScenario("空白", false);
    selector->setCurrentIndex(empty); QApplication::processEvents();
    require(!findNote(*scene, original.id), "Empty scenario inherited shared custom text");
    canvas->setFocus(); QTest::keyClick(canvas->viewport(), Qt::Key_V, Qt::ControlModifier);
    QApplication::processEvents();
    QString pastedId;
    for (const auto& record : document->annotationRecords())
        if (record.kind == AnnotationKind::UserNote && record.text == original.text) pastedId = record.id;
    require(!pastedId.isEmpty() && pastedId != original.id && findNote(*scene, pastedId),
            "Cross-scenario paste did not create an independent note");
    selector->setCurrentIndex(0); QApplication::processEvents();
    require(!findNote(*scene, pastedId) && findNote(*scene, original.id),
            "Cross-scenario paste leaked into the original scenario");
    selector->setCurrentIndex(copy); QApplication::processEvents();
    require(!findNote(*scene, original.id) && !findNote(*scene, pastedId),
            "Deleted note reappeared after another scenario pasted text");
}
