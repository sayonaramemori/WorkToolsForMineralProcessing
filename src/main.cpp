#include "app/MainWindow.h"

#include <QApplication>
#include <QFont>
#include <QMessageBox>
#include <QTimer>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setStyle("Fusion");
    QApplication::setApplicationName("AutoFlotationSheet");
    QApplication::setOrganizationName("AutoFlotationSheet");
    app.setFont(QFont("Microsoft YaHei", 10));
    afs::MainWindow window;
    window.show();
    const QStringList arguments = app.arguments();
    const int openProjectArgument = arguments.indexOf(QStringLiteral("--open-project"));
    if (openProjectArgument >= 0 && openProjectArgument + 1 < arguments.size()) {
        const QString projectPath = arguments.at(openProjectArgument + 1);
        QTimer::singleShot(0, &window, [&window, projectPath] {
            window.openProject(projectPath);
        });
    }
    if (arguments.contains("--smoke-test") || qEnvironmentVariableIsSet("AFS_SMOKE_TEST")) {
        // A startup project is loaded asynchronously and may first need to
        // discard the unsaved blank document. Keep the smoke route entirely
        // non-interactive so it exercises the same load/teardown sequence.
        for (int delay = 100; delay <= 700; delay += 100) {
            QTimer::singleShot(delay, &app, [] {
                for (auto* widget : QApplication::topLevelWidgets()) {
                    if (auto* messageBox = qobject_cast<QMessageBox*>(widget))
                        messageBox->done(QMessageBox::Discard);
                }
            });
        }
        QTimer::singleShot(800, &window, &QWidget::close);
    }
    return app.exec();
}
