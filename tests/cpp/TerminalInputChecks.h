#pragma once
#include "adapters/CanvasTopologyBuilder.h"
#include "document/FlowsheetDocument.h"
#include "editor/FlowsheetScene.h"

namespace afs::tests {
int runTerminalSessionChecks();
// Only the scene/value fixtures needed by more than one check group are shared.
// Views and models remain local to the group that exercises them.
struct TerminalInputContext {
    FlowsheetScene inputScene;
    FlowsheetDocument inputDocument;
    FlowsheetSnapshot connected;
    FlowsheetScene calculationScene;
    FlotationUnitItem* calculationUnit{nullptr};
    FlowsheetSnapshot calculationSnapshot;
    topology::CalculationResult result;
};
int runTerminalFormulaChecks();
int runTerminalModelChecks(TerminalInputContext& context);
int runTerminalCalculationChecks(TerminalInputContext& context);
int runTerminalFilterChecks(TerminalInputContext& context);
int runTerminalAnnotationChecks(TerminalInputContext& context);
int runTerminalClosedLoopChecks();
}
