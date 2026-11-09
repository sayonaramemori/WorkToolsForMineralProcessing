#include "app/TerminalTableController.h"
#include "TerminalInputChecks.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/InputLineItem.h"
#include "graphics/items/ProductLineItem.h"
#include "services/ThemeService.h"
#include "ui/TerminalProductDock.h"
#include "ui/TerminalProductTableModel.h"
#include <QBrush>
#include <QGraphicsSimpleTextItem>

namespace afs::tests {
int runTerminalModelChecks(TerminalInputContext& context) {
    auto& scene = context.inputScene;
    auto* first = new FlotationUnitItem({"first", {0, 0}});
    auto* second = new FlotationUnitItem({"second", {400, 300}});
    scene.addItem(first);
    scene.addItem(second);

    auto initial = CanvasTopologyBuilder::build(scene);
    if (initial.terminalProducts.size() != 4) return 1;
    if (initial.graph.externalFeedStreams().size() != 2) return 2;

    auto* stream = first->products().at(1);
    if (stream->streamId() != "first:right") return 3;
    stream->setProductName("右侧产品");
    stream->setTextSettings({13, true, QColor("#225588")});
    auto* productLabel = dynamic_cast<QGraphicsSimpleTextItem*>(stream->childItems().value(0));
    if (!productLabel || productLabel->text() != "右侧产品" || !productLabel->isVisible()
        || productLabel->font().pointSize() != 13 || !productLabel->font().bold()
        || productLabel->brush().color() != QColor("#225588")) return 33;
    if (!scene.connectProduct(stream, second->inputLine())) return 4;
    auto& connected = context.connected;
    connected = CanvasTopologyBuilder::build(scene);
    if (connected.terminalProducts.size() != 3) return 5;
    if (connected.productStreams.size() != 4) return 36;
    if (connected.reportStreams.size() != 5
        || connected.reportStreams.front().streamId != "first:feed"
        || !connected.reportStreams.front().displayName.contains("主入料")) return 38;
    const auto* graphStream = connected.graph.stream("first:right");
    if (!graphStream || !graphStream->target || graphStream->target->nodeId != "second") return 6;

    auto& document = context.inputDocument;
    TerminalTableController session(document);
    TerminalProductTableModel model(session);
    session.setStreams(connected.terminalProducts);
    if (!model.data(model.index(0, TerminalProductTableModel::ProductNameColumn), Qt::DisplayRole).toString().isEmpty()
        || !model.data(model.index(0, TerminalProductTableModel::ProductNameColumn), Qt::EditRole).toString().isEmpty()
        || model.data(model.index(0, TerminalProductTableModel::NameColumn), Qt::DisplayRole).toString().isEmpty())
        return 80;
    if (model.data(model.index(0, TerminalProductTableModel::MassColumn),
                   Qt::BackgroundRole).isValid()
        || model.data(model.index(0, TerminalProductTableModel::GradeColumn),
                      Qt::BackgroundRole).isValid()) return 39;
    if (!model.setData(model.index(0, TerminalProductTableModel::MassColumn), "12.5", Qt::EditRole)) return 7;
    if (model.data(model.index(0, TerminalProductTableModel::MassColumn), Qt::BackgroundRole).isValid()
        || !model.data(model.index(0, TerminalProductTableModel::GradeColumn),
                       Qt::BackgroundRole).canConvert<QBrush>()) return 40;
    if (!model.setData(model.index(0, TerminalProductTableModel::GradeColumn), "8.2", Qt::EditRole)) return 8;
    if (model.data(model.index(0, TerminalProductTableModel::GradeColumn),
                   Qt::BackgroundRole).isValid()) return 41;
    if (!model.setData(model.index(0, TerminalProductTableModel::ProductNameColumn),
                       "精矿产品", Qt::EditRole)
        || document.productName(model.streamAt(0)->streamId) != "精矿产品") return 30;
    if (!model.setData(model.index(0, TerminalProductTableModel::ProductNameColumn),
                       "", Qt::EditRole)
        || !document.productName(model.streamAt(0)->streamId).isEmpty()
        || !model.data(model.index(0, TerminalProductTableModel::ProductNameColumn), Qt::DisplayRole).toString().isEmpty())
        return 31;
    if (model.setData(model.index(0, TerminalProductTableModel::GradeColumn), "101", Qt::EditRole)) return 9;
    const auto id = model.streamAt(0)->streamId;
    if (!document.measurement(id).complete()) return 10;
    document.setComponents({{"component-1", "Cu"}, {"component-2", "Zn"}});
    const int zincGradeColumn = model.gradeColumn("component-2");
    if (model.columnCount() != 6 || zincGradeColumn < 0
        || !model.flags(model.index(0, zincGradeColumn)).testFlag(Qt::ItemIsEditable)
        || !model.data(model.index(0, zincGradeColumn), Qt::BackgroundRole).canConvert<QBrush>())
        return 51;
    if (!model.setData(model.index(0, zincGradeColumn), "1.6", Qt::EditRole)
        || !document.measurement(id).completeFor(document.componentIds())) return 52;
    document.setCalculationMode(CalculationMode::DataReconciliation);
    if (model.columnCount() != 9
        || model.gradeColumn("component-1") != 4
        || model.gradeStdDevColumn("component-2") != 7
        || !model.setData(model.index(0, TerminalProductTableModel::MassStdDevColumn),
                          "0.25", Qt::EditRole)
        || !model.setData(model.index(0, model.gradeStdDevColumn("component-1")),
                          "0.05", Qt::EditRole)
        || document.measurement(id).dryMassStdDev != 0.25
        || document.measurement(id).gradeStdDev("component-1") != 0.05) return 80;
    document.setCalculationMode(CalculationMode::Strict);

    // In reconciliation mode, hovering a grade must report its grade correction,
    // rather than the dry-mass residual belonging to the same stream.
    FlowsheetDocument tooltipDocument;
    tooltipDocument.setDryMass("tooltip-stream", 100.0);
    tooltipDocument.setGradePercent("tooltip-stream", 1.0);
    tooltipDocument.setGradeStdDev("tooltip-stream", "component-1", 0.1);
    tooltipDocument.setCalculationMode(CalculationMode::DataReconciliation);
    topology::CalculationResult tooltipCalculation;
    tooltipCalculation.reconciled = true;
    tooltipCalculation.values.insert("tooltip-stream", {105.0, 1.26});
    tooltipCalculation.residuals.insert("tooltip-stream", {5.0, 0.26, 2.5, 2.6});
    tooltipCalculation.components["component-1"].values.insert("tooltip-stream", {105.0, 1.26});
    tooltipDocument.setCalculationResult(std::move(tooltipCalculation));
    TerminalTableController tooltipSession(tooltipDocument);
    TerminalProductTableModel tooltipModel(tooltipSession);
    tooltipSession.setStreams({{"tooltip-stream", "提示物流"}});
    const QString massTooltip = tooltipModel.data(
        tooltipModel.index(0, TerminalProductTableModel::MassColumn), Qt::ToolTipRole).toString();
    const QString gradeTooltip = tooltipModel.data(
        tooltipModel.index(0, tooltipModel.gradeColumn("component-1")), Qt::ToolTipRole).toString();
    if (!massTooltip.contains("质量修正 5") || !gradeTooltip.contains("品位修正 0.2")
        || gradeTooltip.contains("质量修正")) return 81;

    QSet<QString> editableIds;
    for (const auto& product : connected.reportStreams) editableIds.insert(product.streamId);
    session.setStreams(connected.reportStreams, editableIds);
    const int intermediateRow = model.rowForStreamId(stream->streamId());
    if (intermediateRow < 0
        || model.data(model.index(intermediateRow, TerminalProductTableModel::MassColumn),
                      Qt::BackgroundRole).isValid()
        || !model.flags(model.index(intermediateRow, TerminalProductTableModel::MassColumn))
            .testFlag(Qt::ItemIsEditable)
        || !model.flags(model.index(intermediateRow, TerminalProductTableModel::ProductNameColumn))
            .testFlag(Qt::ItemIsEditable)
        || !model.setData(model.index(intermediateRow, TerminalProductTableModel::MassColumn),
                          "99", Qt::EditRole)
        || !model.data(model.index(intermediateRow, TerminalProductTableModel::GradeColumn),
                       Qt::BackgroundRole).canConvert<QBrush>()) return 37;

    if (!scene.disconnectProduct(stream)) return 11;
    auto disconnected = CanvasTopologyBuilder::build(scene);
    if (disconnected.terminalProducts.size() != 4) return 12;
    if (!document.measurement(id).complete()) return 13;

    ThemeService::applyApplicationPalette(true);
    TerminalProductDock dock(session);
    dock.refreshAppearance();
    auto* panel = dock.findChild<QWidget*>("terminalProductPanel");
    if (!panel || !panel->styleSheet().contains("#ff151a21")) return 14;
    ThemeService::applyApplicationPalette(false);
    dock.refreshAppearance();
    if (!panel->styleSheet().contains("#fff4f4f4")
        || panel->styleSheet().contains("#ff151a21")) return 15;

    return 0;
}
}
