#include "WorkspaceToolsTestSupport.h"
#include "annotations/AnnotationItem.h"
#include "annotations/AnnotationManager.h"
#include "graphics/items/ProductLineItem.h"
#include "graphics/items/InputLineItem.h"
#include "services/FlowGroupSnapshotOperations.h"
#include "services/FlowTemplateStore.h"
#include "services/ProjectSerializer.h"
#include "ui/FlowTemplateDock.h"
#include <QAction>
#include <QDir>
#include <QFile>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidget>
#include <QMessageBox>
#include <QAbstractButton>
#include <QPushButton>
#include <QTimer>
using namespace afs;
using namespace afs::tests;
namespace {
int countUnits(FlowsheetScene& scene) {
    int count = 0; for (auto* item : scene.items()) if (dynamic_cast<FlotationUnitItem*>(item)) ++count; return count;
}
void persistence(const QString& directory) {
    FlowsheetScene scene; FlowsheetDocument document; QString error;
    QDir root(QCoreApplication::applicationDirPath()); root.cdUp(); root.cdUp();
    require(ProjectSerializer::load(scene, document, root.filePath("tests/copy-move.afs.json"), &error), error);
    for (auto* item : scene.items()) if (dynamic_cast<FlotationUnitItem*>(item)) item->setSelected(true);
    AnnotationRecord reagent; reagent.id = "template-reagent"; reagent.kind = AnnotationKind::Reagent;
    reagent.ownerKind = AnnotationOwnerKind::Stream; reagent.ownerId = "3:left"; reagent.text = "Collector"; reagent.dosage = "120"; reagent.dosageUnit = "g/t";
    document.setAnnotationRecord(reagent);
    flow_group::Snapshot snapshot; require(flow_group::capture(scene, document, snapshot).units == 3, "Template fixture capture failed");
    for (const auto& record : document.annotationRecords()) if (record.kind == AnnotationKind::UserNote) snapshot.notes.append(record);
    require(snapshot.notes.size() == 3 && !snapshot.reagents.isEmpty(), "Fixture annotation capture failed");
    FlowTemplateStore store(directory + "/library"); const auto id = store.save("Loop template", snapshot, &error);
    require(!id.isEmpty(), error); require(store.list().size() == 1, "Saved template missing from library");
    const auto loaded = FlowTemplateStore(directory + "/library").load(id, &error); require(loaded.has_value(), error);
    require(loaded->units.size() == 3 && loaded->feeds.size() == 1 && loaded->notes.size() == 3 && loaded->reagents.size() == snapshot.reagents.size(),
        "Template reload lost topology, notes or reagents");
    require(std::abs(loaded->feeds.front().routeXs.value("3:left")
        - (snapshot.feeds.front().routeXs.value("3:left") - snapshot.units.front().position.x())) < 1e-6,
        "Template normalization changed return corridor structure");
    require(loaded->terminalLengths == snapshot.terminalLengths && loaded->productNames == snapshot.productNames,
        "Template reload lost endpoint lengths or product names");
    bool dosageRetained = false;
    for (const auto& record : loaded->reagents)
        dosageRetained |= record.text == "Collector" && record.dosage == "120" && record.dosageUnit == "g/t";
    require(dosageRetained, "Template reload lost reagent dosage or units");
    FlowsheetScene target; FlowsheetDocument targetDocument; target.addItem(new FlotationUnitItem({"existing", {0, 0}})); int index = 0;
    const auto result = flow_group::paste(*loaded, {500, 500}, target, targetDocument, [&] { return QString("insert-%1").arg(++index); });
    require(result.units == 3 && result.feeds == 1 && result.reagents == snapshot.reagents.size() && result.noteIds.size() == 3 && countUnits(target) == 4,
        "Template insertion lost content");
    require(targetDocument.measurements().isEmpty(), "Template inserted experimental measurements");
    require(store.rename(id, "Renamed", &error) && store.list().front().name == "Renamed", error);
    require(!store.rename(id, " ", &error) && !store.load("../unsafe", &error), "Invalid template input was accepted");
    const auto path = directory + "/library/" + id + ".aft.json";
    QFile file(path); require(file.open(QIODevice::ReadOnly), "Cannot inspect template"); auto object = QJsonDocument::fromJson(file.readAll()).object(); file.close();
    auto project = object.value("project").toObject(); project.insert("version", 22); object.insert("project", project);
    require(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "Cannot corrupt test template"); file.write(QJsonDocument(object).toJson()); file.close();
    require(!store.load(id, &error) && !error.isEmpty(), "Old or corrupt project format in template was accepted");
    require(store.remove(id, &error) && store.list().isEmpty(), "Template removal did not persist");
}
QPushButton* button(FlowTemplateDock& dock, const QString& label) {
    for (auto* value : dock.findChildren<QPushButton*>()) if (value->text() == label) return value;
    return nullptr;
}
void uiLibrary() {
    {
        ToolsUi ui; auto* root = unit(*ui.scene, "1"); root->adjustWidth(60); root->adjustInputLength(98);
        root->products().front()->setTerminalLength(123); ui.document->setProductName("1:left", "Template product");
        ui.document->setDryMass("1:left", 50); ui.document->setGradePercent("1:left", 1.2);
        AnnotationRecord note; note.id = "template-note"; note.kind = AnnotationKind::UserNote; note.ownerKind = AnnotationOwnerKind::Free;
        note.ownerId = "canvas"; note.text = "Stage note"; note.manuallyPlaced = true; note.manualOffset = root->pos() + QPointF(40, 50); ui.document->setAnnotationRecord(note);
        AnnotationRecord reagent; reagent.id = "template-reagent"; reagent.kind = AnnotationKind::Reagent;
        reagent.ownerKind = AnnotationOwnerKind::Stream; reagent.ownerId = "1:left"; reagent.text = "Collector"; ui.document->setAnnotationRecord(reagent);
        ui.window.findChild<AnnotationManager*>()->synchronize(); ui.select(root);
        for (auto* item : ui.scene->items()) if (auto* text = dynamic_cast<AnnotationItem*>(item); text && text->record().id == note.id) text->setSelected(true);
        flush(); const auto before = ProjectSerializer::serialize(*ui.scene, *ui.document); bool prompted = false;
        QTimer::singleShot(0, [&] {
            if (auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget())) { prompted = true; dialog->setTextValue("Reusable stage"); dialog->accept(); }
        });
        ui.window.findChild<QAction*>("saveFlowTemplateAction")->trigger(); flush();
        require(prompted && FlowTemplateStore().list().size() == 1, "Save-template UI did not persist selection");
        require(ProjectSerializer::serialize(*ui.scene, *ui.document) == before, "Saving a local template modified the project");
        root->adjustWidth(80); // Stored template must remain an immutable snapshot.
    }
    ToolsUi ui; auto* dock = ui.window.findChild<FlowTemplateDock*>(); ui.window.findChild<WorkspaceSidebar*>()->showPanel("templates"); flush();
    auto* list = dock->findChild<QListWidget*>("flowTemplateList"); require(list->count() == 1, "New window did not restore template library");
    ui.undo->initialize(); QTest::mouseClick(dock->findChild<QPushButton*>("insertFlowTemplate"), Qt::LeftButton); flush();
    auto* clone = unit(*ui.scene, "2");
    require(clone && clone->unit().width == 420 && clone->inputLine()->length() == 170
        && clone->products().front()->terminalLength() == 123 && ui.document->productName("2:left") == "Template product",
        "Template UI insertion lost geometry or stable ID remapping");
    require(ui.document->measurements().isEmpty(), "Template UI copied measurements");
    int notes = 0, reagents = 0;
    for (const auto& record : ui.document->annotationRecords()) { notes += record.kind == AnnotationKind::UserNote; reagents += record.kind == AnnotationKind::Reagent; }
    require(notes == 1 && reagents == 1, "Template UI insertion lost text or reagent");
    preview(ui.window, "templates");
    ui.undo->undo(); flush(); require(countUnits(*ui.scene) == 1 && ui.document->annotationRecords().isEmpty(), "Template insertion did not undo atomically");
    ui.undo->redo(); flush(); require(countUnits(*ui.scene) == 2 && unit(*ui.scene, "2"), "Template insertion did not redo");
    QTest::mouseClick(dock->findChild<QPushButton*>("insertFlowTemplate"), Qt::LeftButton); flush();
    require(countUnits(*ui.scene) == 3 && unit(*ui.scene, "3"), "Repeated insertion reused unit IDs");
    require(unit(*ui.scene, "2")->pos() != unit(*ui.scene, "3")->pos(), "Repeated template insertion overlapped exactly");
    const auto before = ProjectSerializer::serialize(*ui.scene, *ui.document);
    QTimer::singleShot(0, [] { if (auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget())) { dialog->setTextValue("Renamed stage"); dialog->accept(); } });
    QTest::mouseClick(button(*dock, "重命名"), Qt::LeftButton); flush();
    require(FlowTemplateStore().list().front().name == "Renamed stage", "Template rename UI did not persist");
    QTimer::singleShot(0, [] { if (auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) dialog->button(QMessageBox::Yes)->click(); });
    QTest::mouseClick(button(*dock, "删除"), Qt::LeftButton); flush();
    require(FlowTemplateStore().list().isEmpty() && ProjectSerializer::serialize(*ui.scene, *ui.document) == before,
        "Template deletion modified inserted project content");
}
}
void runFlowTemplateChecks(const QString& directory) { persistence(directory); uiLibrary(); }
