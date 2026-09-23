#include "ui/TerminalProductTableModel.h"
#include "document/FlowsheetDocument.h"

#include <QBrush>
#include <QColor>
#include <QGuiApplication>
#include <QPalette>
#include <QStringList>

namespace afs {

TerminalProductTableModel::TerminalProductTableModel(FlowsheetDocument& document, QObject* parent)
    : QAbstractTableModel(parent), m_document(document) {
    connect(&document, &FlowsheetDocument::measurementChanged, this, [this](const QString& id) {
        for (int row = 0; row < m_streams.size(); ++row) {
            if (id.isEmpty() || m_streams[row].streamId == id)
                emit dataChanged(index(row, MassColumn), index(row, statusColumn()));
        }
    });
    connect(&document, &FlowsheetDocument::productNameChanged, this, [this](const QString& id) {
        for (int row = 0; row < m_streams.size(); ++row)
            if (id.isEmpty() || m_streams[row].streamId == id)
                emit dataChanged(index(row, ProductNameColumn), index(row, ProductNameColumn));
    });
    connect(&document, &FlowsheetDocument::calculationChanged, this, [this] {
        if (!m_streams.isEmpty())
            emit dataChanged(index(0, MassColumn), index(m_streams.size() - 1, statusColumn()));
    });
    connect(&document, &FlowsheetDocument::componentsChanged, this, [this] {
        beginResetModel(); endResetModel();
    });
    connect(&document, &FlowsheetDocument::calculationModeChanged, this, [this] {
        beginResetModel(); endResetModel();
    });
}

int TerminalProductTableModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_streams.size();
}

int TerminalProductTableModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return m_document.calculationMode() == CalculationMode::DataReconciliation
        ? 5 + 2 * m_document.components().size()
        : 4 + m_document.components().size();
}

int TerminalProductTableModel::gradeColumn(const QString& componentId) const {
    for (int i = 0; i < m_document.components().size(); ++i)
        if (m_document.components()[i].id == componentId)
            return (m_document.calculationMode() == CalculationMode::DataReconciliation ? 4 : 3)
                + (m_document.calculationMode() == CalculationMode::DataReconciliation ? 2 * i : i);
    return -1;
}
int TerminalProductTableModel::gradeStdDevColumn(const QString& componentId) const {
    const int grade = gradeColumn(componentId);
    return m_document.calculationMode() == CalculationMode::DataReconciliation ? grade + 1 : -1;
}
int TerminalProductTableModel::statusColumn() const {
    return columnCount() - 1;
}
bool TerminalProductTableModel::isGradeColumn(int column) const {
    for (const auto& component : m_document.components())
        if (gradeColumn(component.id) == column) return true;
    return false;
}
bool TerminalProductTableModel::isUncertaintyColumn(int column) const {
    if (m_document.calculationMode() != CalculationMode::DataReconciliation) return false;
    if (column == MassStdDevColumn) return true;
    for (const auto& component : m_document.components())
        if (gradeStdDevColumn(component.id) == column) return true;
    return false;
}
QString TerminalProductTableModel::componentIdForColumn(int column) const {
    for (const auto& component : m_document.components())
        if (gradeColumn(component.id) == column || gradeStdDevColumn(component.id) == column)
            return component.id;
    return {};
}

