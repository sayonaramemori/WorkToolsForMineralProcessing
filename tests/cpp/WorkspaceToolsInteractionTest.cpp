#include <QApplication>
#include <QDebug>
#include <QSettings>
#include <QTemporaryDir>
#include <stdexcept>
void runObjectPropertyChecks();
void runCalculationDiagnosticsChecks();
void runCalculationControllerChecks();
void runWorkspaceSidebarChecks();
void runWorkspacePanelSelectorChecks();
void runTerminalTableLayoutChecks();
void runFlowTemplateChecks(const QString& directory);
void runTemplatePreviewChecks();
int main(int argc, char** argv) {
    QTemporaryDir directory; if (!directory.isValid()) return 1;
    qputenv("XDG_DATA_HOME", directory.path().toUtf8());
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("AutoFlotationSheetTests");
    QCoreApplication::setApplicationName("WorkspaceTools");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
    try {
        runWorkspaceSidebarChecks(); runWorkspacePanelSelectorChecks();
        runTerminalTableLayoutChecks();
        runObjectPropertyChecks(); runCalculationDiagnosticsChecks(); runCalculationControllerChecks(); runFlowTemplateChecks(directory.path());
        runTemplatePreviewChecks();
        qInfo() << "All workspace tools interaction checks passed";
    } catch (const std::exception& error) { qCritical().noquote() << error.what(); return 1; }
    return 0;
}
