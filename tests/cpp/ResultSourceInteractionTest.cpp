#include "app/MainWindow.h"
#include "annotations/AnnotationItem.h"
#include "annotations/AnnotationManager.h"
#include "document/FlowsheetDocument.h"
#include "editor/CanvasView.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FlotationUnitItem.h"
#include "services/FlowsheetCalculationService.h"
#include "services/ProjectSerializer.h"
#include "services/ProjectUndoManager.h"
#include "services/SvgExporter.h"

#include <QAction>
#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QDebug>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QInputDialog>
#include <QFile>
#include <QImage>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QXmlStreamReader>
#include <cmath>
#include <stdexcept>

using namespace afs;

namespace {
void require(bool condition, const QString& message) {
    if (!condition) throw std::runtime_error(message.toStdString());
}

AnnotationItem* resultCard(FlowsheetScene& scene, const QString& streamId) {
    for (auto* item : scene.items())
        if (auto* annotation = dynamic_cast<AnnotationItem*>(item);
            annotation && annotation->record().id == "result:" + streamId) return annotation;
    throw std::runtime_error("Missing result card: " + streamId.toStdString());
}

void expectLine(FlowsheetScene& scene, const QString& streamId,
                const QString& prefix, const QString& suffix) {
    auto* card = resultCard(scene, streamId);
    require(card->isVisible(), "Result card is hidden: " + streamId);
    for (const auto& line : card->text().split('\n')) {
        if (!line.startsWith(prefix)) continue;
        require(line.endsWith(suffix), "Unexpected metric text: " + line);
        return;
    }
    require(false, "Missing metric: " + prefix + " in " + card->text());
}

void expectEmphasis(FlowsheetScene& scene, const QString& streamId, bool emphasized) {
    const auto* card = resultCard(scene, streamId);
    require(card->isVisible() && card->calculatedResultEmphasized() == emphasized,
            "Wrong result border emphasis: " + streamId);
    require(!card->text().contains("[计算]") && !card->text().contains("[实测]")
        && !card->text().contains("[实测·协调]"), "Result card must not contain source labels");
}

QImage renderCard(AnnotationItem& card) {
    const QRect bounds = card.boundingRect().toAlignedRect();
    QImage image(bounds.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.translate(-bounds.topLeft());
    card.paint(&painter, nullptr, nullptr);
    return image;
}

void calculate(FlowsheetScene& scene, FlowsheetDocument& document) {
    auto result = FlowsheetCalculationService::calculate(scene, document);
    require(result.complete && result.fullySolved, "Fixture must be fully solved");
    document.setCalculationResult(std::move(result));
}

void observe(FlowsheetDocument& document, const QString& id, double mass, double cu, double zn) {
    document.setDryMass(id, mass);
    document.setGradePercent(id, "cu", cu);
    document.setGradePercent(id, "zn", zn);
}

void editBorderWidth(QAction& action, double width, bool accept) {
    bool edited = false;
    QTimer::singleShot(0, [&] {
        auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
        if (!dialog) return;
        auto* input = dialog->findChild<QDoubleSpinBox*>();
        if (!input) { dialog->reject(); return; }
        input->selectAll();
        QTest::keyClicks(input, QString::number(width, 'f', 1));
        edited = std::abs(input->value() - width) < 1e-6;
        if (accept) dialog->accept();
        else dialog->reject();
    });
    action.trigger();
    require(edited, "Border width dialog did not accept numeric input");
}

void expectUnclippedBorder(AnnotationItem& card) {
    const QImage image = renderCard(card);
    for (int x = 0; x < image.width(); ++x)
        require(image.pixelColor(x, 0).alpha() == 0
            && image.pixelColor(x, image.height() - 1).alpha() == 0, "Vertical border extent was clipped");
    for (int y = 0; y < image.height(); ++y)
        require(image.pixelColor(0, y).alpha() == 0
            && image.pixelColor(image.width() - 1, y).alpha() == 0, "Horizontal border extent was clipped");
}

void testSources(const QString& directory) {
    MainWindow window;
    window.resize(1600, 1000); window.show();
    QApplication::processEvents();
    auto* canvas = dynamic_cast<CanvasView*>(window.centralWidget());
    auto* scene = qobject_cast<FlowsheetScene*>(canvas->scene());
    auto* document = window.findChild<FlowsheetDocument*>();
    auto* undo = window.findChild<ProjectUndoManager*>();
    auto* annotations = window.findChild<AnnotationManager*>();
    auto* widthAction = window.findChild<QAction*>("calculatedResultBorderWidthAction");
    require(widthAction, "Missing calculated-result border width action");
    auto* sourceAction = window.findChild<QAction*>("distinguishValueSourcesAction");
    require(sourceAction && sourceAction->isCheckable() && sourceAction->isChecked(),
            "Value source setting must be enabled by default");
    bool inDisplayMenu = false;
    for (auto* menu : window.findChildren<QMenu*>())
        if (menu->title() == "显示设置" && menu->actions().contains(sourceAction)) inDisplayMenu = true;
    require(inDisplayMenu, "Value source switch is missing from display settings");
    QString unitId;
    for (auto* item : scene->items())
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item)) unitId = unit->unit().id;
    require(!unitId.isEmpty(), "Missing initial unit");
    const QString left = unitId + ":left", right = unitId + ":right", feed = unitId + ":feed";
    document->setComponents({{"cu", "Cu"}, {"zn", "Zn"}});
    observe(*document, left, 30, 2, 4);
    observe(*document, right, 70, 1, 2);
    // This partial input is ignored by the solver, so the displayed 100 is derived.
    document->setDryMass(feed, 999);
    auto settings = document->annotationTextSettings();
    settings.showOverallYield = settings.showOverallRecovery = settings.showProductName = true;
    document->setAnnotationTextSettings(settings);
    document->setProductName(left, "精矿");
    calculate(*scene, *document);

