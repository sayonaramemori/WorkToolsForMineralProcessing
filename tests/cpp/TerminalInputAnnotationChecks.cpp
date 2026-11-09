#include "TerminalInputChecks.h"
#include "annotations/AnnotationItem.h"
#include "annotations/AnnotationManager.h"
#include "annotations/ReagentAnnotationItem.h"
#include <QApplication>
#include <QKeyEvent>
#include <cmath>
#include <utility>

namespace afs::tests {
int runTerminalFormulaChecks() {
    AnnotationRecord formulaRecord{"formula", AnnotationKind::Reagent,
        AnnotationStyle::PlainText, AnnotationOwnerKind::Stream, "test", {}, false, true};
    formulaRecord.text = "3418A + CuSO_4 + SO_4^{2-} + Fe_2O_3";
    ReagentAnnotationItem formulaItem(formulaRecord);
    const QString formulaHtml = formulaItem.formattedHtml();
    if (!formulaHtml.contains("3418A") || formulaHtml.contains("<sub>3418</sub>")
        || !formulaHtml.contains("CuSO<sub>4</sub>")
        || !formulaHtml.contains("SO<sub>4</sub><sup>2-</sup>")
        || !formulaHtml.contains("Fe<sub>2</sub>O<sub>3</sub>")) return 35;
    return 0;
}

int runTerminalAnnotationChecks(TerminalInputContext& context) {
    auto& calculationScene = context.calculationScene;
    const auto& calculationSnapshot = context.calculationSnapshot;
    auto& result = context.result;
    FlowsheetDocument resultDocument;
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
    annotationManager.setMassUnit(MassUnit::Kilogram);
    if (!leftAnnotation->text().contains(" kg")
        || resultDocument.annotationTextSettings().massUnit != MassUnit::Kilogram) return 78;
    annotationManager.setCustomMassUnit("dry lb");
    if (!leftAnnotation->text().contains(" dry lb")
        || resultDocument.annotationTextSettings().massUnit != MassUnit::Custom
        || resultDocument.annotationTextSettings().customMassUnit != "dry lb") return 79;
    calculationScene.clearSelection();
    leftAnnotation->setSelected(true);
    QKeyEvent hideResult(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
    QApplication::sendEvent(&calculationScene, &hideResult);
    if (leftAnnotation->isVisible()
        || !resultDocument.annotationRecord("result:calculation:left").visible) return 76;
    const auto recalculatedResult = *resultDocument.calculationResult();
    resultDocument.setCalculationResult(recalculatedResult);
    if (!leftAnnotation->isVisible()) return 77;
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
    resultDocument.setProductName("calculation:left", "测试精矿");
    annotationManager.setProductNamesVisible(true);
    if (!leftAnnotation->text().contains("产品  测试精矿")) return 78;
    annotationManager.setProductNamesVisible(false);
    if (leftAnnotation->text().contains("产品  测试精矿")) return 79;

    AnnotationRecord userNote{"note-test", AnnotationKind::UserNote, AnnotationStyle::Note,
        AnnotationOwnerKind::Free, "canvas", QPointF(180, 90), true, true, "自定义\n说明"};
    userNote.noteFontFamily = "DejaVu Sans";
    userNote.notePointSize = 18;
    userNote.noteBold = true;
    userNote.noteBorderVisible = false;
    userNote.noteColor = QColor("#2c6da4");
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
        || noteItem->record().noteBorderVisible
        || noteItem->record().noteColor != QColor("#2c6da4")) return 42;
    noteItem->setPos(210, 120);
    if (resultDocument.annotationRecord("note-test").manualOffset != QPointF(210, 120)) return 43;

    resultDocument.setDryMass("calculation:left", 11.0);
    if (resultDocument.calculationResult() || leftAnnotation->isVisible()
        || !noteItem->isVisible()) return 24;

    return 0;
}
}
