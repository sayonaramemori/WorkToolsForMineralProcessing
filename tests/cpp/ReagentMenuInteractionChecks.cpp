#include "ReagentMenuTestSupport.h"
#include "annotations/AnnotationItem.h"
#include "annotations/ReagentOrder.h"
#include "services/FlowsheetCalculationService.h"
#include "services/ProjectSerializer.h"
#include <QClipboard>
#include <QSignalSpy>
#include <QStatusBar>
#include <QUndoStack>
#include <algorithm>

using namespace afs;
using namespace afs::tests::reagent_menu;
namespace {
void transfers(const QString& directory) {
    Ui ui; auto* root = ui.unit();
    const auto source = sourceRecord();
    auto target = sourceRecord("zz-existing-target", "1:right"); target.text = "Existing"; target.manualOffset.setY(160);
    ui.document->setAnnotationRecord(source); ui.document->setAnnotationRecord(target);
    ui.document->setDryMass("1:left", 20); ui.document->setGradePercent("1:left", 2);
    ui.document->setDryMass("1:right", 80); ui.document->setGradePercent("1:right", 1);
    ui.document->setCalculationResult(FlowsheetCalculationService::calculate(*ui.scene, *ui.document));
    ui.manager->synchronize(); flush();
    const QPointF destination = root->products().back()->sourceAnchorScenePosition() + QPointF(0, 30);
    const auto emptyPaste = menu(ui, destination, "pasteReagentAnnotationAction", false);
    require(emptyPaste.found && !emptyPaste.enabled && count(*ui.document) == 2, "Empty clipboard enabled paste");
    ui.scene->clearSelection(); root->setSelected(true); ui.window.activateWindow(); ui.canvas->setFocus(); flush();
    QTest::keyClick(ui.canvas->viewport(), Qt::Key_C, Qt::ControlModifier); flush();
    require(ui.window.statusBar()->currentMessage().contains("已复制 1 个单元"), "Cannot prepare the flow-group clipboard");
    QApplication::clipboard()->setText("ordinary clipboard");
    ui.undo->initialize();
    QSignalSpy projects(ui.document, &FlowsheetDocument::projectChanged);
    QSignalSpy calculations(ui.document, &FlowsheetDocument::calculationChanged);
    QSignalSpy topology(ui.scene, &FlowsheetScene::topologyChanged);
    const auto beforeCopy = ProjectSerializer::serialize(*ui.scene, *ui.document);
    copy(ui, source.id);
    require(ProjectSerializer::serialize(*ui.scene, *ui.document) == beforeCopy && projects.isEmpty()
        && ui.undo->stack()->index() == 0 && QApplication::clipboard()->text() == "ordinary clipboard",
        "Copy modified the project, undo history or system clipboard");
    auto changedSource = source; changedSource.text = "Edited after copying";
    ui.document->setAnnotationRecord(changedSource); ui.manager->synchronize(); flush();
    const QPointF existingPosition = ui.reagent(target.id)->pos();
    ui.scene->clearSelection(); root->products().front()->setSelected(true);
    const auto first = paste(ui, destination, "1:right");
    sameContent(source, ui.document->annotationRecord(first));
    require(ui.document->annotationRecord(source.id).text == changedSource.text && ui.reagent(target.id)->pos() == existingPosition,
            "Paste changed the source or moved an existing target reagent");
    require(ui.reagent(first)->sceneBoundingRect().top() >= ui.reagent(target.id)->sceneBoundingRect().bottom() + 7.9,
            "Pasted reagent overlaps the target's manually positioned reagent");
    const auto firstPosition = ui.reagent(first)->pos();
    const auto second = paste(ui, destination, "1:right");
    require(first != second && count(*ui.document) == 4 && ui.reagent(first)->pos() == firstPosition
        && ui.reagent(second)->sceneBoundingRect().top() >= ui.reagent(first)->sceneBoundingRect().bottom() + 7.9,
        "Repeated paste reused IDs, moved the previous copy or overlapped it");
    require(calculations.isEmpty() && topology.isEmpty() && ui.document->calculationResult()->fullySolved,
            "Reagent paste invalidated calculation or changed topology");
    ui.undo->undo(); flush();
    require(count(*ui.document) == 3 && !ui.reagent(second) && ui.reagent(first), "One undo did not remove only the latest paste");
    ui.undo->redo(); flush(); require(count(*ui.document) == 4 && ui.reagent(second), "Redo did not restore the pasted reagent");
    const auto beforeInvalid = ProjectSerializer::serialize(*ui.scene, *ui.document);
    ui.scene->clearSelection(); ui.unit()->products().back()->setSelected(true);
    const auto blank = menu(ui, ui.unit()->pos() + QPointF(650, -450), "pasteReagentAnnotationAction", false);
    require(!blank.found && ProjectSerializer::serialize(*ui.scene, *ui.document) == beforeInvalid, "Blank context used a previously selected line");
    for (auto* item : ui.scene->items()) if (auto* card = dynamic_cast<AnnotationItem*>(item);
        card && card->record().kind == AnnotationKind::StreamResult && card->isVisible()) {
        require(!menu(ui, card->sceneBoundingRect().center(), "copyReagentAnnotationAction", false).found, "Result card offered reagent copying"); break;
    }
    QString error; const auto path = directory + "/reagent-menu.afs.json";
    require(ProjectSerializer::save(*ui.scene, *ui.document, path, &error), error);
    FlowsheetScene restored; FlowsheetDocument restoredDocument;
    require(ProjectSerializer::load(restored, restoredDocument, path, &error), error);
    AnnotationManager restoredManager(restored, restoredDocument); restoredManager.synchronize();
    sameContent(source, restoredDocument.annotationRecord(first)); sameContent(source, restoredDocument.annotationRecord(second));
    QHash<QString, QPointF> restoredPositions;
    for (auto* item : restored.items()) if (auto* reagent = dynamic_cast<ReagentAnnotationItem*>(item)) restoredPositions.insert(reagent->record().id, reagent->pos());
    require(restoredPositions.value(first) == ui.reagent(first)->pos() && restoredPositions.value(second) == ui.reagent(second)->pos(),
            "Project round trip changed pasted reagent ordering or placement");
    ui.window.activateWindow(); ui.canvas->setFocus(); flush();
    ui.scene->clearSelection(); ui.unit()->setSelected(true);
    QTest::keyClick(ui.canvas->viewport(), Qt::Key_C, Qt::ControlModifier); flush();
    QTest::keyClick(ui.canvas->viewport(), Qt::Key_V, Qt::ControlModifier); flush();
    require(ui.unit("2") && !ui.unit("3"), "Cannot switch back to flow-group copying: " + ui.window.statusBar()->currentMessage());
    ui.scene->clearSelection(); ui.unit()->setSelected(true); ui.canvas->setFocus();
    QTest::keyClick(ui.canvas->viewport(), Qt::Key_C, Qt::ControlModifier); QTest::keyClick(ui.canvas->viewport(), Qt::Key_V, Qt::ControlModifier); flush();
    QVector<AnnotationRecord> groupCopies;
    for (const auto& record : ui.document->annotationRecords())
        if (record.kind == AnnotationKind::Reagent && record.ownerId == "3:right") groupCopies.append(record);
    std::sort(groupCopies.begin(), groupCopies.end(), reagentDisplayLess);
    require(groupCopies.size() == 3, "Group copy omitted individually pasted reagents");
    const QVector<QString> originalIds{target.id, first, second};
    for (int i = 0; i < originalIds.size(); ++i)
        require(ui.reagent(groupCopies[i].id)->pos() == ui.reagent(originalIds[i])->pos() + QPointF(40, 40),
                "Group copy changed the display order of individually pasted reagents");
    ui.scene->clearSelection(); ui.reagent(source.id)->setSelected(true); ui.canvas->setFocus();
    QTest::keyClick(ui.canvas->viewport(), Qt::Key_Delete); flush();
    require(!ui.reagent(source.id), "Cannot delete the copied source reagent");
    sameContent(source, ui.document->annotationRecord(paste(ui,
        ui.unit()->products().back()->sourceAnchorScenePosition() + QPointF(0, 30), "1:right")));
    require(!ui.reagent(source.id), "Paste recreated the deleted source reagent");
    const auto oldScenario = ui.document->scenarios().front().reagentAnnotations;
    const int scenario = ui.document->addScenario("Other scenario", false);
    require(ui.document->setCurrentScenario(scenario), "Cannot switch scenario"); flush();
    const auto crossScenario = paste(ui, ui.unit()->products().back()->sourceAnchorScenePosition() + QPointF(0, 30), "1:right");
    sameContent(source, ui.document->annotationRecord(crossScenario));
    require(count(*ui.document) == 1 && ui.document->scenarios().front().reagentAnnotations.size() == oldScenario.size(),
            "Cross-scenario paste changed the source scenario");
}
}
void runReagentMenuInteractionChecks(const QString& directory) { transfers(directory); }
