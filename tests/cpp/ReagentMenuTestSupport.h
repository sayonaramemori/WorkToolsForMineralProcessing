#pragma once
#include "app/MainWindow.h"
#include "annotations/AnnotationManager.h"
#include "annotations/ReagentAnnotationItem.h"
#include "document/FlowsheetDocument.h"
#include "editor/CanvasView.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/ProductLineItem.h"
#include "services/ProjectUndoManager.h"
#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QMenu>
#include <QTest>
#include <QTimer>
#include <functional>
#include <stdexcept>

namespace afs::tests::reagent_menu {
inline void require(bool value, const QString& message) { if (!value) throw std::runtime_error(message.toStdString()); }
inline void flush() {
    for (int i = 0; i < 3; ++i) { QApplication::processEvents(); QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete); }
}
struct Ui {
    MainWindow window;
    CanvasView* canvas{dynamic_cast<CanvasView*>(window.centralWidget())};
    FlowsheetScene* scene{qobject_cast<FlowsheetScene*>(canvas->scene())};
    FlowsheetDocument* document{window.findChild<FlowsheetDocument*>()};
    AnnotationManager* manager{window.findChild<AnnotationManager*>()};
    ProjectUndoManager* undo{window.findChild<ProjectUndoManager*>()};
    Ui() { window.resize(1600, 1000); window.show(); window.activateWindow(); flush(); }
    FlotationUnitItem* unit(const QString& id = "1") const {
        for (auto* item : scene->items()) if (auto* unit = dynamic_cast<FlotationUnitItem*>(item); unit && unit->unit().id == id) return unit;
        return nullptr;
    }
    ReagentAnnotationItem* reagent(const QString& id) const {
        for (auto* item : scene->items()) if (auto* label = dynamic_cast<ReagentAnnotationItem*>(item); label && label->record().id == id) return label;
        return nullptr;
    }
    QString selectedReagent() const {
        for (auto* item : scene->selectedItems()) if (auto* label = dynamic_cast<ReagentAnnotationItem*>(item)) return label->record().id;
        return {};
    }
};
struct MenuResult { bool shown{false}, found{false}, enabled{false}, noShortcut{false}; QString target; };
inline MenuResult menu(Ui& ui, QPointF point, const QString& name, bool invoke = true,
                       const std::function<void()>& beforeActivate = {}) {
    ui.scene->setSceneRect(ui.scene->sceneRect().united(QRectF(point - QPointF(700, 700), QSizeF(1400, 1400))));
    ui.canvas->centerOn(point); flush();
    const auto local = ui.canvas->mapFromScene(point);
    require(ui.canvas->viewport()->rect().contains(local), "Context target is outside the viewport");
    MenuResult result;
    QTimer::singleShot(0, &ui.window, [&] {
        auto* popup = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (!popup) return;
        result.shown = true;
        for (auto* action : popup->actions()) if (action->objectName() == name) {
            result.found = true; result.enabled = action->isEnabled(); result.target = action->data().toString();
            result.noShortcut = action->shortcut().isEmpty();
            if (invoke && action->isEnabled()) {
                if (beforeActivate) beforeActivate();
                QTest::mouseClick(popup, Qt::LeftButton, Qt::NoModifier, popup->actionGeometry(action).center());
                return;
            }
            break;
        }
        popup->close();
    });
    QContextMenuEvent event(QContextMenuEvent::Mouse, local, ui.canvas->viewport()->mapToGlobal(local));
    QApplication::sendEvent(ui.canvas->viewport(), &event); flush();
    require(result.shown, "Canvas did not open its context menu");
    return result;
}
inline AnnotationRecord sourceRecord(const QString& id = "source-reagent", const QString& owner = "1:left") {
    AnnotationRecord record{id, AnnotationKind::Reagent, AnnotationStyle::PlainText,
        AnnotationOwnerKind::Stream, owner, {0, 37}, true, true};
    record.text = "CuSO_4 + SO_4^{2-}"; record.dosage = "120"; record.dosageUnit = "g/t"; record.note = "Test note";
    return record;
}
inline void copy(Ui& ui, const QString& id) {
    auto* item = ui.reagent(id); require(item, "Missing reagent to copy");
    const auto result = menu(ui, item->mapToScene(item->boundingRect().center()), "copyReagentAnnotationAction");
    require(result.found && result.enabled && result.noShortcut, "Missing right-menu-only copy action");
}
inline QString paste(Ui& ui, QPointF point, const QString& stream) {
    const auto result = menu(ui, point, "pasteReagentAnnotationAction");
    require(result.found && result.enabled && result.noShortcut && result.target == stream,
            "Paste menu resolved the wrong stream: expected " + stream + ", got " + result.target
                + QString(" (found %1, enabled %2 at %3,%4)").arg(result.found).arg(result.enabled).arg(point.x()).arg(point.y()));
    const auto id = ui.selectedReagent(); require(!id.isEmpty(), "Pasted reagent was not selected");
    require(ui.document->annotationRecord(id).ownerId == stream, "Paste changed the wrong stream owner");
    return id;
}
inline void sameContent(const AnnotationRecord& source, const AnnotationRecord& copy) {
    require(copy.id != source.id && copy.kind == AnnotationKind::Reagent && copy.ownerKind == AnnotationOwnerKind::Stream
        && copy.text == source.text && copy.dosage == source.dosage && copy.dosageUnit == source.dosageUnit
        && copy.note == source.note && copy.style == source.style, "Reagent copy lost its content or identity");
}
inline int count(const FlowsheetDocument& document) {
    int result = 0; for (const auto& record : document.annotationRecords()) result += record.kind == AnnotationKind::Reagent; return result;
}
}
