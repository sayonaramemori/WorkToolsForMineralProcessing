#include "ui/TerminalProductTableModel.h"
#include <QBrush>
#include <QColor>
#include <QGuiApplication>
#include <QPalette>
#include <QStringList>

namespace afs {
QVariant TerminalProductTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_state.streams.size()) return {};
    const auto& stream = m_state.streams[index.row()];
    const bool isEditableMeasurement = m_state.editableStreamIds.contains(stream.streamId);
    const auto measurement = m_state.measurements.value(stream.streamId);
    bool hasAnyMeasurement = measurement.dryMass.has_value();
    for (const auto& componentId : m_state.componentIds())
        hasAnyMeasurement = hasAnyMeasurement || measurement.grade(componentId).has_value();
    const bool measurementComplete = measurement.completeFor(m_state.componentIds());
    const auto* calculation = m_state.result();
    const bool hasCalculatedValue = calculation && calculation->values.contains(stream.streamId);
    const auto calculated = hasCalculatedValue ? calculation->values.value(stream.streamId)
                                               : topology::StreamValue{};
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
        case NameColumn: return stream.displayName;
        case ProductNameColumn: return m_state.productNames.value(stream.streamId);
        case MassColumn:
            if (role == Qt::DisplayRole && hasCalculatedValue
                && calculation->reconciled) return calculated.dryMass;
            if (measurement.dryMass) return *measurement.dryMass;
            return role == Qt::DisplayRole && hasCalculatedValue
                ? QVariant(calculated.dryMass) : QVariant();
        case MassStdDevColumn:
            if (m_state.mode == CalculationMode::DataReconciliation
                && measurement.dryMassStdDev) return *measurement.dryMassStdDev;
            break;
        default: break;
        }
        if (isUncertaintyColumn(index.column())) {
            const auto sigma = measurement.gradeStdDev(componentIdForColumn(index.column()));
            return sigma ? QVariant(*sigma) : QVariant();
        }
        if (isGradeColumn(index.column())) {
            const QString componentId = componentIdForColumn(index.column());
            const auto grade = measurement.grade(componentId);
            if (role == Qt::DisplayRole && calculation && calculation->reconciled) {
                const auto component = calculation->components.constFind(componentId);
                if (component != calculation->components.cend()
                    && component->values.contains(stream.streamId))
                    return component->values.value(stream.streamId).gradePercent();
            }
            if (grade) return *grade;
            if (role == Qt::DisplayRole && calculation) {
                const auto component = calculation->components.constFind(componentId);
                if (component != calculation->components.cend()
                    && component->values.contains(stream.streamId))
                    return component->values.value(stream.streamId).gradePercent();
                if (!m_state.components.isEmpty()
                    && index.column() == gradeColumn(m_state.components.front().id)
                    && hasCalculatedValue)
                    return calculated.gradePercent();
            }
            return {};
        }
        if (index.column() == statusColumn())
            return hasCalculatedValue && calculation->reconciled ? QString("协调值")
                : measurementComplete ? QString("实测值")
                : hasAnyMeasurement ? QString("填写中")
                : hasCalculatedValue ? QString("计算值") : QString("可填写");
        return {};
    }
    if (role == Qt::TextAlignmentRole && index.column() != NameColumn)
        return int(Qt::AlignCenter);
    if (role == Qt::BackgroundRole && isEditableMeasurement && hasAnyMeasurement) {
        const bool missingRequiredValue =
            (index.column() == MassColumn && !measurement.dryMass)
            || (isGradeColumn(index.column())
                && !measurement.grade(componentIdForColumn(index.column())));
        if (missingRequiredValue) {
            const bool dark = QGuiApplication::palette().color(QPalette::Base).lightness() < 128;
            return QBrush(dark ? QColor("#6a3f2b") : QColor("#ffd2b3"));
        }
    }
    if (role == CompletionRole) return measurementComplete;
    if (role == Qt::ForegroundRole && index.column() == statusColumn())
        return QBrush(measurementComplete ? QColor(35, 145, 70)
            : hasAnyMeasurement ? QColor(210, 125, 25)
            : hasCalculatedValue ? QColor(40, 110, 180) : QColor(120, 120, 120));
    if (role == Qt::ToolTipRole) {
        if (index.column() == ProductNameColumn)
            return QString("产品名称默认留空；仅已命名且具有计算结果的物流参与 Excel 导出");
        if (calculation && calculation->reconciled && hasCalculatedValue
            && calculation->residuals.contains(stream.streamId)
            && index.column() == MassColumn) {
            const auto residual = calculation->residuals.value(stream.streamId);
            return QString("显示协调值；双击编辑原始实测值。质量修正 %1（%2σ）")
                .arg(residual.dryMass, 0, 'g', 6)
                .arg(residual.dryMassStandardized, 0, 'f', 2);
        }
        if (calculation && calculation->reconciled && isGradeColumn(index.column())) {
            const QString componentId = componentIdForColumn(index.column());
            const auto observedGrade = measurement.grade(componentId);
            const auto component = calculation->components.constFind(componentId);
            if (observedGrade && component != calculation->components.cend()
                && component->values.contains(stream.streamId)) {
                const double reconciledGrade = component->values.value(stream.streamId).gradePercent();
                const double correction = reconciledGrade - *observedGrade;
                const double sigma = measurement.gradeStdDev(componentId).value_or(0.1);
                return QString("显示协调值；双击编辑原始实测值。%1 品位修正 %2 个百分点（%3σ）")
                    .arg(componentId)
                    .arg(correction, 0, 'g', 6)
                    .arg(correction / sigma, 0, 'f', 2);
            }
        }
        if (index.column() == MassColumn)
            return QString("请输入非负绝对干质量；留空表示未知");
        if (index.column() == MassStdDevColumn)
            return QString("质量测量的标准差；留空时默认采用实测质量的 1%");
        if (isUncertaintyColumn(index.column()))
            return QString("品位测量的标准差（百分点）；留空时默认采用 0.1");
        if (isGradeColumn(index.column())) return QString("请输入 0–100 之间的组分品位；留空表示未知");
        return QString("物流 ID：%1").arg(stream.streamId);
    }
    return {};
}

QVariant TerminalProductTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    if (section == NameColumn) return QString("物流/入料定义");
    if (section == ProductNameColumn) return QString("产品名称");
    if (section == MassColumn) return QString("绝对干质量");
    if (section == MassStdDevColumn
        && m_state.mode == CalculationMode::DataReconciliation)
        return QString("质量 σ");
    if (isGradeColumn(section)) {
        const QString id = componentIdForColumn(section);
        for (const auto& component : m_state.components)
            if (component.id == id) return QString("%1 / %").arg(component.name);
    }
    if (isUncertaintyColumn(section)) {
        const QString id = componentIdForColumn(section);
        for (const auto& component : m_state.components)
            if (component.id == id) return QString("%1 σ").arg(component.name);
    }
    if (section == statusColumn()) return QString("状态");
    return {};
}

} // namespace afs
