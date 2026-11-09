#pragma once
#include "app/MainWindow.h"
#include "document/FlowsheetDocument.h"
#include "editor/CanvasView.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FlotationUnitItem.h"
#include "services/ProjectUndoManager.h"
#include "ui/ObjectPropertiesDock.h"
#include "ui/WorkspaceSidebar.h"
#include <QApplication>
#include <QEvent>
#include <QDoubleSpinBox>
#include <QDir>
#include <QPixmap>
#include <QTest>
#include <stdexcept>
namespace afs::tests {
inline void require(bool value, const QString& message) { if (!value) throw std::runtime_error(message.toStdString()); }
inline void flush() {
    for (int i = 0; i < 3; ++i) { QApplication::processEvents(); QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete); }
}
inline void preview(QWidget& widget, const QString& name) {
    const auto directory = qEnvironmentVariable("AFS_WORKSPACE_PREVIEW_DIRECTORY");
    if (!directory.isEmpty()) { QDir().mkpath(directory); widget.grab().save(directory + '/' + name + ".png"); }
}
inline FlotationUnitItem* unit(FlowsheetScene& scene, const QString& id) {
    for (auto* item : scene.items()) if (auto* value = dynamic_cast<FlotationUnitItem*>(item); value && value->unit().id == id) return value;
    return nullptr;
}
struct ToolsUi {
    MainWindow window;
    CanvasView* canvas{dynamic_cast<CanvasView*>(window.centralWidget())};
    FlowsheetScene* scene{qobject_cast<FlowsheetScene*>(canvas->scene())};
    FlowsheetDocument* document{window.findChild<FlowsheetDocument*>()};
    ProjectUndoManager* undo{window.findChild<ProjectUndoManager*>()};
    ObjectPropertiesDock* properties{window.findChild<ObjectPropertiesDock*>()};
    ToolsUi() { window.resize(1800, 1100); window.show(); window.activateWindow(); flush(); }
    void select(QGraphicsItem* item) { scene->clearSelection(); item->setSelected(true); window.findChild<WorkspaceSidebar*>()->showPanel("properties"); flush(); }
    void number(const QString& key, double value) {
        auto* editor = properties->findChild<QDoubleSpinBox*>("property_" + key);
        require(editor && editor->isVisible(), "Missing property editor: " + key);
        editor->setFocus(); editor->setValue(value); QTest::keyClick(editor, Qt::Key_Return); flush();
        require(QApplication::focusWidget() && properties->isAncestorOf(QApplication::focusWidget()),
                "Property editing lost keyboard focus after commit");
    }
};
}
