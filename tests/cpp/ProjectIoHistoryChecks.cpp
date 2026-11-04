#include "ProjectIoChecks.h"
#include "document/FlowsheetDocument.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/ProductLineItem.h"
#include "services/ProjectSerializer.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "services/ProjectUndoManager.h"
#include "services/FlowGroupClipboard.h"
#include <QCoreApplication>
#include <cmath>

namespace afs::tests {

int checkProjectHistory() {
    FlowsheetScene undoScene;
    FlowsheetDocument undoDocument;
    undoScene.addItem(new FlotationUnitItem({"undo-a", {0, 0}}));
    ProjectUndoManager undoManager(undoScene, undoDocument, {}, {});
    undoManager.initialize();
    undoScene.addItem(new FlotationUnitItem({"undo-b", {300, 200}}));
    undoDocument.setDryMass("undo-a:left", 25.0);
    undoManager.scheduleCheckpoint("test edit");
    undoManager.undo();
    const auto unitCount = [&undoScene] {
        int count = 0;
        for (auto* item : undoScene.items())
            if (dynamic_cast<FlotationUnitItem*>(item)) ++count;
        return count;
    };
    if (unitCount() != 1 || undoDocument.measurement("undo-a:left").dryMass) return 47;
    undoManager.redo();
    if (unitCount() != 2
        || undoDocument.measurement("undo-a:left").dryMass != std::optional<double>(25.0))
        return 48;

    // Manual merge corridors use scene coordinates. A copied group must offset
    // them with its cloned units, and must retain that relationship when the
    // whole clone is subsequently dragged.
    FlowsheetScene clipboardScene;
    auto* copyA = new FlotationUnitItem({"copy-a", {0, 0}});
    auto* copyB = new FlotationUnitItem({"copy-b", {300, 140}});
    auto* copyC = new FlotationUnitItem({"copy-c", {650, 340}});
    clipboardScene.addItem(copyA);
    clipboardScene.addItem(copyB);
    clipboardScene.addItem(copyC);
    auto* copyMerge = clipboardScene.mergeProducts(
        copyA->products().front(), copyB->products().front(), "copy-merge");
    if (!copyMerge || !clipboardScene.connectMergeDirect(copyMerge, copyC->inputLine())) return 75;
    copyMerge->setManualMergeY(480.0);
    copyA->setSelected(true); copyB->setSelected(true); copyC->setSelected(true);
    FlowsheetDocument clipboardDocument;
    FlowGroupClipboard clipboard;
    if (clipboard.copy(clipboardScene, clipboardDocument).merges != 1) return 76;
    int cloneNumber = 0;
    if (clipboard.paste(clipboardScene, clipboardDocument,
                        [&cloneNumber] { return QString("clone-%1").arg(++cloneNumber); }).merges != 1)
        return 77;
    FlotationUnitItem* cloneA = nullptr;
    FlotationUnitItem* cloneB = nullptr;
    FlotationUnitItem* cloneC = nullptr;
    for (auto* item : clipboardScene.items()) {
        auto* unit = dynamic_cast<FlotationUnitItem*>(item);
        if (!unit) continue;
        if (unit->unit().id == "clone-1") cloneA = unit;
        else if (unit->unit().id == "clone-2") cloneB = unit;
        else if (unit->unit().id == "clone-3") cloneC = unit;
    }
    if (!cloneA || !cloneB || !cloneC) return 78;
    auto* cloneMerge = cloneA->products().front()->mergeJunction();
    if (!cloneMerge || !cloneMerge->manualMergeY()
        || std::abs(*cloneMerge->manualMergeY() - 520.0) > 0.001) return 79;
    const QPointF dragDelta(70.0, -90.0);
    // A programmatic move translates the component. Qt's per-selected-item
    // mouse dispatch is covered separately by CopyMoveInteractionChecks.
    cloneA->setPos(cloneA->pos() + dragDelta);
    QCoreApplication::processEvents();
    if (!cloneMerge->manualMergeY()
        || std::abs(*cloneMerge->manualMergeY() - 430.0) > 0.001) return 80;
    return 0;
}

} // namespace afs::tests
