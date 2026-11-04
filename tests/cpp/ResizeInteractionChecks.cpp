#include "app/MainWindow.h"
#include "document/FlowsheetDocument.h"
#include "editor/CanvasView.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FeedJunctionItem.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"
#include "services/FlowGroupClipboard.h"
#include "services/ProjectSerializer.h"
#include "services/ProjectUndoManager.h"
#include "services/SvgExporter.h"

#include <QAction>
#include <QApplication>
#include <QFile>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>
#include <QXmlStreamReader>
#include <stdexcept>

using namespace afs;

namespace {
void require(bool value, const QString& message) {
    if (!value) throw std::runtime_error(message.toStdString());
}
FlotationUnitItem* find(FlowsheetScene& scene, const QString& id) {
    for (auto* item : scene.items())
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item); unit && unit->unit().id == id) return unit;
    return nullptr;
}
void click(CanvasView& canvas, QGraphicsItem& item, QPointF local) {
    canvas.centerOn(&item); QApplication::processEvents();
    QTest::mouseClick(canvas.viewport(), Qt::LeftButton, Qt::NoModifier,
                      canvas.mapFromScene(item.mapToScene(local)));
    QApplication::processEvents();
    require(item.isSelected(), "Resize target was not selected by the mouse");
}
void resize(CanvasView& canvas, Qt::Key key = Qt::Key_Equal) {
    canvas.setFocus(); QApplication::processEvents();
    QTest::keyClick(canvas.viewport(), key, Qt::ControlModifier);
    QApplication::processEvents();
}
}

