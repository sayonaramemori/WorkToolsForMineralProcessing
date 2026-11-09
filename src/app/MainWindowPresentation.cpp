#include "app/MainWindow.h"
#include "app/FlowTemplatePreviewSession.h"
#include "annotations/AnnotationManager.h"
#include "core/FlotationUnit.h"
#include "document/FlowsheetDocument.h"
#include "editor/FlowsheetScene.h"
#include "graphics/items/FlotationUnitItem.h"
#include "services/ThemeService.h"
#include "ui/AnnotationTextStyleDialog.h"
#include "ui/OperationLogDock.h"
#include "ui/TerminalProductDock.h"
#include <QAction>
#include <QLabel>
#include <QSignalBlocker>

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
    case UnitKind::BallMill: return QObject::tr("球磨机");
    case UnitKind::Thickener: return QObject::tr("浓密池");
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

void MainWindow::editAnnotationTextStyle() {
    if (const auto settings = editAnnotationTextStyleDialog(this, m_document->annotationTextSettings()))
        m_document->setAnnotationTextSettings(*settings);
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
    if (m_templatePreview) m_templatePreview->refreshAppearance(m_document->annotationTextSettings());
    if (saveSetting) ThemeService::saveDarkPreference(dark);
}

} // namespace afs
