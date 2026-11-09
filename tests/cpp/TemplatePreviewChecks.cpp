#include "WorkspaceToolsTestSupport.h"
#include "annotations/AnnotationItem.h"
#include "annotations/ReagentAnnotationItem.h"
#include "graphics/items/FeedJunctionItem.h"
#include "services/FlowGroupSnapshotOperations.h"
#include "services/FlowTemplateStore.h"
#include "services/ProjectSerializer.h"
#include "ui/FlowTemplateDock.h"
#include <QAction>
#include <QFile>
#include <QGraphicsSimpleTextItem>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineF>
#include <QLineEdit>
#include <QListWidget>
#include <QPointer>
#include <QPushButton>
#include <QSplitter>
#include <QStandardPaths>
#include <QUndoStack>
#include <QWheelEvent>
#include <cmath>

using namespace afs;
using namespace afs::tests;
namespace {
QByteArray bytes(const QString& path) { QFile file(path); require(file.open(QIODevice::ReadOnly), "Cannot read template"); return file.readAll(); }
int units(QGraphicsScene& scene) {
    int count = 0; for (auto* item : scene.items()) count += dynamic_cast<FlotationUnitItem*>(item) != nullptr; return count;
}
void selectTemplate(QListWidget& list, const QString& id) {
    for (int i = 0; i < list.count(); ++i) if (list.item(i)->data(Qt::UserRole) == id) {
        list.scrollToItem(list.item(i)); flush();
        QTest::mouseClick(list.viewport(), Qt::LeftButton, Qt::NoModifier, list.visualItemRect(list.item(i)).center()); flush(); return;
    }
    require(false, "Missing preview template");
}
void fits(CanvasView& view) {
    const QRectF visible = view.mapToScene(view.viewport()->rect()).boundingRect();
    for (auto* item : view.scene()->items()) if (item->isVisible())
        require(visible.contains(item->sceneBoundingRect()), "Preview fit clipped visible content or annotation");
}
void wheel(CanvasView& view, int delta) {
    const QPointF center = view.viewport()->rect().center();
    QWheelEvent event(center, view.viewport()->mapToGlobal(center.toPoint()), {}, {0, delta},
                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(view.viewport(), &event); flush();
}
}

void runTemplatePreviewChecks() {
    FlowTemplateStore store; QString error;
    FlowsheetScene source; FlowsheetDocument document;
    QDir root(QCoreApplication::applicationDirPath()); root.cdUp(); root.cdUp();
    require(ProjectSerializer::load(source, document, root.filePath("tests/copy-move.afs.json"), &error), error);
    for (auto* item : source.items()) if (dynamic_cast<FlotationUnitItem*>(item)) item->setSelected(true);
    document.setProductName("3:right", "Preview concentrate");
    AnnotationRecord reagent; reagent.id = "preview-reagent"; reagent.kind = AnnotationKind::Reagent;
    reagent.ownerKind = AnnotationOwnerKind::Stream; reagent.ownerId = "3:left"; reagent.text = "Collector";
    reagent.dosage = "120"; reagent.dosageUnit = "g/t"; document.setAnnotationRecord(reagent);
    flow_group::Snapshot snapshot; require(flow_group::capture(source, document, snapshot).units == 3, "Preview capture failed");
    for (const auto& record : document.annotationRecords()) if (record.kind == AnnotationKind::UserNote) snapshot.notes.append(record);
    const auto loopId = store.save("Preview loop", snapshot, &error); require(!loopId.isEmpty(), error);
    flow_group::Snapshot wide;
    FlotationUnit mill{"mill", {0, 0}}; mill.kind = UnitKind::BallMill;
    FlotationUnit thickener{"thickener", {20000, 10000}}; thickener.kind = UnitKind::Thickener;
    wide.units = {mill, thickener};
    const auto wideId = store.save("Wide template", wide, &error); require(!wideId.isEmpty(), error);
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/flow-templates/";
    const auto originalFile = bytes(directory + loopId + ".aft.json");

    ToolsUi ui; auto* mainUnit = unit(*ui.scene, "1"); ui.select(mainUnit); ui.undo->initialize();
    mainUnit->adjustWidth(20); QTest::qWait(250); flush();
    ui.canvas->setFocus(); QTest::keyClick(ui.canvas, Qt::Key_C, Qt::ControlModifier); flush();
    const auto before = ProjectSerializer::serialize(*ui.scene, *ui.document);
    const int undoIndex = ui.undo->stack()->index();
    auto* dock = ui.window.findChild<FlowTemplateDock*>();
    ui.window.findChild<WorkspaceSidebar*>()->showPanel("templates"); flush();
    auto* list = dock->findChild<QListWidget*>("flowTemplateList");
    auto* view = dynamic_cast<CanvasView*>(dock->findChild<QGraphicsView*>("templatePreviewView"));
    auto* insert = dock->findChild<QPushButton*>("insertFlowTemplate");
    auto* fit = dock->findChild<QPushButton*>("fitTemplatePreview");
    selectTemplate(*list, loopId);
    require(view->scene() && view->scene() != ui.scene && !view->isInteractive() && units(*view->scene()) == 3 && insert->isEnabled(),
            "Single click did not create an independent read-only preview");
    int notes = 0, reagents = 0, feeds = 0; bool name = false, dosage = false;
    QHash<QGraphicsItem*, QPointF> positions;
    for (auto* item : view->scene()->items()) {
        require(!(item->flags() & (QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsFocusable)),
                "Preview item is editable");
        positions.insert(item, item->pos());
        if (auto* note = dynamic_cast<AnnotationItem*>(item)) notes += note->record().kind == AnnotationKind::UserNote;
        if (auto* badge = dynamic_cast<ReagentAnnotationItem*>(item)) { ++reagents; dosage |= badge->record().dosage == "120"; }
        feeds += dynamic_cast<FeedJunctionItem*>(item) != nullptr;
        if (auto* label = dynamic_cast<QGraphicsSimpleTextItem*>(item)) name |= label->text() == "Preview concentrate";
    }
    require(notes == snapshot.notes.size() && reagents == snapshot.reagents.size() && dosage && feeds == 1 && name,
            "Preview lost loop structure, product names, notes or reagents");
    fits(*view); preview(ui.window, "template-preview-loop");
    const QPoint start = view->viewport()->rect().center();
    for (auto* item : view->scene()->items()) if (dynamic_cast<FlotationUnitItem*>(item)) {
        const QPoint target = view->mapFromScene(item->sceneBoundingRect().center());
        QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier, target);
        QTest::mouseMove(view->viewport(), target + QPoint(45, 30));
        QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, target + QPoint(45, 30));
        break;
    }
    for (auto* item : view->scene()->items()) if (dynamic_cast<AnnotationItem*>(item)) {
        QTest::mouseDClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->mapFromScene(item->sceneBoundingRect().center()));
        require(!QApplication::activeModalWidget(), "Double-clicking a preview note opened an editor");
        break;
    }
    view->setFocus(); QTest::keyClick(view, Qt::Key_Delete);
    QTest::keyClick(view, Qt::Key_Equal, Qt::ControlModifier);
    QTest::keyClick(view, Qt::Key_Z, Qt::ControlModifier);
    QTest::keyClick(view, Qt::Key_V, Qt::ControlModifier); flush();
    for (auto it = positions.cbegin(); it != positions.cend(); ++it) require(it.key()->pos() == it.value(), "Preview gesture edited an item");
    require(ProjectSerializer::serialize(*ui.scene, *ui.document) == before && mainUnit->isSelected()
        && ui.undo->stack()->index() == undoIndex, "Preview interaction changed the project, selection or undo history");
    QPointer<QGraphicsScene> previous = view->scene();
    selectTemplate(*list, wideId);
    require(previous.isNull() && units(*view->scene()) == 2, "Switching templates retained the old preview scene");
    fits(*view); const auto fittedScale = view->transform().m11();
    require(fittedScale < 0.2, "Large preview fixture did not exercise small fit scale");
    wheel(*view, 120);
    require(view->transform().m11() > fittedScale && view->transform().m11() < fittedScale * 1.3,
            "Zoom jumped to the main canvas minimum scale");
    const auto zoomScale = view->transform().m11();
    auto* splitter = dock->findChild<QSplitter*>("templatePreviewSplitter"); splitter->setSizes({160, 650}); flush();
    require(std::abs(view->transform().m11() - zoomScale) < 1e-9, "Splitter resize discarded manual zoom");
    const QPointF centerBefore = view->mapToScene(view->viewport()->rect().center());
    QTest::mousePress(view->viewport(), Qt::MiddleButton, Qt::NoModifier, start);
    QTest::mouseMove(view->viewport(), start + QPoint(40, 25));
    QTest::mouseRelease(view->viewport(), Qt::MiddleButton, Qt::NoModifier, start + QPoint(40, 25)); flush();
    require(QLineF(centerBefore, view->mapToScene(view->viewport()->rect().center())).length() > 1, "Middle-button pan did not move preview");
    QTest::mouseClick(fit, Qt::LeftButton); flush(); fits(*view);
    splitter->setSizes({240, 400}); flush(); fits(*view);
    selectTemplate(*list, loopId);
    auto* theme = ui.window.findChild<QAction*>("darkThemeAction");
    if (!theme) for (auto* action : ui.window.findChildren<QAction*>())
        if (action->shortcut() == QKeySequence("Ctrl+Shift+T")) theme = action;
    require(theme, "Missing theme action"); theme->trigger(); flush();
    require(view->scene()->backgroundBrush().color() == QApplication::palette().color(QPalette::Base), "Preview did not follow dark theme");
    preview(ui.window, "template-preview-dark"); theme->trigger(); flush();
    auto* search = dock->findChild<QLineEdit*>("templateSearch"); search->setText("no matches"); flush();
    require(!view->scene() && !insert->isEnabled() && dock->findChild<QLabel*>("templatePreviewMessage")->isVisible(), "Empty search retained stale preview");
    search->clear(); flush(); selectTemplate(*list, wideId);
    auto corrupt = QJsonDocument::fromJson(bytes(directory + wideId + ".aft.json")).object();
    auto project = corrupt.value("project").toObject(); project.insert("version", 22); corrupt.insert("project", project);
    { QFile file(directory + wideId + ".aft.json"); require(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "Cannot modify fixture"); file.write(QJsonDocument(corrupt).toJson()); }
    emit dock->refreshRequested(); flush();
    require(!view->scene() && !insert->isEnabled() && dock->findChild<QLabel*>("templatePreviewMessage")->text().contains("无法预览"), "Invalid template kept a usable stale preview");
    selectTemplate(*list, loopId); fits(*view);
    require(bytes(directory + loopId + ".aft.json") == originalFile && ProjectSerializer::serialize(*ui.scene, *ui.document) == before,
            "Preview changed the stored template or current project");
    ui.canvas->setFocus(); QTest::keyClick(ui.canvas, Qt::Key_V, Qt::ControlModifier); flush();
    require(units(*ui.scene) == 2 && unit(*ui.scene, "2")->unit().width == 380, "Preview overwrote the flow clipboard or consumed main unit IDs");
    QTest::mouseDClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, list->visualItemRect(list->currentItem()).center()); flush();
    require(units(*ui.scene) == 5, "Double-clicking a template no longer inserts its complete flow");
    require(store.remove(loopId, &error) && store.remove(wideId, &error), error);
    emit dock->refreshRequested(); flush();
    require(!view->scene() && !insert->isEnabled(), "Deleting the selected template left stale preview content");
}