void runResizeInteractionChecks(const QString& directory) {
    qInfo() << "Inlet and circle resize shortcuts, anchors, undo/redo, copy, persistence and SVG geometry";
    MainWindow window;
    window.resize(1500, 1000); window.show(); window.activateWindow();
    QApplication::processEvents();
    auto* canvas = dynamic_cast<CanvasView*>(window.centralWidget());
    auto* scene = qobject_cast<FlowsheetScene*>(canvas->scene());
    auto* document = window.findChild<FlowsheetDocument*>();
    auto* undo = window.findChild<ProjectUndoManager*>();
    FlotationUnitItem* root = nullptr;
    for (auto* item : scene->items()) if (auto* unit = dynamic_cast<FlotationUnitItem*>(item)) root = unit;
    require(root, "Missing initial flotation unit");
    const auto rootId = root->unit().id;
    const auto originalPosition = root->pos();
    undo->initialize();
    click(*canvas, *root->inputLine(), {0, -36});
    resize(*canvas);
    require(root->inputLine()->length() == 112 && root->inputLine()->line().p1() == QPointF(0, -112)
        && root->inputLine()->line().p2() == QPointF(0, 0) && root->pos() == originalPosition,
        "Inlet shortcut did not extend just the flotation input line");
    require(scene->inputAtDropPosition(root->mapToScene({0, -105})) == root->inputLine(),
            "Extended inlet is not a connection drop target");
    undo->undo(); root = find(*scene, rootId);
    require(root && root->inputLine()->length() == 72, "Undo did not restore inlet length");
    undo->redo(); root = find(*scene, rootId);
    require(root && root->inputLine()->length() == 112, "Redo did not restore edited inlet length");
    click(*canvas, *root->inputLine(), {0, -56}); resize(*canvas, Qt::Key_Minus);
    require(root->inputLine()->length() == 72, "Inlet decrease shortcut did not work");
    resize(*canvas);

    window.findChild<QAction*>("addBallMillAction")->trigger();
    FlotationUnitItem* mill = nullptr;
    for (auto* item : scene->items())
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item); unit && unit->unit().kind == UnitKind::BallMill)
            mill = unit;
    require(mill, "Missing ball mill");
    const auto millId = mill->unit().id;
    click(*canvas, *mill, {12, 12});
    resize(*canvas);
    require(mill->symbolWidth() == 104 && mill->shape().contains({45, 0})
        && mill->boundingRect().contains({52, 0}), "Ball-mill shortcut did not enlarge its circle and hit area");
    resize(*canvas, Qt::Key_Minus);
    require(mill->symbolWidth() == 64 && !mill->shape().contains({45, 0}), "Ball-mill decrease shortcut did not work");
    undo->initialize(); resize(*canvas);
    undo->undo(); mill = find(*scene, millId); root = find(*scene, rootId);
    require(mill && mill->symbolWidth() == 64, "Undo did not restore circle size");
    undo->redo(); mill = find(*scene, millId); root = find(*scene, rootId);
    require(mill && mill->symbolWidth() == 104 && mill->inputLine()->length() == 40,
            "Redo did not restore resized circle and inlet");
    require(mill->inputAnchorScenePosition() == mill->mapToScene({0, -52})
        && mill->products().front()->sourceAnchorScenePosition() == mill->mapToScene({0, 52}),
        "Resized circle has stale inlet/outlet anchors");
    click(*canvas, *mill->inputLine(), {0, -72}); resize(*canvas);
    require(mill->inputLine()->length() == 80 && mill->symbolWidth() == 104,
            "Resizing ball-mill inlet changed the circle size");
    mill->setPos(300, 200); root->setPos(-300, -250);
    auto* downstream = new FlotationUnitItem({"resize-downstream", {600, 550}});
    scene->addItem(downstream);
    require(scene->connectProductDirect(root->products().back(), mill->inputLine())
        && scene->connectProductDirect(mill->products().front(), downstream->inputLine()),
        "Could not connect resized ball mill");
    const auto downstreamPosition = downstream->pos();
    click(*canvas, *mill, {12, 12}); resize(*canvas);
    auto* incoming = root->products().back();
    require(mill->symbolWidth() == 144 && downstream->pos() == downstreamPosition
        && incoming->mapToScene(incoming->path().currentPosition()) == mill->inputAnchorScenePosition()
        && mill->products().front()->sourceAnchorScenePosition() == mill->mapToScene({0, 72}),
        "Resizing connected circle did not refresh anchors or moved downstream equipment");
    auto* feed = scene->connectRecycle(downstream->products().back(), mill->inputLine());
    require(feed, "Could not connect recycle to resized inlet");
    const auto previousJunction = feed->junctionPosition();
    mill->adjustInputLength(40);
    require(feed->junctionPosition() == previousJunction - QPointF(0, 20),
            "Input resize did not refresh recycle junction geometry");
    scene->clearSelection(); mill->setSelected(true);
    FlowGroupClipboard clipboard;
    require(clipboard.copy(*scene, *document).units == 1, "Could not copy resized unit");
    require(clipboard.paste(*scene, *document, [] { return QString("resized-copy"); }).units == 1,
            "Could not paste resized unit");
    auto* copy = find(*scene, "resized-copy");
    require(copy && copy->symbolWidth() == 144 && copy->inputLine()->length() == 120,
            "Copy lost circle diameter or inlet length");
    const auto bytes = ProjectSerializer::serialize(*scene, *document);
    FlowsheetScene restored; FlowsheetDocument restoredDocument; QString error;
    require(ProjectSerializer::deserialize(restored, restoredDocument, bytes, &error), error);
    auto* restoredMill = find(restored, millId);
    require(restoredMill && restoredMill->symbolWidth() == 144 && restoredMill->inputLine()->length() == 120,
            "Project round trip lost resize geometry");
    require(find(restored, rootId)->inputLine()->length() == 112,
            "Project round trip lost flotation inlet length");
    const auto svgPath = directory + "/resized-circle.svg";
    require(SvgExporter::exportScene(restored, svgPath, "resize"), "Could not export resized circle");
    QFile svg(svgPath); require(svg.open(QIODevice::ReadOnly), "Could not read resized-circle SVG");
    QXmlStreamReader xml(svg.readAll()); bool circle = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == "ellipse")
            circle |= xml.attributes().value("rx").toDouble() == 72 && xml.attributes().value("ry").toDouble() == 72;
        if (xml.isStartElement() && xml.name() == "circle") circle |= xml.attributes().value("r").toDouble() == 72;
    }
    require(circle && !xml.hasError(), "Exported circle does not use the edited diameter");
    {
        FlowsheetScene aligned;
        auto* a = new FlotationUnitItem({"align-a", {-300, 0}});
        auto* b = new FlotationUnitItem({"align-b", {300, 0}});
        FlotationUnit target{"align-target", {700, 400}};
        target.kind = UnitKind::BallMill; target.ballMillDiameter = 144; target.inputLength = 120;
        auto* directTarget = new FlotationUnitItem(target);
        aligned.addItem(a); aligned.addItem(b); aligned.addItem(directTarget);
        require(aligned.connectProduct(a->products().back(), directTarget->inputLine(), true)
            && directTarget->inputLine()->startScenePosition() == a->products().back()->unconnectedEndScenePosition(),
            "Ctrl connection alignment ignored edited input length or circle radius");
        target.id = "align-merged";
        auto* mergedTarget = new FlotationUnitItem(target); aligned.addItem(mergedTarget);
        auto* merge = aligned.mergeProducts(a->products().front(), b->products().front());
        require(merge, "Could not create merge for alignment check");
        const auto output = merge->outputEndPosition();
        require(aligned.connectMerge(merge, mergedTarget->inputLine(), true)
            && mergedTarget->inputLine()->startScenePosition() == output,
            "Merged connection alignment ignored edited input geometry");
    }
    for (const auto& field : {QString("inputLength"), QString("ballMillDiameter")}) {
        for (const auto value : {QJsonValue(-1), QJsonValue(10000), QJsonValue("100"), QJsonValue(QJsonValue::Null)}) {
            auto object = QJsonDocument::fromJson(bytes).object(); auto units = object.value("units").toArray();
            for (int index = 0; index < units.size(); ++index) {
                auto unit = units[index].toObject();
                if (unit.value("id").toString() != millId) continue;
                unit.insert(field, value); units[index] = unit;
            }
            object.insert("units", units);
            require(!ProjectSerializer::deserialize(restored, restoredDocument, QJsonDocument(object).toJson(), &error),
                    "Invalid resize geometry was accepted: " + field);
        }
    }
    require(mill->adjustInputLength(-10000) && mill->inputLine()->length() == 30
        && mill->adjustInputLength(10000) && mill->inputLine()->length() == 5000,
        "Inlet length limits are not enforced");
    require(mill->adjustWidth(-10000) && mill->symbolWidth() == 40
        && mill->adjustWidth(10000) && mill->symbolWidth() == 720,
        "Circle size limits are not enforced");
}
