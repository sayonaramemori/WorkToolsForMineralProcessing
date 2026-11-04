#include "ProjectIoChecks.h"
#include "adapters/CanvasTopologyBuilder.h"
#include "document/FlowsheetDocument.h"
#include "editor/FlowsheetScene.h"
#include "services/ProjectSerializer.h"
#include "services/FlowsheetCalculationService.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QTemporaryDir>

namespace afs::tests {

int checkProjectExample() {
    QString error;
    FlowsheetScene exampleScene;
    FlowsheetDocument exampleDocument;
    QDir exampleRoot(QCoreApplication::applicationDirPath());
    exampleRoot.cdUp();
    exampleRoot.cdUp();
    if (!ProjectSerializer::load(exampleScene, exampleDocument,
                                 exampleRoot.filePath(
                                     QStringLiteral("tests/test.json")),
                                 &error)) {
        qCritical() << "example load failed:" << error;
        return 81;
    }
    const auto exampleSnapshot = CanvasTopologyBuilder::build(exampleScene);
    const auto exampleFeeds = exampleSnapshot.graph.externalFeedStreams();
    if (exampleFeeds.size() != 1 || exampleFeeds.front() != QStringLiteral("12:feed")) return 82;
    const auto exampleResult = FlowsheetCalculationService::calculate(exampleSnapshot, exampleDocument);
    if (!exampleResult.values.contains(QStringLiteral("12:feed"))) return 84;
    // The example intentionally has some unmeasured terminal streams, so its
    // feed is not expected to be fully solved.  It must nevertheless have no
    // phantom fresh-feed boundaries at its source-only multi-input junctions.
    for (const auto& issue : exampleResult.issues)
        if (issue.code == topology::IssueCode::Underdetermined
            && issue.objectId.contains(QStringLiteral(":external-feed"))) return 83;
    return 0;
}

int checkExternalProject(const QTemporaryDir& directory) {
    QString error;
    // Allows an explicitly supplied real-world project to be checked by the
    // complete reader/restorer pipeline without making a developer-local path
    // part of the normal test fixture.
    const QString externalProject = qEnvironmentVariable("AFS_PROJECT_IO_EXTERNAL_PATH");
    if (!externalProject.isEmpty()) {
        FlowsheetScene externalScene;
        FlowsheetDocument externalDocument;
        if (!ProjectSerializer::load(externalScene, externalDocument, externalProject, &error)) {
            qWarning().noquote() << error;
            return 71;
        }
        if (externalScene.items().isEmpty()) return 72;
        const QString externalRoundTrip = directory.filePath("external-roundtrip.afs.json");
        if (!ProjectSerializer::save(externalScene, externalDocument, externalRoundTrip, &error)) {
            qWarning().noquote() << error;
            return 73;
        }
        FlowsheetScene roundTrippedScene;
        FlowsheetDocument roundTrippedDocument;
        if (!ProjectSerializer::load(
                roundTrippedScene, roundTrippedDocument, externalRoundTrip, &error)) {
            qWarning().noquote() << error;
            return 74;
        }
    }
    return 0;
}

} // namespace afs::tests
