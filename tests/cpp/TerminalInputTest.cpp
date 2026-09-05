#include "adapters/CanvasTopologyBuilder.h"
#include "annotations/AnnotationItem.h"
#include "annotations/AnnotationManager.h"
#include "annotations/ReagentAnnotationItem.h"
#include "document/FlowsheetDocument.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/ProductLineItem.h"
#include "ui/TerminalProductTableModel.h"
#include "ui/TerminalProductDock.h"
#include "services/ThemeService.h"
#include "services/FlowsheetCalculationService.h"
#include "topology/OpenCircuitCalculator.h"

#include <QApplication>
#include <QBrush>
#include <QTableWidget>
#include <QTableView>
#include <QComboBox>
#include <QPushButton>
#include <QAction>
#include <QMenu>
#include <QGraphicsSimpleTextItem>
#include <QKeyEvent>

using namespace afs;

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    AnnotationRecord formulaRecord{"formula", AnnotationKind::Reagent,
        AnnotationStyle::PlainText, AnnotationOwnerKind::Stream, "test", {}, false, true};
    formulaRecord.text = "3418A + CuSO_4 + SO_4^{2-} + Fe_2O_3";
    ReagentAnnotationItem formulaItem(formulaRecord);
    const QString formulaHtml = formulaItem.formattedHtml();
    if (!formulaHtml.contains("3418A") || formulaHtml.contains("<sub>3418</sub>")
        || !formulaHtml.contains("CuSO<sub>4</sub>")
        || !formulaHtml.contains("SO<sub>4</sub><sup>2-</sup>")
        || !formulaHtml.contains("Fe<sub>2</sub>O<sub>3</sub>")) return 35;
    FlowsheetScene scene;
    auto* first = new FlotationUnitItem({"first", {0, 0}});
    auto* second = new FlotationUnitItem({"second", {400, 300}});
    scene.addItem(first);
    scene.addItem(second);

    auto initial = CanvasTopologyBuilder::build(scene);
    if (initial.terminalProducts.size() != 4) return 1;
    if (initial.graph.externalFeedStreams().size() != 2) return 2;

    auto* stream = first->products().at(1);
    if (stream->streamId() != "first:right") return 3;
    stream->setProductName("右侧产品");
    stream->setTextSettings({13, true, QColor("#225588")});
    auto* productLabel = dynamic_cast<QGraphicsSimpleTextItem*>(stream->childItems().value(0));
    if (!productLabel || productLabel->text() != "右侧产品" || !productLabel->isVisible()
        || productLabel->font().pointSize() != 13 || !productLabel->font().bold()
        || productLabel->brush().color() != QColor("#225588")) return 33;
    if (!scene.connectProduct(stream, second->inputLine())) return 4;
    auto connected = CanvasTopologyBuilder::build(scene);
    if (connected.terminalProducts.size() != 3) return 5;
    if (connected.productStreams.size() != 4) return 36;
    if (connected.reportStreams.size() != 5
        || connected.reportStreams.front().streamId != "first:feed"
        || !connected.reportStreams.front().displayName.contains("合计")) return 38;
    const auto* graphStream = connected.graph.stream("first:right");
    if (!graphStream || !graphStream->target || graphStream->target->nodeId != "second") return 6;

    FlowsheetDocument document;
    TerminalProductTableModel model(document);
    model.setStreams(connected.terminalProducts);
    if (model.data(model.index(0, TerminalProductTableModel::MassColumn),
                   Qt::BackgroundRole).isValid()
        || model.data(model.index(0, TerminalProductTableModel::GradeColumn),
                      Qt::BackgroundRole).isValid()) return 39;
    if (!model.setData(model.index(0, TerminalProductTableModel::MassColumn), "12.5", Qt::EditRole)) return 7;
    if (model.data(model.index(0, TerminalProductTableModel::MassColumn), Qt::BackgroundRole).isValid()
        || !model.data(model.index(0, TerminalProductTableModel::GradeColumn),
                       Qt::BackgroundRole).canConvert<QBrush>()) return 40;
    if (!model.setData(model.index(0, TerminalProductTableModel::GradeColumn), "8.2", Qt::EditRole)) return 8;
    if (model.data(model.index(0, TerminalProductTableModel::GradeColumn),
                   Qt::BackgroundRole).isValid()) return 41;
    if (!model.setData(model.index(0, TerminalProductTableModel::ProductNameColumn),
                       "精矿产品", Qt::EditRole)
        || document.productName(model.streamAt(0)->streamId) != "精矿产品") return 30;
    if (!model.setData(model.index(0, TerminalProductTableModel::ProductNameColumn),
                       "", Qt::EditRole)
        || !document.productName(model.streamAt(0)->streamId).isEmpty()) return 31;
    if (model.setData(model.index(0, TerminalProductTableModel::GradeColumn), "101", Qt::EditRole)) return 9;
    const auto id = model.streamAt(0)->streamId;
    if (!document.measurement(id).complete()) return 10;
    document.setComponents({{"component-1", "Cu"}, {"component-2", "Zn"}});
    const int zincGradeColumn = model.gradeColumn("component-2");
    if (model.columnCount() != 9 || zincGradeColumn < 0
        || !model.flags(model.index(0, zincGradeColumn)).testFlag(Qt::ItemIsEditable)
        || !model.data(model.index(0, zincGradeColumn), Qt::BackgroundRole).canConvert<QBrush>())
        return 51;
    if (!model.setData(model.index(0, zincGradeColumn), "1.6", Qt::EditRole)
        || !document.measurement(id).completeFor(document.componentIds())) return 52;

    QSet<QString> editableIds;
    for (const auto& product : connected.reportStreams) editableIds.insert(product.streamId);
    model.setStreams(connected.reportStreams, editableIds);
    const int intermediateRow = model.rowForGraphicsItem(stream);
    if (intermediateRow < 0
        || model.data(model.index(intermediateRow, TerminalProductTableModel::MassColumn),
                      Qt::BackgroundRole).isValid()
        || !model.flags(model.index(intermediateRow, TerminalProductTableModel::MassColumn))
            .testFlag(Qt::ItemIsEditable)
        || !model.flags(model.index(intermediateRow, TerminalProductTableModel::ProductNameColumn))
            .testFlag(Qt::ItemIsEditable)
        || !model.setData(model.index(intermediateRow, TerminalProductTableModel::MassColumn),
                          "99", Qt::EditRole)
        || !model.data(model.index(intermediateRow, TerminalProductTableModel::GradeColumn),
                       Qt::BackgroundRole).canConvert<QBrush>()) return 37;

    if (!scene.disconnectProduct(stream)) return 11;
    auto disconnected = CanvasTopologyBuilder::build(scene);
    if (disconnected.terminalProducts.size() != 4) return 12;
    if (!document.measurement(id).complete()) return 13;

    ThemeService::applyApplicationPalette(true);
    TerminalProductDock dock(document);
    dock.refreshAppearance();
    auto* panel = dock.findChild<QWidget*>("terminalProductPanel");
    if (!panel || !panel->styleSheet().contains("#ff242628")) return 14;
    ThemeService::applyApplicationPalette(false);
    dock.refreshAppearance();
    if (!panel->styleSheet().contains("#fff4f4f4")
        || panel->styleSheet().contains("#ff242628")) return 15;

    FlowsheetScene calculationScene;
    auto* calculationUnit = new FlotationUnitItem({"calculation", {0, 0}});
    calculationScene.addItem(calculationUnit);
    const auto calculationSnapshot = CanvasTopologyBuilder::build(calculationScene);
    FlowsheetDocument calculationDocument;
    calculationDocument.setDryMass("calculation:left", 10.0);
    calculationDocument.setGradePercent("calculation:left", 5.0);
    calculationDocument.setDryMass("calculation:right", 90.0);
    calculationDocument.setGradePercent("calculation:right", 1.0);
    calculationDocument.setComponents({{"component-1", "Cu"}, {"component-2", "Pb"}});
    calculationDocument.setGradePercent("calculation:left", "component-2", 2.0);
    calculationDocument.setGradePercent("calculation:right", "component-2", 0.5);
    auto result = FlowsheetCalculationService::calculate(calculationScene, calculationDocument);
    if (!result.complete || !result.values.contains("calculation:feed")) return 16;
    if (result.components.size() != 2
        || !result.components["component-1"].values.contains("calculation:feed")
        || !result.components["component-2"].values.contains("calculation:feed")
        || std::abs(result.components["component-1"].values["calculation:feed"].gradePercent()
                    - 1.4) > 0.001
        || std::abs(result.components["component-2"].values["calculation:feed"].gradePercent()
                    - 0.65) > 0.001) return 50;

    FlowsheetDocument flexibleDocument;
    flexibleDocument.setDryMass("calculation:feed", 100.0);
    flexibleDocument.setGradePercent("calculation:feed", 2.0);
    flexibleDocument.setDryMass("calculation:left", 30.0);
    flexibleDocument.setGradePercent("calculation:left", 5.0);
    const auto flexibleResult = FlowsheetCalculationService::calculate(
        calculationScene, flexibleDocument);
    if (!flexibleResult.complete || !flexibleResult.fullySolved
        || flexibleResult.dryMassDegreesOfFreedom != 0
        || flexibleResult.componentMassDegreesOfFreedom != 0
        || std::abs(flexibleResult.values["calculation:right"].dryMass - 70.0) > 0.001
        || std::abs(flexibleResult.values["calculation:right"].gradePercent()
                    - (0.5 / 70.0 * 100.0)) > 0.001) return 53;

    FlowsheetDocument filterDocument;
    filterDocument.setDryMass("calculation:feed", 100.0);
    filterDocument.setGradePercent("calculation:feed", 2.0);
    const auto insufficient = FlowsheetCalculationService::calculate(
        calculationScene, filterDocument);
    if (insufficient.dryMassDegreesOfFreedom != 1
        || insufficient.componentMassDegreesOfFreedom != 1) return 54;
    TerminalProductDock filterDock(filterDocument);
    filterDock.setSnapshot(calculationSnapshot);
    auto* filterCombo = filterDock.findChild<QComboBox*>("streamFilterCombo");
    auto* filterTable = filterDock.findChild<QTableView*>("terminalProductTable");
    if (!filterCombo || !filterTable || filterTable->model()->rowCount() != 3) return 55;
    filterCombo->setCurrentText("已填写");
    if (filterTable->model()->rowCount() != 1) return 56;
    filterCombo->setCurrentText("终端产品");
    if (filterTable->model()->rowCount() != 2) return 57;
    filterCombo->setCurrentText("浮选入料");
    if (filterTable->model()->rowCount() != 1) return 58;

    FlowsheetScene propagationScene;
    auto* cleaner = new FlotationUnitItem({"cleaner", {0, 0}});
    auto* scavenger = new FlotationUnitItem({"scavenger", {400, 300}});
    propagationScene.addItem(cleaner);
    propagationScene.addItem(scavenger);
    if (!propagationScene.connectProduct(cleaner->products().at(1), scavenger->inputLine()))
        return 59;
    FlowsheetDocument propagationDocument;
    propagationDocument.setDryMass("scavenger:left", 35.0);
    propagationDocument.setGradePercent("scavenger:left", 4.0);
    propagationDocument.setDryMass("scavenger:right", 65.0);
    propagationDocument.setGradePercent("scavenger:right", 1.0);
    const auto propagationResult = FlowsheetCalculationService::calculate(
        propagationScene, propagationDocument);
    if (!propagationResult.values.contains("cleaner:right")
        || std::abs(propagationResult.values["cleaner:right"].dryMass - 100.0) > 0.001
        || std::abs(propagationResult.values["cleaner:right"].gradePercent() - 2.05) > 0.001)
        return 60;

    TerminalProductDock interestDock(filterDocument);
    interestDock.setSnapshot(connected);
    auto* interestButton = interestDock.findChild<QPushButton*>("interestObjectButton");
    auto* interestTable = interestDock.findChild<QTableView*>("terminalProductTable");
    if (!interestButton || !interestButton->menu() || !interestTable
        || interestTable->model()->rowCount() != 5) return 61;
    QAction* firstUnitAction = nullptr;
    for (auto* action : interestButton->menu()->actions())
        if (action->data().toString() == "first") { firstUnitAction = action; break; }
    if (!firstUnitAction) return 62;
    firstUnitAction->setChecked(true);
    if (interestTable->model()->rowCount() != 3) return 63;

    FlowsheetDocument resultDocument;
    TerminalProductDock resultDock(resultDocument);
    resultDocument.setCalculationResult(std::move(result));
    AnnotationRecord reagentA{"reagent-a", AnnotationKind::Reagent,
        AnnotationStyle::PlainText, AnnotationOwnerKind::Stream,
        "calculation:left", {}, false, true};
    reagentA.text = "药剂 A";
    AnnotationRecord reagentB = reagentA;
    reagentB.id = "reagent-b";
    reagentB.text = "药剂 B";
    resultDocument.setAnnotationRecord(reagentA);
    resultDocument.setAnnotationRecord(reagentB);
    AnnotationManager annotationManager(calculationScene, resultDocument);
    annotationManager.synchronize();
    if (annotationManager.annotationCount() != 3) return 17;
    QVector<ReagentAnnotationItem*> stackedReagents;
    for (auto* graphicsItem : calculationScene.items())
        if (auto* reagent = dynamic_cast<ReagentAnnotationItem*>(graphicsItem))
            stackedReagents.append(reagent);
    if (stackedReagents.size() != 2
        || stackedReagents[0]->pos() == stackedReagents[1]->pos()) return 45;
    auto* draggedReagent = stackedReagents[0];
    const QPointF reagentStart = draggedReagent->pos();
    calculationScene.clearSelection();
    draggedReagent->setSelected(true);
    if (!annotationManager.nudgeSelectedReagents(2.0)) return 49;
    if (std::abs(draggedReagent->pos().x() - reagentStart.x()) > 0.001
        || std::abs(draggedReagent->pos().y() - reagentStart.y() - 2) > 0.001)
        return 46;
    const auto draggedRecord = resultDocument.annotationRecord(draggedReagent->record().id);
    if (!draggedRecord.manuallyPlaced
        || std::abs(draggedRecord.manualOffset.x()) > 0.001
        || std::abs(draggedRecord.manualOffset.y() - 2) > 0.001)
        return 47;
    annotationManager.synchronize();
    if (std::abs(draggedReagent->pos().x() - reagentStart.x()) > 0.001
        || std::abs(draggedReagent->pos().y() - reagentStart.y() - 2) > 0.001)
        return 48;
    AnnotationItem* leftAnnotation = nullptr;
    for (auto* graphicsItem : calculationScene.items()) {
        auto* annotation = dynamic_cast<AnnotationItem*>(graphicsItem);
        if (annotation && annotation->record().ownerId == "calculation:left") {
            leftAnnotation = annotation;
            break;
        }
    }
    if (!leftAnnotation || !leftAnnotation->isVisible()) return 18;
    if (!leftAnnotation->text().contains("质量") || !leftAnnotation->text().contains("品位")
        || leftAnnotation->text().contains("产率")) return 19;
    resultDocument.setAnnotationTextSettings({15, true, QColor("#c03040")});
    if (leftAnnotation->textSettings().pointSize != 15
        || !leftAnnotation->textSettings().bold
        || leftAnnotation->textSettings().color != QColor("#c03040")) return 32;
    leftAnnotation->setPos(leftAnnotation->pos() + QPointF(20, 15));
    const QPointF manuallyPlacedPosition = leftAnnotation->pos();
    const auto savedRecord = resultDocument.annotationRecord("result:calculation:left");
    if (!savedRecord.manuallyPlaced || savedRecord.manualOffset != leftAnnotation->manualOffset()) return 20;
    annotationManager.setMetricVisible(ResultMetric::OverallYield, true);
    if (!leftAnnotation->text().contains("产率") || leftAnnotation->pos() != manuallyPlacedPosition) return 21;
    annotationManager.setMetricLabelMode(MetricLabelMode::Symbols);
    if (!leftAnnotation->text().contains("m  ")
        || !leftAnnotation->text().contains("β 目标组分")
        || !leftAnnotation->text().contains("γ  ") || leftAnnotation->text().contains("品位"))
        return 34;
    annotationManager.setMetricVisible(ResultMetric::DryMass, false);
    if (leftAnnotation->text().contains("质量")) return 22;

    AnnotationRecord userNote{"note-test", AnnotationKind::UserNote, AnnotationStyle::Note,
        AnnotationOwnerKind::Free, "canvas", QPointF(180, 90), true, true, "自定义\n说明"};
    userNote.noteFontFamily = "DejaVu Sans";
    userNote.notePointSize = 18;
    userNote.noteBold = true;
    userNote.noteBorderVisible = false;
    auto largeNoteRecord = userNote;
    largeNoteRecord.notePointSize = 48;
    AnnotationItem largeNote(largeNoteRecord, largeNoteRecord.text);
    if (largeNote.boundingRect().height() < 100.0) return 44;
    AnnotationItem resizedNote(userNote, userNote.text);
    const QSizeF smallNoteSize = resizedNote.boundingRect().size();
    resizedNote.setRecord(largeNoteRecord);
    if (resizedNote.boundingRect().width() <= smallNoteSize.width()
        || resizedNote.boundingRect().height() <= smallNoteSize.height()) return 45;
    resultDocument.setAnnotationRecord(userNote);
    annotationManager.synchronize();
    AnnotationItem* noteItem = nullptr;
    for (auto* graphicsItem : calculationScene.items()) {
        auto* annotation = dynamic_cast<AnnotationItem*>(graphicsItem);
        if (annotation && annotation->record().kind == AnnotationKind::UserNote) {
            noteItem = annotation; break;
        }
    }
    if (!noteItem || noteItem->pos() != QPointF(180, 90)
        || noteItem->text() != "自定义\n说明"
        || noteItem->record().notePointSize != 18 || !noteItem->record().noteBold
        || noteItem->record().noteBorderVisible) return 42;
    noteItem->setPos(210, 120);
    if (resultDocument.annotationRecord("note-test").manualOffset != QPointF(210, 120)) return 43;

    resultDock.showUnitResult("calculation", calculationSnapshot.graph);
    auto* details = resultDock.findChild<QTableWidget*>("resultDetailTable");
    if (!details || details->rowCount() != 6 || details->columnCount() != 4) return 23;
    resultDocument.setDryMass("calculation:left", 11.0);
    if (resultDocument.calculationResult() || leftAnnotation->isVisible()
        || !noteItem->isVisible()) return 24;

    FlowsheetScene closedScene;
    auto* closedUpper = new FlotationUnitItem({"closed-upper", {0, 0}});
    auto* closedLower = new FlotationUnitItem({"closed-lower", {500, 400}});
    closedScene.addItem(closedUpper);
    closedScene.addItem(closedLower);
    if (!closedScene.connectProduct(closedUpper->products().at(1), closedLower->inputLine())
        || !closedScene.connectProduct(closedLower->products().at(0), closedUpper->inputLine()))
        return 25;
    const auto closedSnapshot = CanvasTopologyBuilder::build(closedScene);
    if (closedSnapshot.terminalProducts.size() != 2
        || closedSnapshot.requiredMeasurements.size() != 3) return 26;
    FlowsheetDocument closedDocument;
    TerminalProductDock closedDock(closedDocument);
    closedDock.setSnapshot(closedSnapshot);
    if (closedDock.model()->rowCount() != closedSnapshot.reportStreams.size()
        || closedDock.model()->editableCount() != closedSnapshot.reportStreams.size()) return 27;
    closedDocument.setDryMass("closed-upper:left", 10.0);
    closedDocument.setGradePercent("closed-upper:left", 5.0);
    closedDocument.setDryMass("closed-lower:right", 40.0);
    closedDocument.setGradePercent("closed-lower:right", 1.0);
    closedDocument.setDryMass("closed-lower:left", 50.0);
    closedDocument.setGradePercent("closed-lower:left", 2.0);
    const auto closedResult = FlowsheetCalculationService::calculate(closedScene, closedDocument);
    if (!closedResult.complete || closedResult.values.size() != closedSnapshot.graph.streamIds().size())
        return 28;
    closedDocument.setCalculationResult(closedResult);
    AnnotationManager closedAnnotations(closedScene, closedDocument);
    closedAnnotations.synchronize();
    if (closedAnnotations.annotationCount() != closedResult.values.size()) return 29;
    return 0;
}