    qInfo() << "Measured and derived values, multiple components and partial inputs";
    expectLine(*scene, left, "质量", "30.00 g");
    expectLine(*scene, left, "品位 Cu", "2.00 %");
    expectLine(*scene, left, "品位 Zn", "4.00 %");
    expectLine(*scene, feed, "质量", "100.00 g");
    expectLine(*scene, feed, "品位 Cu", "1.30 %");
    expectLine(*scene, feed, "品位 Zn", "2.60 %");
    expectLine(*scene, left, "产率", "%");
    expectLine(*scene, left, "回收率 Cu", "%");
    expectLine(*scene, left, "产品", "精矿");
    expectEmphasis(*scene, left, false);
    expectEmphasis(*scene, right, false);
    expectEmphasis(*scene, feed, true);

    qInfo() << "Display toggle updates immediately without recalculation or moving cards";
    auto* card = resultCard(*scene, left);
    card->setPos(card->pos() + QPointF(20, 15));
    const auto position = card->pos();
    const auto values = document->calculationResult()->values;
    const auto snapshot = ProjectSerializer::serialize(*scene, *document);
    const auto measuredImage = renderCard(*card);
    auto* computedCard = resultCard(*scene, feed);
    const auto computedImage = renderCard(*computedCard);
    const auto computedText = computedCard->text();
    QSignalSpy calculations(document, &FlowsheetDocument::calculationChanged);
    QSignalSpy projectChanges(document, &FlowsheetDocument::projectChanged);
    sourceAction->trigger();
    require(!window.findChild<AnnotationManager*>()->distinguishValueSources() && !sourceAction->isChecked(),
            "Toggle did not update display preference");
    expectEmphasis(*scene, left, false);
    expectEmphasis(*scene, feed, false);
    require(renderCard(*card) == measuredImage && renderCard(*computedCard) != computedImage
        && computedCard->text() == computedText,
        "Source switch must change only the calculated card's border, not text or layout");
    require(card->pos() == position && calculations.isEmpty()
        && document->calculationResult()->values.value(left).dryMass == values.value(left).dryMass,
        "Changing presentation changed position or calculation");
    require(projectChanges.isEmpty() && ProjectSerializer::serialize(*scene, *document) == snapshot
        && !snapshot.contains("distinguishValueSources"), "Display preference changed project data");
    {
        MainWindow reopened;
        auto* preference = reopened.findChild<QAction*>("distinguishValueSourcesAction");
        require(preference && !preference->isChecked(), "New window did not restore local preference");
    }

