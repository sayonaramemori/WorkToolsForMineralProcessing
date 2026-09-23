#include "ui/ResultMetricMenu.h"

#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QInputDialog>
#include <QLineEdit>
#include <QSignalBlocker>

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
        return action;
    };
    m_dryMassAction = addMetric(tr("干质量"), ResultMetric::DryMass, true);
    m_gradeAction = addMetric(tr("品位"), ResultMetric::Grade, true);
    m_productNameAction = popup->addAction(tr("显示产品名称"));
    m_productNameAction->setCheckable(true);
    connect(m_productNameAction, &QAction::toggled, this,
            [this](bool visible) { emit productNameVisibilityChanged(visible); });
    popup->addSeparator();
    m_yieldAction = addMetric(tr("全流程产率"), ResultMetric::OverallYield, false);
    m_recoveryAction = addMetric(tr("全流程回收率"), ResultMetric::OverallRecovery, false);
    popup->addSeparator();
    auto* labelsMenu = popup->addMenu(tr("指标标签"));
    labelsMenu->setToolTipsVisible(true);
    auto* labelGroup = new QActionGroup(this);
    labelGroup->setExclusive(true);
    m_chineseLabelAction = labelsMenu->addAction(tr("中文名称"));
    m_chineseLabelAction->setCheckable(true);
    m_chineseLabelAction->setChecked(true);
    labelGroup->addAction(m_chineseLabelAction);
    m_symbolLabelAction = labelsMenu->addAction(tr("符号（m、β、γ、ε）"));
    m_symbolLabelAction->setCheckable(true);
    m_symbolLabelAction->setToolTip(tr("m：干质量，β：品位，γ：产率，ε：回收率"));
    labelGroup->addAction(m_symbolLabelAction);
    connect(m_chineseLabelAction, &QAction::triggered, this,
            [this] { emit metricLabelModeChanged(MetricLabelMode::Chinese); });
    connect(m_symbolLabelAction, &QAction::triggered, this,
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

void ResultMetricMenu::setSettings(const AnnotationTextSettings& settings) {
    // Project loading and the annotation-style dialog both update the document.
    // Reflect that state without sending UI-originated changes back to it.
    const auto setCheckedSilently = [](QAction* action, bool checked) {
        if (!action) return;
        const QSignalBlocker blocker(action);
        action->setChecked(checked);
    };
    setCheckedSilently(m_dryMassAction, settings.showDryMass);
    setCheckedSilently(m_gradeAction, settings.showGrade);
    setCheckedSilently(m_productNameAction, settings.showProductName);
    setCheckedSilently(m_yieldAction, settings.showOverallYield);
    setCheckedSilently(m_recoveryAction, settings.showOverallRecovery);
    setCheckedSilently(m_chineseLabelAction,
                       settings.metricLabelMode == MetricLabelMode::Chinese);
    setCheckedSilently(m_symbolLabelAction,
                       settings.metricLabelMode == MetricLabelMode::Symbols);
    setMassUnit(settings.massUnit, settings.customMassUnit);
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
