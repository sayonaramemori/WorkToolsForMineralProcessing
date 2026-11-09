#include "app/MainWindow.h"
#include "adapters/CanvasTopologyBuilder.h"
#include "document/FlowsheetDocument.h"
#include "editor/CanvasView.h"
#include "editor/FlowsheetScene.h"
#include "graphics/FlotationGeometry.h"
#include "graphics/items/FeedJunctionItem.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"
#include "services/FlowGroupClipboard.h"
#include "services/FlowsheetCalculationService.h"
#include "services/ProjectSerializer.h"
#include "services/ProjectUndoManager.h"
#include "services/SvgExporter.h"

#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QXmlStreamReader>
#include <cmath>
#include <stdexcept>

using namespace afs;

void runResizeInteractionChecks(const QString& directory);
void runCrossingBridgeChecks(const QString& directory);

namespace {
void require(bool condition, const QString& message) {
    if (!condition) throw std::runtime_error(message.toStdString());
}

FlotationUnitItem* unitById(FlowsheetScene& scene, const QString& id) {
    for (auto* item : scene.items())
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item); unit && unit->unit().id == id) return unit;
    return nullptr;
}

void measure(FlowsheetDocument& document, const QString& id, double mass, double grade) {
    document.setDryMass(id, mass);
    document.setGradePercent(id, grade);
}

void checkBalance(FlowsheetScene& scene, FlowsheetDocument& document,
                  const QString& inlet, const QString& outlet) {
    auto result = FlowsheetCalculationService::calculate(scene, document);
    require(result.complete && result.fullySolved, "Ball-mill flowsheet must be fully solved");
    const auto input = result.values.value(inlet), output = result.values.value(outlet);
    require(result.values.contains(inlet) && result.values.contains(outlet)
        && std::abs(input.dryMass - output.dryMass) < 1e-6
        && std::abs(input.componentMass - output.componentMass) < 1e-6,
        "Ball mill must preserve mass and grade");
    document.setCalculationResult(std::move(result));
}

void testEquipmentCaptions(const QString& directory) {
    qInfo() << "Equipment icons have no captions beneath their symbols";
    for (int value = 0; value <= static_cast<int>(UnitKind::Thickener); ++value) {
        const auto kind = static_cast<UnitKind>(value);
        // Storage-pool names are inside the vessel, above the liquid wave.
        if (isStoragePool(kind) && !isCylindricalTank(kind)) continue;
        FlowsheetScene scene;
        scene.addItem(new FlotationUnitItem({"icon", {0, 0}, 180, 100, kind}));
        const QString path = directory + QString("/icon-%1.svg").arg(value);
        require(SvgExporter::exportScene(scene, path, "icon"), "Icon export failed");
        QFile svg(path);
        require(svg.open(QIODevice::ReadOnly), "Cannot read icon SVG");
        QXmlStreamReader xml(svg.readAll());
        while (!xml.atEnd()) {
            xml.readNext();
            require(!xml.isStartElement() || xml.name() != "text",
                    QString("Equipment kind %1 still draws a caption").arg(value));
        }
        require(!xml.hasError(), "Invalid icon SVG");
    }
}