    qInfo() << "Custom border width, cancelled edits and selected wide borders";
    sourceAction->trigger();
    const auto beforeWidthChange = ProjectSerializer::serialize(*scene, *document);
    const auto normalCardImage = renderCard(*card);
    const auto normalCardPosition = card->pos();
    const auto wideCardPosition = computedCard->pos();
    editBorderWidth(*widthAction, 8.0, true);
    require(annotations->calculatedResultBorderWidth() == 8.0
        && computedCard->calculatedResultBorderWidth() == 8.0, "Custom width did not reach result cards");
    expectUnclippedBorder(*computedCard);
    computedCard->setSelected(true);
    expectUnclippedBorder(*computedCard);
    computedCard->setSelected(false);
    editBorderWidth(*widthAction, 5.5, true);
    const auto customBorderImage = renderCard(*computedCard);
    editBorderWidth(*widthAction, 7.0, false);
    require(annotations->calculatedResultBorderWidth() == 5.5
        && renderCard(*computedCard) == customBorderImage, "Cancelled dialog changed border width");
    require(renderCard(*card) == normalCardImage && card->pos() == normalCardPosition
        && computedCard->pos() == wideCardPosition && computedCard->text() == computedText,
        "Width setting changed measured cards, text or positions");
    require(ProjectSerializer::serialize(*scene, *document) == beforeWidthChange
        && projectChanges.isEmpty() && calculations.isEmpty(), "Width setting modified project or calculation");
    sourceAction->trigger();
    {
        MainWindow reopened;
        require(reopened.findChild<AnnotationManager*>()->calculatedResultBorderWidth() == 5.5,
                "New window did not restore local border width");
    }

