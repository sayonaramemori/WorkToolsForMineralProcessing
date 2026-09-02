#include "ui/ResultMetricMenu.h"

#include <QAction>
#include <QActionGroup>
#include <QMenu>

namespace afs {

ResultMetricMenu::ResultMetricMenu(QWidget* parent) : QToolButton(parent) {
    setText(tr("指标设置"));
    setPopupMode(QToolButton::InstantPopup);
    auto* popup = new QMenu(this);
    popup->setTitle(tr("指标设置"));
    const auto addMetric = [this, popup](const QString& text, ResultMetric metric, bool checked) {
        auto* action = popup->addAction(text);
        action->setCheckable(true);
        action->setChecked(checked);
        connect(action, &QAction::toggled, this,
                [this, metric](bool visible) { emit metricVisibilityChanged(metric, visible); });
    };
    addMetric(tr("干质量"), ResultMetric::DryMass, true);
    addMetric(tr("品位"), ResultMetric::Grade, true);
    popup->addSeparator();
    addMetric(tr("全流程产率"), ResultMetric::OverallYield, false);
    addMetric(tr("全流程回收率"), ResultMetric::OverallRecovery, false);
    popup->addSeparator();
    auto* labelsMenu = popup->addMenu(tr("指标标签"));
    labelsMenu->setToolTipsVisible(true);
    auto* labelGroup = new QActionGroup(this);
    labelGroup->setExclusive(true);
    auto* chinese = labelsMenu->addAction(tr("中文名称"));
    chinese->setCheckable(true);
    chinese->setChecked(true);
    labelGroup->addAction(chinese);
    auto* symbols = labelsMenu->addAction(tr("符号（m、β、γ、ε）"));
    symbols->setCheckable(true);
    symbols->setToolTip(tr("m：干质量，β：品位，γ：产率，ε：回收率"));
    labelGroup->addAction(symbols);
    connect(chinese, &QAction::triggered, this,
            [this] { emit metricLabelModeChanged(MetricLabelMode::Chinese); });
    connect(symbols, &QAction::triggered, this,
            [this] { emit metricLabelModeChanged(MetricLabelMode::Symbols); });
    setMenu(popup);
}

} // namespace afs
