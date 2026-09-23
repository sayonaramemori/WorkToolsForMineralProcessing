#include "app/MainWindow.h"

#include "adapters/CanvasTopologyBuilder.h"
#include "annotations/AnnotationManager.h"
#include "core/FlotationUnit.h"
#include "document/FlowsheetDocument.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FeedJunctionItem.h"
#include "graphics/items/FlotationUnitItem.h"
#include "graphics/items/MergeJunctionItem.h"
#include "graphics/items/ProductLineItem.h"
#include "services/FlowsheetCalculationService.h"
#include "services/ThemeService.h"
#include "ui/OperationLogDock.h"
#include "ui/TerminalProductDock.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGraphicsScene>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>
#include <QVBoxLayout>

namespace afs {

namespace {
QString unitTypeName(UnitKind kind) {
    switch (kind) {
    case UnitKind::Flotation: return QObject::tr("浮选单元");
    case UnitKind::MagneticSeparation: return QObject::tr("通用磁选机");
    case UnitKind::WeakMagneticSeparation: return QObject::tr("弱磁选机");
    case UnitKind::StrongMagneticSeparation: return QObject::tr("强磁选机");
    case UnitKind::SpiralChute: return QObject::tr("螺旋溜槽");
    case UnitKind::ShakingTable: return QObject::tr("摇床");
    case UnitKind::DenseMediumCyclone: return QObject::tr("重介质旋流器");
    case UnitKind::SedimentationTank: return QObject::tr("沉降箱");
    case UnitKind::DemediumScreen: return QObject::tr("脱介筛");
    case UnitKind::Screening: return QObject::tr("筛分机");
    case UnitKind::Classification: return QObject::tr("分级机");
    case UnitKind::TailingsPool: return QObject::tr("尾矿池");
    case UnitKind::ConcentratePool: return QObject::tr("精矿池");
    case UnitKind::WaterPool: return QObject::tr("回水池");
    case UnitKind::MediaTank: return QObject::tr("介质桶");
    case UnitKind::MixingTank: return QObject::tr("混料桶");
    case UnitKind::BinarySplitter: return QObject::tr("二分流器");
    case UnitKind::ThreeProductFlotation: return QObject::tr("三产品浮选单元");
    case UnitKind::ThreeProductScreening: return QObject::tr("三产品筛分器");
    case UnitKind::ThreeProductDemediumScreen: return QObject::tr("三产品脱介筛");
    }
    return QObject::tr("流程单元");
}
} // namespace

void MainWindow::appendOperationLog(const QString& message) {
    if (m_operationLog) m_operationLog->appendMessage(message);
}

void MainWindow::refreshTerminalProducts() {
    // Import emits topologyChanged so views can rebuild their snapshots, but
    // the imported scenarios have just been recalculated and must be retained.
    if (!m_loadingProject) m_document->invalidateAllCalculations();
    m_terminalDock->setSnapshot(CanvasTopologyBuilder::build(*static_cast<FlowsheetScene*>(m_scene)));
    refreshProductNames();
    syncCanvasSelectionToTable();
}

void MainWindow::refreshProductNames() {
    for (auto* item : m_scene->items()) {
        if (auto* product = dynamic_cast<ProductLineItem*>(item)) {
            product->setTextSettings(m_document->annotationTextSettings());
            product->setProductName(m_document->productName(product->streamId()));
        } else if (auto* merge = dynamic_cast<MergeJunctionItem*>(item)) {
            merge->setTextSettings(m_document->annotationTextSettings());
            merge->setProductName(m_document->productName(merge->outputStreamId()));
        }
    }
}

void MainWindow::editAnnotationTextStyle() {
    QDialog dialog(this);
    dialog.setWindowTitle(tr("标注文字样式"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;
    const auto current = m_document->annotationTextSettings();

    auto* pointSize = new QSpinBox(&dialog);
    pointSize->setRange(7, 36);
    pointSize->setSuffix(tr(" pt"));
    pointSize->setValue(current.pointSize);
    form->addRow(tr("字体大小"), pointSize);

    auto* bold = new QCheckBox(tr("加粗"), &dialog);
    bold->setChecked(current.bold);
    form->addRow(tr("字重"), bold);

    auto* followTheme = new QCheckBox(tr("跟随界面主题颜色"), &dialog);
    followTheme->setChecked(!current.color.isValid());
    form->addRow(tr("颜色模式"), followTheme);

    QColor selectedColor = current.color.isValid()
        ? current.color : qApp->palette().color(QPalette::Text);
    auto* colorButton = new QPushButton(&dialog);
    const auto refreshColorButton = [&] {
        colorButton->setText(selectedColor.name(QColor::HexRgb));
        colorButton->setStyleSheet(QString("background:%1; color:%2;")
            .arg(selectedColor.name(), selectedColor.lightness() < 128 ? "white" : "black"));
        colorButton->setEnabled(!followTheme->isChecked());
    };
    refreshColorButton();
    connect(followTheme, &QCheckBox::toggled, &dialog,
            [&](bool) { refreshColorButton(); });
    connect(colorButton, &QPushButton::clicked, &dialog, [&] {
        const QColor chosen = QColorDialog::getColor(
            selectedColor, &dialog, tr("选择标注文字颜色"), QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) { selectedColor = chosen; refreshColorButton(); }
    });
    form->addRow(tr("文字颜色"), colorButton);
    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) return;
    m_document->setAnnotationTextSettings({
        pointSize->value(), bold->isChecked(),
        followTheme->isChecked() ? QColor() : selectedColor,
        current.massUnit, current.customMassUnit});
}

void MainWindow::syncCanvasSelectionToTable() {
    const auto selected = m_scene->selectedItems();
    m_terminalDock->selectGraphicsItem(selected.isEmpty() ? nullptr : selected.first());
    refreshSelectedResult();
    refreshSelectionStatus();
}

void MainWindow::refreshSelectionStatus() {
    FlotationUnitItem* selectedUnit = nullptr;
    int selectedUnitCount = 0;
    for (auto* item : m_scene->selectedItems()) {
        if (auto* unit = dynamic_cast<FlotationUnitItem*>(item)) {
            if (!selectedUnit) selectedUnit = unit;
            ++selectedUnitCount;
        }
    }
    if (!selectedUnit) {
        m_selectionStatusLabel->setText(tr("当前未选中流程单元"));
        return;
    }
    QString text = tr("当前已选中：“%1 %2”")
        .arg(unitTypeName(selectedUnit->unit().kind), selectedUnit->unit().id);
    if (selectedUnitCount > 1) text += tr("（共 %1 个单元）").arg(selectedUnitCount);
    m_selectionStatusLabel->setText(text);
}

void MainWindow::calculateFlowsheet() {
    auto result = FlowsheetCalculationService::calculate(
        *static_cast<FlowsheetScene*>(m_scene), *m_document);
    const bool complete = result.complete;
    const int issueCount = result.issues.size();
    m_document->setCalculationResult(std::move(result));
    if (!complete) {
        const auto* storedResult = m_document->calculationResult();
        if (storedResult) for (const auto& issue : storedResult->issues) {
            appendOperationLog(tr("%1：%2").arg(
                issue.severity == topology::IssueSeverity::Error ? tr("错误") : tr("提示"),
                issue.message));
        }
    }
    statusBar()->showMessage(complete ? tr("平衡计算成功")
        : tr("计算未完成，共发现 %1 个问题").arg(issueCount), 5000);
}

void MainWindow::refreshSelectedResult() {
    const auto selected = m_scene->selectedItems();
    if (selected.isEmpty()) {
        m_terminalDock->clearResultDetails();
        return;
    }
    auto* item = selected.first();
    if (auto* product = dynamic_cast<ProductLineItem*>(item)) {
        m_terminalDock->showStreamResult(product->streamId());
    } else if (auto* merge = dynamic_cast<MergeJunctionItem*>(item)) {
        m_terminalDock->showStreamResult(merge->outputStreamId());
    } else if (auto* feedJunction = dynamic_cast<FeedJunctionItem*>(item)) {
        m_terminalDock->showStreamResult(feedJunction->outputStreamId());
    } else if (auto* unit = dynamic_cast<FlotationUnitItem*>(item)) {
        const auto snapshot = CanvasTopologyBuilder::build(*static_cast<FlowsheetScene*>(m_scene));
        m_terminalDock->showUnitResult(unit->unit().id, snapshot.graph);
    } else {
        m_terminalDock->clearResultDetails();
    }
}

void MainWindow::applyTheme(bool dark, bool saveSetting) {
    m_darkTheme = dark;
    m_scene->setBackgroundBrush(ThemeService::applyApplicationPalette(dark));
    if (m_themeAction) {
        const QSignalBlocker blocker(m_themeAction);
        m_themeAction->setChecked(dark);
        m_themeAction->setText(dark ? tr("明亮主题") : tr("CAD 暗色画布"));
    }
    static_cast<FlowsheetScene*>(m_scene)->refreshAppearance();
    if (m_terminalDock) m_terminalDock->refreshAppearance();
    if (m_annotationManager) m_annotationManager->refreshAppearance();
    if (saveSetting) ThemeService::saveDarkPreference(dark);
}

} // namespace afs
