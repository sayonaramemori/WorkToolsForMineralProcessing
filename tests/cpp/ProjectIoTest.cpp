#include "ProjectIoChecks.h"

#include <QApplication>
#include <QTemporaryDir>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;
    using namespace afs::tests;
    if (const int result = checkProjectRoundTrip(directory)) return result;
    if (const int result = checkProjectConnections(directory)) return result;
    if (const int result = checkProjectEquipment(directory)) return result;
    if (const int result = checkProjectHistory()) return result;
    if (const int result = checkScenarioNotes()) return result;
    if (const int result = checkVersion23Compatibility()) return result;
    return checkProjectExample();
}