QVariant TerminalProductTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_streams.size()) return {};
    const auto& stream = m_streams[index.row()];
    const bool isEditableMeasurement = m_editableStreamIds.contains(stream.streamId);
    const auto measurement = m_document.measurement(stream.streamId);
    bool hasAnyMeasurement = measurement.dryMass.has_value();
    for (const auto& componentId : m_document.componentIds())
        hasAnyMeasurement = hasAnyMeasurement || measurement.grade(componentId).has_value();
    const bool measurementComplete = measurement.completeFor(m_document.componentIds());
    const auto* calculation = m_document.calculationResult();
    const bool hasCalculatedValue = calculation && calculation->values.contains(stream.streamId);
    const auto calculated = hasCalculatedValue ? calculation->values.value(stream.streamId)
                                               : topology::StreamValue{};
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
        case NameColumn: return stream.displayName;
        case ProductNameColumn: return m_document.productName(stream.streamId);
        case MassColumn:
            if (role == Qt::DisplayRole && hasCalculatedValue
                && calculation->reconciled) return calculated.dryMass;
            if (measurement.dryMass) return *measurement.dryMass;
            return role == Qt::DisplayRole && hasCalculatedValue
                ? QVariant(calculated.dryMass) : QVariant();
        case MassStdDevColumn:
            if (m_document.calculationMode() == CalculationMode::DataReconciliation
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
                if (!m_document.components().isEmpty()
                    && index.column() == gradeColumn(m_document.components().front().id)
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
        && m_document.calculationMode() == CalculationMode::DataReconciliation)
        return QString("质量 σ");
    if (isGradeColumn(section)) {
        const QString id = componentIdForColumn(section);
        for (const auto& component : m_document.components())
            if (component.id == id) return QString("%1 / %").arg(component.name);
    }
    if (isUncertaintyColumn(section)) {
        const QString id = componentIdForColumn(section);
        for (const auto& component : m_document.components())
            if (component.id == id) return QString("%1 σ").arg(component.name);
    }
    if (section == statusColumn()) return QString("状态");
    return {};
}

Qt::ItemFlags TerminalProductTableModel::flags(const QModelIndex& index) const {
    auto result = QAbstractTableModel::flags(index);
    if (index.column() == ProductNameColumn
        || (m_editableStreamIds.contains(m_streams[index.row()].streamId)
            && (index.column() == MassColumn || isGradeColumn(index.column())
                || isUncertaintyColumn(index.column()))))
        result |= Qt::ItemIsEditable;
    return result;
}

bool TerminalProductTableModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (role != Qt::EditRole || !index.isValid() || index.row() >= m_streams.size()) return false;
    if (index.column() == ProductNameColumn) {
        m_document.setProductName(m_streams[index.row()].streamId, value.toString());
        return true;
    }
    if (index.column() != MassColumn && !isGradeColumn(index.column())
        && !isUncertaintyColumn(index.column())) return false;
    if (!m_editableStreamIds.contains(m_streams[index.row()].streamId)) return false;
    const QString text = value.toString().trimmed();
    std::optional<double> number;
    if (!text.isEmpty()) {
        bool ok = false;
        const double parsed = text.toDouble(&ok);
        const bool valid = ok && parsed >= 0.0
            && (!isGradeColumn(index.column()) || parsed <= 100.0);
        if (!valid) return false;
        number = parsed;
    }
    const auto& id = m_streams[index.row()].streamId;
    if (index.column() == MassColumn) m_document.setDryMass(id, number);
    else if (index.column() == MassStdDevColumn
             && m_document.calculationMode() == CalculationMode::DataReconciliation) {
        if (number && *number <= 0.0) return false;
        m_document.setDryMassStdDev(id, number);
    }
    else if (isUncertaintyColumn(index.column())) {
        if (number && *number <= 0.0) return false;
        m_document.setGradeStdDev(id, componentIdForColumn(index.column()), number);
    }
    else if (isGradeColumn(index.column()))
        m_document.setGradePercent(id, componentIdForColumn(index.column()), number);
    return true;
}

void TerminalProductTableModel::setStreams(QVector<CanvasStreamDescriptor> streams) {
    QSet<QString> ids;
    for (const auto& stream : streams) ids.insert(stream.streamId);
    setStreams(std::move(streams), std::move(ids));
}

void TerminalProductTableModel::setStreams(QVector<CanvasStreamDescriptor> streams,
                                           QSet<QString> editableStreamIds) {
    beginResetModel();
    m_streams = std::move(streams);
    m_editableStreamIds = std::move(editableStreamIds);
    endResetModel();
}

const CanvasStreamDescriptor* TerminalProductTableModel::streamAt(int row) const {
    return row >= 0 && row < m_streams.size() ? &m_streams[row] : nullptr;
}

int TerminalProductTableModel::rowForGraphicsItem(const QGraphicsItem* item) const {
    for (int row = 0; row < m_streams.size(); ++row)
        if (m_streams[row].graphicsItem == item) return row;
    return -1;
}

int TerminalProductTableModel::completedCount() const {
    int result = 0;
    for (const auto& stream : m_streams)
        if (m_document.measurement(stream.streamId).completeFor(m_document.componentIds())) ++result;
    return result;
}

} // namespace afs
