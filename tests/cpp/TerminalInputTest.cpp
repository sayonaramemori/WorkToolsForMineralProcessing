#include "TerminalInputChecks.h"
#include <QApplication>

using namespace afs::tests;

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    if (const int code = runTerminalFormulaChecks()) return code;
    TerminalInputContext context;
    if (const int code = runTerminalModelChecks(context)) return code;
    if (const int code = runTerminalCalculationChecks(context)) return code;
    if (const int code = runTerminalFilterChecks(context)) return code;
    if (const int code = runTerminalAnnotationChecks(context)) return code;
    return runTerminalClosedLoopChecks();
}
