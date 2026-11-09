#include "app/MainWindow.h"
#include "annotations/AnnotationItem.h"
#include "annotations/ReagentAnnotationItem.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FeedJunctionItem.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"
#include <QStatusBar>

namespace afs {
void MainWindow::copyCanvasSelection() {
    const auto selected = m_scene->selectedItems();
    // A selected group keeps its existing automatic reagent capture. A lone
    // reagent uses the separate value snapshot shared with the context menu.
    for (auto* item : selected) {
        auto* note = dynamic_cast<AnnotationItem*>(item);
        if (dynamic_cast<FlotationUnitItem*>(item)
            || (note && note->record().kind == AnnotationKind::UserNote)) {
            copySelectedFlowGroup();
            return;
        }
    }
    if (selected.size() == 1)
        if (auto* reagent = dynamic_cast<ReagentAnnotationItem*>(selected.front())) {
            copyReagentAnnotation(reagent->record().id);
            return;
        }
    m_canvasClipboardKind = CanvasClipboardKind::Empty;
    statusBar()->showMessage(tr("请选择流程单元、自定义文字或一条药剂标注进行复制。"), 4000);
}

void MainWindow::pasteCanvasSelection() {
    if (m_canvasClipboardKind == CanvasClipboardKind::Empty) {
        statusBar()->showMessage(tr("请先复制流程单元、自定义文字或一条药剂标注。"), 4000);
        return;
    }
    if (m_canvasClipboardKind == CanvasClipboardKind::FlowGroup) {
        pasteFlowGroup();
        return;
    }
    const auto selected = m_scene->selectedItems();
    QString streamId;
    if (selected.size() == 1) {
        auto* item = selected.front();
        if (auto* product = dynamic_cast<ProductLineItem*>(item)) streamId = product->streamId();
        else if (auto* input = dynamic_cast<InputLineItem*>(item)) streamId = input->unit()->unit().id + ":feed";
        else if (auto* merge = dynamic_cast<MergeJunctionItem*>(item)) streamId = merge->selectedAnnotationStreamId();
        else if (auto* feed = dynamic_cast<FeedJunctionItem*>(item)) streamId = feed->selectedAnnotationStreamId();
        else if (auto* reagent = dynamic_cast<ReagentAnnotationItem*>(item)) streamId = reagent->record().ownerId;
    }
    if (streamId.isEmpty()) {
        statusBar()->showMessage(tr("请单击选中一段可明确区分的目标物流线，再按 Ctrl+V 粘贴药剂标注。"), 4000);
        return;
    }
    pasteReagentAnnotation(streamId);
}
}
