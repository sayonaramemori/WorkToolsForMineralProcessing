#pragma once
#include "adapters/CanvasTopologyBuilder.h"
#include "document/FlowsheetDocument.h"
#include "editor/FlowsheetScene.h"

namespace afs::tests {
// Only the scene/value fixtures needed by more than one check group are shared.
// Views and models remain local to the group that exercises them.
struct TerminalInputContext {
    FlowsheetScene inputScene;
    FlowsheetDocument inputDocument;
    CanvasTopologySnapshot connected;
    FlowsheetScene calculationScene;
    FlotationUnitItem* calculationUnit{nullptr};
    CanvasTopologySnapshot calculationSnapshot;
    topology::CalculationResult result;
};
int runTerminalFormulaChecks();
int runTerminalModelChecks(TerminalInputContext& context);
int runTerminalCalculationChecks(TerminalInputContext& context);
int runTerminalFilterChecks(TerminalInputContext& context);
int runTerminalAnnotationChecks(TerminalInputContext& context);
int runTerminalClosedLoopChecks();
}
