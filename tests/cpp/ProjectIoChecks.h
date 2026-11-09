#pragma once

class QTemporaryDir;

namespace afs {
class FlowsheetScene;
class FlowsheetDocument;
}

namespace afs::tests {
int checkProjectRoundTrip(const QTemporaryDir& directory);
int checkProjectConnections(const QTemporaryDir& directory);
int checkProjectEquipment(const QTemporaryDir& directory);
int checkProjectHistory();
int checkScenarioNotes();
int checkVersion23Compatibility();
int checkProjectExample();
int checkExternalProject(const QTemporaryDir& directory);
int checkProjectFormat(const FlowsheetScene& source, const FlowsheetDocument& sourceDocument,
                       FlowsheetScene& loaded, FlowsheetDocument& loadedDocument);
} // namespace afs::tests
