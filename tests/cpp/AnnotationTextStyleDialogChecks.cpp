#include "app/MainWindow.h"
#include "document/FlowsheetDocument.h"
#include "editor/FlowsheetScene.h"
#include "services/ProjectSerializer.h"
#include "services/ProjectUndoManager.h"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <stdexcept>

using namespace afs;

namespace {
void require(bool condition, const QString& message) {
    if (!condition) throw std::runtime_error(message.toStdString());
}
}

void runAnnotationTextStyleDialogChecks() {
    MainWindow window; window.resize(1500, 1000); window.show(); window.activateWindow();
    QApplication::processEvents();
    auto* document = window.findChild<FlowsheetDocument*>();
    auto* scene = window.findChild<FlowsheetScene*>();
    auto* undo = window.findChild<ProjectUndoManager*>();
    QAction* edit = nullptr;
    for (auto* action : window.findChildren<QAction*>())
        if (action->text() == "标注样式") edit = action;
    require(edit, "Annotation style menu is missing");
    const auto original = document->annotationTextSettings();
    const auto before = ProjectSerializer::serialize(*scene, *document);
    undo->initialize();
    const auto changeStyle = [&](bool accept) {
        QString error;
        QTimer::singleShot(0, [&] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if (!dialog) { error = "Style editor did not open a modal dialog"; return; }
            auto* pointSize = dialog->findChild<QSpinBox*>();
            QCheckBox* bold = nullptr; QCheckBox* followTheme = nullptr; QPushButton* color = nullptr;
            for (auto* checkbox : dialog->findChildren<QCheckBox*>()) {
                if (checkbox->text() == "加粗") bold = checkbox;
                if (checkbox->text() == "跟随界面主题颜色") followTheme = checkbox;
            }
            for (auto* button : dialog->findChildren<QPushButton*>())
                if (button->text().startsWith('#')) color = button;
            if (!pointSize || !bold || !followTheme || !color) {
                error = "Style dialog lost its input controls"; dialog->reject(); return;
            }
            pointSize->setValue(19); bold->setChecked(true);
            followTheme->setChecked(true);
            if (color->isEnabled()) error = "Theme-following color button must be disabled";
            followTheme->setChecked(false);
            if (!color->isEnabled()) error = "Custom color button must be enabled";
            if (accept && error.isEmpty()) dialog->accept(); else dialog->reject();
        });
        edit->trigger(); QApplication::processEvents();
        require(error.isEmpty(), error);
    };
    changeStyle(false);
    require(ProjectSerializer::serialize(*scene, *document) == before, "Cancelled style edit modified the project");
    changeStyle(true);
    auto settings = document->annotationTextSettings();
    require(settings.pointSize == 19 && settings.bold && settings.color.isValid(), "Accepted style edit was not committed");
    undo->undo(); settings = document->annotationTextSettings();
    require(settings.pointSize == original.pointSize && settings.bold == original.bold && settings.color == original.color,
            "Style edit was not undoable");
    undo->redo(); settings = document->annotationTextSettings();
    require(settings.pointSize == 19 && settings.bold && settings.color.isValid(), "Style edit could not be redone");
}
