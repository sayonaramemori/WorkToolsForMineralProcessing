#include "ui/ResultMetricMenu.h"

#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QInputDialog>
#include <QLineEdit>

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
    popup->addSeparator();
    auto* unitMenu = popup->addMenu(tr("质量单位"));
    m_massUnitGroup = new QActionGroup(this);
    m_massUnitGroup->setExclusive(true);
    const auto addUnit = [this, unitMenu](const QString& label, MassUnit unit, bool checked) {
        auto* action = unitMenu->addAction(label);
        action->setCheckable(true);
        action->setChecked(checked);
        action->setData(static_cast<int>(unit));
        m_massUnitGroup->addAction(action);
        connect(action, &QAction::triggered, this,
                [this, unit] { emit massUnitChanged(unit); });
    };
    addUnit(tr("克（g）"), MassUnit::Gram, true);
    addUnit(tr("千克（kg）"), MassUnit::Kilogram, false);
    addUnit(tr("吨（t）"), MassUnit::Tonne, false);
    auto* custom = unitMenu->addAction(tr("自定义单位…"));
    custom->setCheckable(true);
    custom->setData(static_cast<int>(MassUnit::Custom));
    m_massUnitGroup->addAction(custom);
    connect(custom, &QAction::triggered, this, [this] {
        bool accepted = false;
        const QString unit = QInputDialog::getText(
            this, tr("自定义质量单位"), tr("单位（最多 16 个字符）"),
            QLineEdit::Normal, m_customMassUnit, &accepted).simplified().left(16);
        if (accepted && !unit.isEmpty()) {
            m_massUnit = MassUnit::Custom;
            m_customMassUnit = unit;
            emit customMassUnitChanged(unit);
        } else {
            setMassUnit(m_massUnit, m_customMassUnit);
        }
    });
    setMenu(popup);
}

void ResultMetricMenu::setMassUnit(MassUnit unit, const QString& customUnit) {
    if (!m_massUnitGroup) return;
    m_massUnit = unit;
    m_customMassUnit = customUnit;
    for (auto* action : m_massUnitGroup->actions()) {
        if (action->data().toInt() == static_cast<int>(unit)) {
            action->setChecked(true);
            return;
        }
    }
}

} // namespace afs
