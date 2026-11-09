#include "app/MainWindow.h"
#include "annotations/AnnotationManager.h"
#include "document/FlowsheetDocument.h"
#include "ui/ResultMetricMenu.h"
#include "ui/WorkspaceSidebar.h"
#include <QAction>
#include <QInputDialog>
#include <QKeySequence>
#include <QMenu>
#include <QSignalBlocker>

namespace afs {
void MainWindow::populateDisplayMenu(QMenu* displayMenu, QWidget* controlParent) {
    m_themeAction = displayMenu->addAction(tr("CAD 暗色画布"));
    m_themeAction->setCheckable(true);
    m_themeAction->setShortcut(QKeySequence("Ctrl+Shift+T"));
    connect(m_themeAction, &QAction::toggled, this, [this](bool checked) { applyTheme(checked); });

    m_annotationsAction = displayMenu->addAction(tr("显示指标"));
    m_annotationsAction->setCheckable(true);
    m_annotationsAction->setChecked(true);
    connect(m_annotationsAction, &QAction::toggled, this,
            [this](bool visible) { m_annotationManager->setAnnotationsVisible(visible); });

    auto* metricMenu = new ResultMetricMenu(controlParent);
    connect(metricMenu, &ResultMetricMenu::metricVisibilityChanged,
            m_annotationManager, &AnnotationManager::setMetricVisible);
    connect(metricMenu, &ResultMetricMenu::productNameVisibilityChanged,
            m_annotationManager, &AnnotationManager::setProductNamesVisible);
    connect(metricMenu, &ResultMetricMenu::metricLabelModeChanged,
            m_annotationManager, &AnnotationManager::setMetricLabelMode);
    connect(metricMenu, &ResultMetricMenu::massUnitChanged,
            m_annotationManager, &AnnotationManager::setMassUnit);
    connect(metricMenu, &ResultMetricMenu::customMassUnitChanged,
            m_annotationManager, &AnnotationManager::setCustomMassUnit);
    metricMenu->setSettings(m_document->annotationTextSettings());
    connect(m_document, &FlowsheetDocument::annotationTextSettingsChanged,
            metricMenu, [this, metricMenu] {
                metricMenu->setSettings(m_document->annotationTextSettings());
                if (m_annotationsAction) {
                    const QSignalBlocker blocker(m_annotationsAction);
                    m_annotationsAction->setChecked(
                        m_document->annotationTextSettings().resultAnnotationsVisible);
                }
            });
    displayMenu->addMenu(metricMenu);

    auto* valueSources = displayMenu->addAction(tr("区分实测值与计算值"));
    valueSources->setObjectName("distinguishValueSourcesAction");
    valueSources->setCheckable(true);
    valueSources->setChecked(m_annotationManager->distinguishValueSources());
    valueSources->setToolTip(tr("含计算质量或计算品位的结果卡使用加粗边框；产率和回收率不参与区分"));
    connect(valueSources, &QAction::toggled,
            m_annotationManager, &AnnotationManager::setDistinguishValueSources);
    connect(m_annotationManager, &AnnotationManager::valueSourcesVisibilityChanged,
            valueSources, [valueSources](bool enabled) {
                const QSignalBlocker blocker(valueSources);
                valueSources->setChecked(enabled);
            });

    auto* borderWidth = displayMenu->addAction(tr("计算值边框线宽…"));
    borderWidth->setObjectName("calculatedResultBorderWidthAction");
    connect(borderWidth, &QAction::triggered, this, [this] {
        bool accepted = false;
        const double width = QInputDialog::getDouble(this, tr("计算值边框线宽"),
            tr("线宽（画布单位）"), m_annotationManager->calculatedResultBorderWidth(),
            ResultBorderStyle::MinimumWidth, ResultBorderStyle::MaximumWidth,
            1, &accepted, Qt::WindowFlags(), 0.1);
        if (accepted) m_annotationManager->setCalculatedResultBorderWidth(width);
    });

    displayMenu->addSeparator();
    auto* annotationStyleAction = displayMenu->addAction(tr("标注样式"));
    connect(annotationStyleAction, &QAction::triggered,
            this, [this] { editAnnotationTextStyle(); });
    displayMenu->addSeparator();
    for (const auto& key : m_workspaceSidebar->panelKeys())
        displayMenu->addAction(m_workspaceSidebar->panelAction(key));
}
} // namespace afs
