#include "WorkspaceToolsTestSupport.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/ProductLineItem.h"
#include "graphics/items/FeedJunctionItem.h"
#include "services/ProjectSerializer.h"
#include <QComboBox>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <cmath>
using namespace afs;
using namespace afs::tests;
namespace {
void liveProperties() {
    ToolsUi ui; auto* root = unit(*ui.scene, "1"); ui.select(root);
    auto* step = ui.properties->findChild<QDoubleSpinBox*>("propertyStep");
    require(step && step->value() == 10, "Property increment does not default to 10");
    const auto initial = ProjectSerializer::serialize(*ui.scene, *ui.document);
    step->setValue(2.5); flush();
    require(ProjectSerializer::serialize(*ui.scene, *ui.document) == initial,
            "Changing property step modified project data");
    auto* input = ui.properties->findChild<QDoubleSpinBox*>("property_input");
    QPointer<QDoubleSpinBox> original = input;
    require(input->singleStep() == 2.5, "New step was not applied to existing fields");
    input->setFocus(); input->selectAll();
    QTest::keyClicks(input, "120"); flush();
    require(root->inputLine()->length() == 120 && original
        && ui.properties->findChild<QDoubleSpinBox*>("property_input") == original,
        "Typing did not immediately update geometry or replaced the editor");
    QTest::keyClicks(input, "."); flush();
    ui.scene->refreshConnections(); flush();
    require(input->text() == "120.", "Live refresh interrupted fractional input");
    QTest::keyClicks(input, "5"); flush();
    require(root->inputLine()->length() == 120.5 && QApplication::focusWidget()
        && (QApplication::focusWidget() == input || input->isAncestorOf(QApplication::focusWidget())),
        "Fractional input was not applied live or lost focus");
    QTest::keyClick(input, Qt::Key_Up); flush();
    require(root->inputLine()->length() == 123, "Up key did not apply the configured step immediately");
    QTest::keyClick(input, Qt::Key_Down); flush();
    require(root->inputLine()->length() == 120.5, "Down key did not apply the configured step immediately");
    QTest::mouseClick(input, Qt::LeftButton, Qt::NoModifier, QPoint(input->width() - 8, input->height() / 4)); flush();
    require(root->inputLine()->length() == 123 && original,
            "Increment button did not update immediately or its editor was replaced");
    auto* width = ui.properties->findChild<QDoubleSpinBox*>("property_width");
    width->setFocus(); width->selectAll(); QTest::keyClicks(width, "421"); flush();
    require(root->unit().width == 421, "Width still requires Enter or focus loss to apply");
    const auto beforeMinimum = root->inputLine()->length();
    step->setValue(0.01); input->setFocus(); QTest::keyClick(input, Qt::Key_Up); flush();
    require(std::abs(root->inputLine()->length() - beforeMinimum - 0.01) < 1e-9,
            "Fractional property step did not apply");
    preview(ui.window, "live-properties");
    ToolsUi another;
    require(another.properties->findChild<QDoubleSpinBox*>("propertyStep")->value() == 0.01,
            "Property step was not remembered for a new window");
    step->setValue(10);
}
void properties() {
    ToolsUi ui; auto* root = unit(*ui.scene, "1"); require(root, "Missing root unit");
    ui.select(root); ui.undo->initialize(); ui.number("width", 420);
    require(root->unit().width == 420, "Property width was not applied");
    ui.undo->undo(); flush(); root = unit(*ui.scene, "1"); require(root->unit().width == 360, "Property width did not undo");
    ui.undo->redo(); flush(); root = unit(*ui.scene, "1"); require(root->unit().width == 420, "Property width did not redo");
    ui.select(root); ui.number("input", 132); ui.number("output/1:left", 210);
    require(root->inputLine()->length() == 132 && root->products().front()->terminalLength() == 210,
        "Device line-length fields were not applied");
    preview(ui.window, "properties");
    ui.select(root->products().front());
    auto* name = ui.properties->findChild<QLineEdit*>("propertyProductName"); require(name, "No name property");
    name->setText("Concentrate"); QTest::keyClick(name, Qt::Key_Return); flush();
    require(ui.document->productName("1:left") == "Concentrate", "Property name was not stored");
    name = ui.properties->findChild<QLineEdit*>("propertyProductName"); name->clear(); QTest::keyClick(name, Qt::Key_Return); flush();
    require(ui.document->productName("1:left").isEmpty(), "Clearing property name did not remove export name");
    const auto clean = ProjectSerializer::serialize(*ui.scene, *ui.document);
    auto* unchanged = ui.properties->findChild<QDoubleSpinBox*>("property_length");
    unchanged->setFocus(); QTest::keyClick(unchanged, Qt::Key_Return); flush();
    require(ProjectSerializer::serialize(*ui.scene, *ui.document) == clean, "Unchanged property edit modified the project");
    auto* next = new FlotationUnitItem({"next", {600, 300}}); auto* third = new FlotationUnitItem({"third", {900, 500}});
    ui.scene->addItem(next); ui.scene->addItem(third);
    require(ui.scene->connectProductDirect(root->products().back(), next->inputLine()), "Could not connect property fixture");
    ui.select(root->products().back()); ui.number("length", -80);
    require(next->pos().y() - root->pos().y() == -80, "Property spacing did not support an upward connection");
    ui.number("length", 300); ui.number("routeY", 95);
    require(root->products().back()->manualRouteY() == 95, "Direct route property did not apply");
    auto* feed = ui.scene->connectRecycle(third->products().front(), next->inputLine());
    require(feed && ui.scene->connectRecycle(root->products().front(), next->inputLine()) == feed, "Could not add property branches");
    ui.select(feed); ui.number("routeX", 850);
    require(feed->manualRouteXs().value("third:left") == 850, "Branch corridor property did not apply");
    auto* branches = ui.properties->findChild<QComboBox*>("propertyBranch"); require(branches, "No branch selector");
    branches->setCurrentIndex(branches->findData("1:left")); flush(); ui.number("feedY", -50);
    require(feed->manualRouteYs().value("1:left") == -50, "Selected branch height did not apply");
    QTest::mouseClick(ui.properties->findChild<QPushButton*>("propertyResetRoute"), Qt::LeftButton); flush();
    require(!feed->manualRouteYs().contains("1:left") && feed->manualRouteXs().value("third:left") == 850,
        "Reset changed a different branch");
    ui.select(root); next->setSelected(true); flush();
    require(CanvasProperties::describe(*ui.scene).fields.isEmpty(), "Multi-selection exposes ambiguous fields");
    require(!CanvasProperties::apply(*ui.scene, "unit/1", "width", 700), "Stale property target edited another selection");
    FlotationUnit mill{"mill", {1200, 300}}; mill.kind = UnitKind::BallMill;
    auto* circle = new FlotationUnitItem(mill); ui.scene->addItem(circle); ui.select(circle); ui.number("width", 104);
    require(circle->unit().ballMillDiameter == 104, "Circle property does not resize circle");
    // Leave an editor focused at destruction to exercise teardown callbacks.
    ui.properties->findChild<QDoubleSpinBox*>("property_width")->setFocus();
}
}
void runObjectPropertyChecks() { liveProperties(); properties(); }