void testBallMill(const QString& directory) {
    MainWindow window;
    window.resize(1500, 1000); window.show();
    QApplication::processEvents();
    auto* canvas = dynamic_cast<CanvasView*>(window.centralWidget());
    auto* scene = qobject_cast<FlowsheetScene*>(canvas->scene());
    auto* document = window.findChild<FlowsheetDocument*>();
    auto* undo = window.findChild<ProjectUndoManager*>();
    auto* add = window.findChild<QAction*>("addBallMillAction");
    require(add && add->isEnabled(), "Missing add-ball-mill menu action");
    add->trigger();
    FlotationUnitItem* mill = nullptr;
    FlotationUnitItem* upper = nullptr;
    for (auto* item : scene->items()) {
        auto* unit = dynamic_cast<FlotationUnitItem*>(item);
        if (!unit) continue;
        if (unit->unit().kind == UnitKind::BallMill) mill = unit;
        else upper = unit;
    }
    require(mill && upper && mill->products().size() == 1
        && mill->products().front()->side() == ProductSide::Right, "Ball mill must have one outlet");
    const QString millId = mill->unit().id;
    require(mill->shape().contains(QPointF(0, 0)) && !mill->shape().contains(QPointF(-31, -31)),
            "Ball-mill hit shape must follow its circle");
    require(mill->inputAnchorScenePosition() == mill->mapToScene(QPointF(0, -FlotationGeometry::BallMillRadius)),
            "Inlet must stop at the circle boundary");
    auto* outlet = mill->products().front();
    require(outlet->sourceAnchorScenePosition() == mill->mapToScene(QPointF(0, FlotationGeometry::BallMillRadius)),
            "Outlet must start at the bottom of the circle");
    require(outlet->setTerminalLength(30), "Could not shorten outlet");
    require(outlet->unconnectedEndScenePosition() == outlet->sourceAnchorScenePosition() + QPointF(0, 30),
            "Shortest outlet arrow must extend downwards outside the circle");
    outlet->setTerminalLength(std::nullopt);
    canvas->centerOn(mill);
    QApplication::processEvents();
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::NoModifier,
                      canvas->mapFromScene(mill->mapToScene(QPointF(12, 12))));
    require(mill->isSelected(), "Circle could not be selected with the mouse");

    qInfo() << "Direct connections and one-in/one-out balance";
    upper->setPos(-300, -200); mill->setPos(250, 180);
    auto* lower = new FlotationUnitItem({"downstream", {700, 500}});
    scene->addItem(lower);
    require(scene->connectProductDirect(upper->products().back(), mill->inputLine())
        && scene->connectProductDirect(mill->products().front(), lower->inputLine()), "Ball-mill connections failed");
    require(upper->products().back()->mapToScene(upper->products().back()->path().currentPosition())
        == mill->inputAnchorScenePosition(), "Connected path crossed the circle");
    const auto start = outlet->path().elementAt(0);
    require(outlet->mapToScene(QPointF(start.x, start.y)) == outlet->sourceAnchorScenePosition(),
            "Connected outlet did not start at circle bottom");
    const QPointF lowerPosition = lower->pos();
    require(mill->adjustWidth(40) && lower->pos() == lowerPosition,
            "Changing layout width must not move the centered outlet or downstream equipment");
    const auto snapshot = CanvasTopologyBuilder::build(*scene);
    const auto* node = snapshot.graph.storagePoolNode(millId);
    require(node && !node->terminal
        && snapshot.graph.streamsFrom(millId, topology::PortKind::PoolOutput).size() == 1,
        "Ball mill was treated as a two-product separator");
    measure(*document, upper->products().front()->streamId(), 30, 2);
    measure(*document, "downstream:left", 40, 3);
    measure(*document, "downstream:right", 30, 1);
    checkBalance(*scene, *document, upper->products().back()->streamId(), mill->products().front()->streamId());
    scene->clearSelection(); mill->setSelected(true);
    require(mill->isSelected() && !window.findChild<QWidget*>("resultDetails"),
            "Selecting equipment restored the removed result panel");

    qInfo() << "Recycle, project round trip and clipboard retain the new unit";
    auto* junction = scene->connectRecycle(lower->products().back(), mill->inputLine(), "mill-recycle");
    require(junction && junction->outputAnchor().y() < mill->inputAnchorScenePosition().y(),
            "Recycle inlet was not anchored above the circle");
    checkBalance(*scene, *document, junction->outputStreamId(), mill->products().front()->streamId());
    require(mill->products().front()->setTerminalLength(220), "Ball-mill route length could not be edited");
    const auto bytes = ProjectSerializer::serialize(*scene, *document);
    require(bytes.contains("ball-mill"), "Project did not store ball-mill kind");
    FlowsheetScene restored;
    FlowsheetDocument restoredDocument;
    QString error;
    require(ProjectSerializer::deserialize(restored, restoredDocument, bytes, &error), error);
    auto* restoredMill = unitById(restored, millId);
    require(restoredMill && restoredMill->unit().kind == UnitKind::BallMill
        && restoredMill->products().size() == 1 && restoredMill->products().front()->terminalLength() == 220,
        "Project round trip lost ball-mill data");
    {
        FlowsheetScene mergedScene;
        auto* a = new FlotationUnitItem({"a", {-300, -200}});
        auto* b = new FlotationUnitItem({"b", {300, -200}});
        auto* target = new FlotationUnitItem({"mill", {0, 300}, 180, 100, UnitKind::BallMill});
        mergedScene.addItem(a); mergedScene.addItem(b); mergedScene.addItem(target);
        auto* merged = mergedScene.mergeProducts(a->products().front(), b->products().front(), "merged");
        require(merged && mergedScene.connectMergeDirect(merged, target->inputLine())
            && merged->outputEndPosition() == target->inputAnchorScenePosition(),
            "Merged input did not stop at the ball-mill circle");
    }
    scene->clearSelection(); mill->setSelected(true);
    FlowGroupClipboard clipboard;
    require(clipboard.copy(*scene, *document).units == 1, "Ball mill could not be copied");
    require(clipboard.paste(*scene, *document, [] { return QString("mill-copy"); }).units == 1,
            "Ball mill could not be pasted");
    require(unitById(*scene, "mill-copy")->unit().kind == UnitKind::BallMill,
            "Pasted ball mill changed kind");

    qInfo() << "Circle is exported as vector geometry and menu insertion can be undone";
    const QString svgPath = directory + "/ball-mill.svg";
    require(SvgExporter::exportScene(restored, svgPath, "球磨机"), "SVG export failed");
    QFile svg(svgPath);
    require(svg.open(QIODevice::ReadOnly), "Cannot read SVG");
    QXmlStreamReader xml(svg.readAll());
    bool circle = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == "ellipse")
            circle = circle || (xml.attributes().value("rx").toDouble() == restoredMill->symbolWidth() / 2.0
                && xml.attributes().value("ry").toDouble() == restoredMill->symbolWidth() / 2.0);
        if (xml.isStartElement() && xml.name() == "circle")
            circle = circle || xml.attributes().value("r").toDouble() == restoredMill->symbolWidth() / 2.0;
    }
    require(!xml.hasError() && circle, "Ball-mill SVG lost its circle");
    undo->initialize();
    add->trigger();
    const int beforeUndo = CanvasTopologyBuilder::build(*scene).graph.nodeIds().size();
    undo->undo();
    require(CanvasTopologyBuilder::build(*scene).graph.nodeIds().size() == beforeUndo - 1,
            "Undo did not remove new ball mill");
    undo->redo();
    require(CanvasTopologyBuilder::build(*scene).graph.nodeIds().size() == beforeUndo,
            "Redo did not restore new ball mill");
}
} // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("AutoFlotationSheetTests");
    QCoreApplication::setApplicationName("BallMillInteraction");
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
    try {
        testEquipmentCaptions(directory.path());
        testBallMill(directory.path());
        runResizeInteractionChecks(directory.path());
        runCrossingBridgeChecks(directory.path());
        qInfo() << "All ball mill interaction checks passed";
    } catch (const std::exception& error) {
        qCritical().noquote() << error.what(); return 1;
    }
    return 0;
}
