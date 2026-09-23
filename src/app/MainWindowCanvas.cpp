#include "app/MainWindow.h"

#include "core/FlotationUnit.h"
#include "document/FlowsheetDocument.h"
#include "editor/CanvasActions.h"
#include "editor/CanvasView.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FlotationUnitItem.h"

#include <QGraphicsScene>
#include <QInputDialog>
#include <QSet>
#include <QStatusBar>

namespace afs {

namespace {
QPointF canvasCenter(const CanvasView& view) {
    return view.mapToScene(view.viewport()->rect().center());
}

void addUnit(FlowsheetScene& scene, FlotationUnit unit) {
    scene.addItem(new FlotationUnitItem(std::move(unit)));
    scene.notifyTopologyChanged();
}
} // namespace

void MainWindow::addFlotationUnit() {
    addUnit(*static_cast<FlowsheetScene*>(m_scene),
            {takeNextUnitId(), canvasCenter(*m_view)});
}

void MainWindow::addMagneticSeparator(UnitKind kind) {
    FlotationUnit unit{takeNextUnitId(), canvasCenter(*m_view)};
    unit.kind = kind;
    unit.bodyHeight = 100.0;
    addUnit(*static_cast<FlowsheetScene*>(m_scene), std::move(unit));
}

void MainWindow::addGravitySeparator(UnitKind kind) {
    FlotationUnit unit{takeNextUnitId(), canvasCenter(*m_view)};
    unit.kind = kind;
    unit.bodyHeight = 100.0;
    addUnit(*static_cast<FlowsheetScene*>(m_scene), std::move(unit));
}

void MainWindow::addSizingSeparator(UnitKind kind) {
    FlotationUnit unit{takeNextUnitId(), canvasCenter(*m_view)};
    unit.kind = kind;
    unit.bodyHeight = 100.0;
    addUnit(*static_cast<FlowsheetScene*>(m_scene), std::move(unit));
}

void MainWindow::addStoragePool(UnitKind kind) {
    FlotationUnit unit{takeNextUnitId(), canvasCenter(*m_view)};
    unit.kind = kind;
    unit.width = 180.0;
    addUnit(*static_cast<FlowsheetScene*>(m_scene), std::move(unit));
}

void MainWindow::addThreeProductUnit() {
    FlotationUnit unit{takeNextUnitId(), canvasCenter(*m_view)};
    unit.kind = UnitKind::ThreeProductFlotation;
    addUnit(*static_cast<FlowsheetScene*>(m_scene), std::move(unit));
}

void MainWindow::addThreeProductScreening() {
    FlotationUnit unit{takeNextUnitId(), canvasCenter(*m_view)};
    unit.kind = UnitKind::ThreeProductScreening;
    unit.bodyHeight = 100.0;
    addUnit(*static_cast<FlowsheetScene*>(m_scene), std::move(unit));
}

void MainWindow::addThreeProductDemediumScreen() {
    FlotationUnit unit{takeNextUnitId(), canvasCenter(*m_view)};
    unit.kind = UnitKind::ThreeProductDemediumScreen;
    unit.bodyHeight = 100.0;
    addUnit(*static_cast<FlowsheetScene*>(m_scene), std::move(unit));
}

void MainWindow::addBinarySplitter() {
    bool accepted = false;
    const double leftPercent = QInputDialog::getDouble(
        this, tr("添加二分流器"), tr("左支路比例（右支路自动补足至 100%）"),
        50.0, 0.1, 99.9, 1, &accepted);
    if (!accepted) return;

    FlotationUnit unit{takeNextUnitId(), canvasCenter(*m_view)};
    unit.kind = UnitKind::BinarySplitter;
    unit.leftSplitPercent = leftPercent;
    unit.bodyHeight = 110.0;
    addUnit(*static_cast<FlowsheetScene*>(m_scene), std::move(unit));
}

void MainWindow::copySelectedFlowGroup() {
    const auto result = m_flowClipboard.copy(*static_cast<FlowsheetScene*>(m_scene), *m_document);
    statusBar()->showMessage(result.units > 0
        ? tr("已复制 %1 个单元、%2 条连接").arg(result.units).arg(result.connections)
        : tr("请先选择至少一个流程单元"), 3000);
}

void MainWindow::pasteFlowGroup() {
    if (!m_flowClipboard.hasData()) {
        statusBar()->showMessage(tr("剪贴板中没有可粘贴的流程单元"), 3000);
        return;
    }
    const auto result = m_flowClipboard.paste(*static_cast<FlowsheetScene*>(m_scene), *m_document,
                                               [this] { return takeNextUnitId(); });
    statusBar()->showMessage(tr("已粘贴 %1 个单元、%2 条连接")
                                 .arg(result.units).arg(result.connections), 3000);
}

void MainWindow::updateNextUnitId() {
    QSet<QString> ids;
    for (auto* item : m_scene->items()) {
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item)) ids.insert(unit->unit().id);
    }
    m_nextUnitId = 1;
    while (ids.contains(QString::number(m_nextUnitId))) ++m_nextUnitId;
}

QString MainWindow::takeNextUnitId() {
    updateNextUnitId();
    return QString::number(m_nextUnitId++);
}

void MainWindow::resizeSelectedUnits(double delta) {
    auto* flowsheet = static_cast<FlowsheetScene*>(m_scene);
    const auto result = CanvasActions::resizeSelection(*flowsheet, delta);
    if (result.resizedConnections > 0 && result.resizedUnits > 0) {
        statusBar()->showMessage(tr("已调整 %1 个浮选单元宽度和 %2 条产品线长度")
                                     .arg(result.resizedUnits).arg(result.resizedConnections), 3000);
    } else if (result.resizedConnections > 0) {
        statusBar()->showMessage(tr("已调整 %1 条产品线长度").arg(result.resizedConnections), 3000);
    } else if (result.resizedUnits > 0) {
        statusBar()->showMessage(tr("已调整 %1 个浮选单元，当前横向长度：%2")
                                     .arg(result.resizedUnits).arg(result.lastUnitWidth, 0, 'f', 0), 3000);
    } else if (m_scene->selectedItems().isEmpty()) {
        statusBar()->showMessage(tr("请先选中浮选单元或产品线"), 3000);
    } else {
        statusBar()->showMessage(tr("所选对象不支持此操作，或已达到允许的最小/最大长度"), 3000);
    }
}

void MainWindow::disconnectSelectedLines() {
    const int disconnected = CanvasActions::disconnectSelection(*static_cast<FlowsheetScene*>(m_scene));
    statusBar()->showMessage(disconnected > 0
        ? tr("已断开 %1 条浮选单元连接线").arg(disconnected)
        : tr("请先选中已连接的产品线、产品汇流或入料回流汇合点"), 3000);
}

} // namespace afs
