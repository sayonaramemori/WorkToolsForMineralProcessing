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
    if (app.arguments().contains("--smoke-test") || qEnvironmentVariableIsSet("AFS_SMOKE_TEST")) {
        QTimer::singleShot(100, &window, &QWidget::close);
        QTimer::singleShot(200, &app, [] {
            for (auto* widget : QApplication::topLevelWidgets()) {
                if (auto* messageBox = qobject_cast<QMessageBox*>(widget)) {
                    messageBox->done(QMessageBox::Discard);
                    break;
                }
            }
        });
    }
    return app.exec();
}