    qInfo() << "Style editing preserves source switch and other metric settings";
    bool styleEdited = false;
    QTimer::singleShot(0, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (!dialog) return;
        for (auto* box : dialog->findChildren<QCheckBox*>())
            if (box->text() == "加粗") { box->setChecked(true); styleEdited = true; }
        dialog->accept();
    });
    for (auto* action : window.findChildren<QAction*>())
        if (action->text() == "标注样式") { action->trigger(); break; }
    require(styleEdited && !sourceAction->isChecked() && document->annotationTextSettings().bold
        && document->annotationTextSettings().showOverallYield
        && document->annotationTextSettings().showProductName, "Style dialog reset display settings");

    qInfo() << "Opening projects preserves local preference and toggling does not dirty a clean project";
    QString error;
    const QString path = directory + "/sources.afs.json";
    require(ProjectSerializer::save(*scene, *document, path, &error), error);
    sourceAction->trigger();
    QTimer::singleShot(0, [] {
        if (auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
            dialog->button(QMessageBox::Discard)->click();
    });
    require(window.openProject(path), "Saved project did not reopen");
    require(sourceAction->isChecked() && resultCard(*scene, feed)->calculatedResultEmphasized(),
            "Opening a project changed local preference");
    require(resultCard(*scene, feed)->calculatedResultBorderWidth() == 5.5,
            "Opening a project reset custom border width");
    require(!window.windowTitle().contains(" *"), "Loaded project must be clean");
    undo->initialize();
    sourceAction->trigger();
    require(!window.windowTitle().contains(" *") && undo->isClean(), "Display toggle dirtied the project");
    document->setProductName(left, "修改后的精矿名");
    undo->undo();
    require(!sourceAction->isChecked() && document->productName(left) == "精矿",
            "Undo changed local preference or failed to restore project data");
    undo->redo();
    require(!sourceAction->isChecked() && document->productName(left) == "修改后的精矿名",
            "Redo changed local preference or failed to restore project data");
    require(resultCard(*scene, feed)->calculatedResultBorderWidth() == 5.5,
            "Undo/redo reset custom border width");
    sourceAction->trigger();
    expectLine(*scene, left, "品位 Cu", "%");

    qInfo() << "A result card can mix a measured component and a calculated component";
    observe(*document, feed, 100, 1.3, 2.6);
    document->setGradePercent(left, "zn", std::nullopt);
    calculate(*scene, *document);
    expectLine(*scene, left, "质量", "30.00 g");
    expectLine(*scene, left, "品位 Cu", "2.00 %");
    expectLine(*scene, left, "品位 Zn", "4.00 %");
    expectEmphasis(*scene, left, true);

    qInfo() << "Only visible calculated mass/grade affects the border, not yield or recovery";
    settings = document->annotationTextSettings();
    settings.showGrade = false;
    document->setAnnotationTextSettings(settings);
    expectEmphasis(*scene, left, false);
    expectEmphasis(*scene, feed, false);
    settings.showGrade = true;
    settings.showDryMass = false;
    document->setAnnotationTextSettings(settings);
    expectEmphasis(*scene, left, true);
    settings.showGrade = false;
    document->setAnnotationTextSettings(settings);
    expectEmphasis(*scene, left, false);
    expectEmphasis(*scene, right, false);
    settings.showGrade = settings.showDryMass = true;
    document->setAnnotationTextSettings(settings);
    expectEmphasis(*scene, left, true);

    qInfo() << "Scenario changes use the current scenario's measurement sources";
    const int alternate = document->addScenario("另一组实测", false);
    require(document->setCurrentScenario(alternate), "Scenario did not switch");
    observe(*document, feed, 100, 1.3, 2.6);
    observe(*document, right, 70, 1, 2);
    calculate(*scene, *document);
    expectLine(*scene, left, "质量", "30.00 g");
    expectEmphasis(*scene, left, true);
    require(sourceAction->isChecked(), "Scenario switch changed project display setting");
    require(document->setCurrentScenario(0), "Original scenario did not restore");
    expectLine(*scene, left, "质量", "30.00 g");
    expectEmphasis(*scene, right, false);

    qInfo() << "Reconciled observations retain measured styling, derived grades retain emphasis";
    document->setCalculationMode(CalculationMode::DataReconciliation);
    document->setDryMass(feed, 103);
    calculate(*scene, *document);
    require(document->calculationResult()->reconciled, "Fixture must reconcile its observations");
    expectLine(*scene, left, "质量", "g");
    expectLine(*scene, left, "品位 Cu", "%");
    expectLine(*scene, left, "品位 Zn", "%");
    expectLine(*scene, left, "产率", "%");
    expectEmphasis(*scene, left, true);
    expectEmphasis(*scene, right, false);
    expectEmphasis(*scene, feed, false);
    require(std::abs(document->calculationResult()->values.value(feed).dryMass - 103) > 1e-6,
            "Fixture must actually adjust a measurement");

    qInfo() << "Border emphasis follows symbol/unit settings and exported graphics";
    settings = document->annotationTextSettings();
    settings.metricLabelMode = MetricLabelMode::Symbols;
    settings.massUnit = MassUnit::Kilogram;
    document->setAnnotationTextSettings(settings);
    expectLine(*scene, left, "m  ", "kg");
    expectLine(*scene, left, "β Zn", "%");
    expectEmphasis(*scene, left, true);
    const QString svgPath = directory + "/sources.svg";
    require(SvgExporter::exportScene(*scene, svgPath, "来源标记"), "SVG export failed");
    QFile svg(svgPath);
    require(svg.open(QIODevice::ReadOnly), "Cannot read SVG export");
    // Font fallback can split a label into multiple SVG text elements.
    QXmlStreamReader xml(svg.readAll());
    QString svgText;
    bool thickBorder = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()
            && std::abs(xml.attributes().value("stroke-width").toDouble() - 5.5) < 1e-6)
            thickBorder = true;
        if (xml.isStartElement() && xml.name() == "text") svgText += xml.readElementText();
    }
    require(!xml.hasError() && thickBorder && !svgText.contains("[实测") && !svgText.contains("[计算]"),
            "Export must retain border emphasis without source labels");
    sourceAction->trigger();
    expectEmphasis(*scene, left, false);
}
} // namespace

void runAnnotationTextStyleDialogChecks();

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("AutoFlotationSheetTests");
    QCoreApplication::setApplicationName("ResultSourceInteraction");
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
    try {
        testSources(directory.path());
        runAnnotationTextStyleDialogChecks();
        qInfo() << "All result source interaction checks passed";
    } catch (const std::exception& error) {
        qCritical().noquote() << error.what(); return 1;
    }
    return 0;
}
