#include "app/MainWindow.h"

#include <QApplication>
#include <QFont>
#include <QTimer>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setStyle("Fusion");
    QApplication::setApplicationName("AutoFlotationSheet");
    QApplication::setOrganizationName("AutoFlotationSheet");
    app.setFont(QFont("Microsoft YaHei", 10));
    afs::MainWindow window;
    window.show();
    if (app.arguments().contains("--smoke-test") || qEnvironmentVariableIsSet("AFS_SMOKE_TEST"))
        QTimer::singleShot(100, &app, &QCoreApplication::quit);
    return app.exec();
}
