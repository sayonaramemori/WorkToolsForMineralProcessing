#include "app/MainWindow.h"
#include "adapters/CanvasTopologyBuilder.h"
#include "document/FlowsheetDocument.h"
#include "editor/CanvasView.h"
#include "editor/FlowsheetScene.h"
#include "graphics/ThickenerGeometry.h"
#include "graphics/items/FeedJunctionItem.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"
#include "services/FlowGroupClipboard.h"
#include "services/FlowsheetCalculationService.h"
#include "services/ProjectSerializer.h"
#include "services/ProjectUndoManager.h"
#include "services/RasterExporter.h"
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

namespace {
void require(bool condition, const QString& message) {
    if (!condition) throw std::runtime_error(message.toStdString());
}
FlotationUnitItem* find(FlowsheetScene& scene, const QString& id) {
    for (auto* item : scene.items())
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item); unit && unit->unit().id == id) return unit;
    return nullptr;
}
void click(CanvasView& canvas, QGraphicsItem& item, QPointF point) {
    canvas.centerOn(&item); QApplication::processEvents();
    QTest::mouseClick(canvas.viewport(), Qt::LeftButton, Qt::NoModifier,
                      canvas.mapFromScene(item.mapToScene(point)));
    require(item.isSelected(), "Thickener item could not be selected with mouse");
}
void widen(CanvasView& canvas) {
    canvas.setFocus(); QApplication::processEvents();
    QTest::keyClick(canvas.viewport(), Qt::Key_Equal, Qt::ControlModifier);
    QApplication::processEvents();
}
void testThickener(const QString& directory) {
    MainWindow window; window.resize(1500, 1000); window.show(); window.activateWindow();
    QApplication::processEvents();
    auto* canvas = dynamic_cast<CanvasView*>(window.centralWidget());
    auto* scene = qobject_cast<FlowsheetScene*>(canvas->scene());
    auto* document = window.findChild<FlowsheetDocument*>();
    auto* undo = window.findChild<ProjectUndoManager*>();
    auto* add = window.findChild<QAction*>("addThickenerAction");
    require(add && add->isEnabled() && add->text() == "添加浓密池", "Missing thickener menu action");
    undo->initialize(); add->trigger();
    FlotationUnitItem* thickener = nullptr; FlotationUnitItem* upstream = nullptr;
    for (auto* item : scene->items()) if (auto* unit = dynamic_cast<FlotationUnitItem*>(item)) {
        if (unit->unit().kind == UnitKind::Thickener) thickener = unit;
        else upstream = unit;
    }
    require(thickener && upstream, "Menu did not create thickener");
    const auto id = thickener->unit().id, upstreamId = upstream->unit().id;
    undo->undo(); require(!find(*scene, id), "Undo did not remove thickener");
    undo->redo(); thickener = find(*scene, id); upstream = find(*scene, upstreamId);
    require(thickener && thickener->unit().kind == UnitKind::Thickener, "Redo lost thickener type");
    require(thickener->products().size() == 1 && thickener->products().front()->streamId() == id + ":right",
            "Thickener must have exactly one stable output");
    require(thickener->shape().contains({0, 40}) && !thickener->shape().contains({80, 45})
        && thickener->boundingRect().contains(thickener->shape().boundingRect()),
        "Thickener selection shape does not follow the conical vessel");
    require(thickener->inputAnchorScenePosition() == thickener->mapToScene({0, ThickenerGeometry::Top})
        && thickener->products().front()->sourceAnchorScenePosition()
            == thickener->mapToScene({0, ThickenerGeometry::Bottom}), "Thickener ports are not top/bottom centered");
    click(*canvas, *thickener, {25, 0}); widen(*canvas);
    require(thickener->unit().width == 220, "Thickener width shortcut did not resize icon");
    click(*canvas, *thickener->inputLine(), {0, -68}); widen(*canvas);
    require(thickener->inputLine()->length() == 112, "Thickener input length shortcut failed");
    upstream->setPos(-300, -200); thickener->setPos(300, 200);
    require(scene->connectProductDirect(upstream->products().back(), thickener->inputLine()),
            "Thickener input connection failed");
    auto* incoming = upstream->products().back();
    require(incoming->mapToScene(incoming->path().currentPosition()) == thickener->inputAnchorScenePosition(),
            "Incoming line did not stop at vessel top");
    document->setDryMass(upstreamId + ":feed", 100); document->setGradePercent(upstreamId + ":feed", 2);
    document->setDryMass(upstreamId + ":left", 30); document->setGradePercent(upstreamId + ":left", 3);
    auto result = FlowsheetCalculationService::calculate(CanvasTopologyBuilder::build(*scene), *document);
    const auto input = result.values.value(incoming->streamId()), output = result.values.value(id + ":right");
    require(result.complete && result.fullySolved && std::abs(output.dryMass - 70) < 1e-6
        && std::abs(output.dryMass - input.dryMass) < 1e-6
        && std::abs(output.componentMass - input.componentMass) < 1e-6,
        "Thickener must retain dry mass and component mass through its single output");
    const auto topology = CanvasTopologyBuilder::build(*scene);
    require(topology.graph.storagePoolNode(id) && !topology.graph.storagePoolNode(id)->terminal
        && topology.graph.streamsFrom(id, topology::PortKind::PoolOutput).size() == 1,
        "Thickener must use one-in/one-out topology");
    document->setCalculationResult(std::move(result));
    scene->clearSelection(); thickener->setSelected(true);
    require(thickener->isSelected() && !window.findChild<QWidget*>("resultDetails"),
            "Selecting equipment restored the removed result panel");
    auto* downstream = new FlotationUnitItem({"downstream", {600, 600}}); scene->addItem(downstream);
    require(scene->connectProductDirect(thickener->products().front(), downstream->inputLine()),
            "Thickener output connection failed");
    const auto downstreamPosition = downstream->pos();
    click(*canvas, *thickener, {25, 0}); widen(*canvas);
    require(thickener->unit().width == 260 && downstream->pos() == downstreamPosition,
            "Resizing centered-output vessel moved downstream equipment");
    require(scene->connectRecycle(downstream->products().front(), thickener->inputLine()),
            "Thickener did not accept recycle input");
    require(thickener->products().front()->setTerminalLength(180), "Could not set thickener outlet length");
    scene->clearSelection(); upstream->setSelected(true); thickener->setSelected(true); downstream->setSelected(true);
    FlowGroupClipboard clipboard; int index = 0;
    require(clipboard.copy(*scene, *document).units == 3, "Could not copy thickener group");
    const auto pasted = clipboard.paste(*scene, *document, [&] { return QString("clone-%1").arg(++index); });
    require(pasted.units == 3 && pasted.feeds == 1, "Copy lost thickener connections or recycle");
    FlotationUnitItem* clone = nullptr;
    for (auto* item : scene->selectedItems())
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item); unit && unit->unit().kind == UnitKind::Thickener)
            clone = unit;
    require(clone && clone->pos() == thickener->pos() + QPointF(40, 40)
        && clone->unit().width == 260 && clone->inputLine()->length() == 112
        && clone->products().front()->terminalLength() == 180, "Copy lost thickener geometry");
    const auto bytes = ProjectSerializer::serialize(*scene, *document);
    require(bytes.contains("\"thickener\""), "Project did not serialize thickener type");
    FlowsheetScene restored; FlowsheetDocument restoredDocument; QString error;
    require(ProjectSerializer::deserialize(restored, restoredDocument, bytes, &error), error);
    auto* restoredUnit = find(restored, id);
    require(restoredUnit && restoredUnit->unit().kind == UnitKind::Thickener
        && restoredUnit->products().size() == 1 && restoredUnit->inputLine()->length() == 112
        && restoredUnit->products().front()->terminalLength() == 180, "Project round trip lost thickener data");

    FlowsheetScene iconScene; auto icon = thickener->unit(); icon.position = {0, 0};
    iconScene.addItem(new FlotationUnitItem(icon));
    const auto svgPath = directory + "/thickener.svg";
    require(SvgExporter::exportScene(iconScene, svgPath, "浓密池"), "Could not export thickener SVG");
    QFile svg(svgPath); require(svg.open(QIODevice::ReadOnly), "Could not read SVG");
    QXmlStreamReader xml(svg.readAll()); bool rim = false;
    while (!xml.atEnd()) {
        xml.readNext();
        require(!xml.isStartElement() || xml.name() != "text", "Thickener icon has an unwanted caption");
        if (xml.isStartElement() && xml.name() == "ellipse")
            rim |= xml.attributes().value("rx").toDouble() == 130 && xml.attributes().value("ry").toDouble() == 10;
    }
    require(!xml.hasError() && rim, "Thickener SVG lost its elliptical vessel rim");
    const auto preview = qEnvironmentVariable("AFS_THICKENER_PREVIEW_PATH");
    if (!preview.isEmpty()) require(RasterExporter::exportScene(iconScene, preview, "png", 3), "Preview export failed");
    {
        FlowsheetScene mergeScene;
        auto* a = new FlotationUnitItem({"a", {-300, -200}});
        auto* b = new FlotationUnitItem({"b", {300, -200}});
        auto* target = new FlotationUnitItem({"thickener", {0, 300}, 180, 100, UnitKind::Thickener});
        mergeScene.addItem(a); mergeScene.addItem(b); mergeScene.addItem(target);
        auto* merge = mergeScene.mergeProducts(a->products().front(), b->products().front());
        require(merge && mergeScene.connectMergeDirect(merge, target->inputLine())
            && merge->outputEndPosition() == target->inputAnchorScenePosition(), "Merge output missed thickener inlet");
    }
}
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("AutoFlotationSheetTests");
    QCoreApplication::setApplicationName("ThickenerInteraction");
    QTemporaryDir directory; if (!directory.isValid()) return 1;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
    try { testThickener(directory.path()); qInfo() << "All thickener interaction checks passed"; }
    catch (const std::exception& error) { qCritical().noquote() << error.what(); return 1; }
    return 0;
}
