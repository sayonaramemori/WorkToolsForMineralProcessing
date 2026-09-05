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
}

int TerminalProductTableModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_streams.size();
}

int TerminalProductTableModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : 5 + 2 * m_document.components().size();
}

int TerminalProductTableModel::gradeColumn(const QString& componentId) const {
    for (int i = 0; i < m_document.components().size(); ++i)
        if (m_document.components()[i].id == componentId) return FirstGradeColumn + i;
    return -1;
}
int TerminalProductTableModel::dryMassShareColumn() const {
    return FirstGradeColumn + m_document.components().size();
}
int TerminalProductTableModel::componentShareColumn(const QString& componentId) const {
    const int grade = gradeColumn(componentId);
    return grade < 0 ? -1 : dryMassShareColumn() + 1 + grade - FirstGradeColumn;
}
int TerminalProductTableModel::statusColumn() const {
    return dryMassShareColumn() + 1 + m_document.components().size();
}
bool TerminalProductTableModel::isGradeColumn(int column) const {
    return column >= FirstGradeColumn && column < dryMassShareColumn();
}
bool TerminalProductTableModel::isComponentShareColumn(int column) const {
    return column > dryMassShareColumn() && column < statusColumn();
}
QString TerminalProductTableModel::componentIdForColumn(int column) const {
    int componentIndex = -1;
    if (isGradeColumn(column)) componentIndex = column - FirstGradeColumn;
    else if (isComponentShareColumn(column)) componentIndex = column - dryMassShareColumn() - 1;
    return componentIndex >= 0 && componentIndex < m_document.components().size()
        ? m_document.components()[componentIndex].id : QString();
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
            if (measurement.dryMass) return *measurement.dryMass;
            return role == Qt::DisplayRole && hasCalculatedValue
                ? QVariant(calculated.dryMass) : QVariant();
        default: break;
        }
        if (isGradeColumn(index.column())) {
            const QString componentId = componentIdForColumn(index.column());
            const auto grade = measurement.grade(componentId);
            if (grade) return *grade;
            if (role == Qt::DisplayRole && calculation) {
                const auto component = calculation->components.constFind(componentId);
                if (component != calculation->components.cend()
                    && component->values.contains(stream.streamId))
                    return component->values.value(stream.streamId).gradePercent();
                if (index.column() == FirstGradeColumn && hasCalculatedValue)
                    return calculated.gradePercent();
            }
            return {};
        }
        if (index.column() == dryMassShareColumn())
            return stream.mergeBranch && measurement.dryMassSharePercent
                ? QVariant(*measurement.dryMassSharePercent) : QVariant();
        if (isComponentShareColumn(index.column())) {
            const auto share = measurement.componentShare(componentIdForColumn(index.column()));
            return stream.mergeBranch && share ? QVariant(*share) : QVariant();
        }
        if (index.column() == statusColumn())
            return measurementComplete ? QString("实测值")
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
        if (index.column() == MassColumn)
            return QString("请输入非负干质量；也可将同一基准下的产率作为相对量输入；留空表示未知");
        if (isGradeColumn(index.column())) return QString("请输入 0–100 之间的组分品位；留空表示未知");
        if (index.column() == dryMassShareColumn() || isComponentShareColumn(index.column()))
            return stream.mergeBranch
                ? QString("可选：同一合流的干质量占比和组分占比必须分别合计 100%")
                : QVariant();
        return QString("物流 ID：%1").arg(stream.streamId);
    }
    return {};
}

QVariant TerminalProductTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    if (section == NameColumn) return QString("物流/入料定义");
    if (section == ProductNameColumn) return QString("产品名称");
    if (section == MassColumn) return QString("干质量/相对量");
    if (isGradeColumn(section)) {
        const int i = section - FirstGradeColumn;
        return QString("%1 / %").arg(m_document.components()[i].name);
    }
    if (section == dryMassShareColumn()) return QString("质量占比/%");
    if (isComponentShareColumn(section)) {
        const int i = section - dryMassShareColumn() - 1;
        return QString("%1占比/%").arg(m_document.components()[i].name);
    }
    if (section == statusColumn()) return QString("状态");
    return {};
}

Qt::ItemFlags TerminalProductTableModel::flags(const QModelIndex& index) const {
    auto result = QAbstractTableModel::flags(index);
    if (index.column() == ProductNameColumn
        || (m_streams[index.row()].mergeBranch
            && (index.column() == dryMassShareColumn() || isComponentShareColumn(index.column())))
        || (m_editableStreamIds.contains(m_streams[index.row()].streamId)
            && (index.column() == MassColumn || isGradeColumn(index.column()))))
        result |= Qt::ItemIsEditable;
    return result;
}

bool TerminalProductTableModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (role != Qt::EditRole || !index.isValid() || index.row() >= m_streams.size()) return false;
    if (index.column() == ProductNameColumn) {
        m_document.setProductName(m_streams[index.row()].streamId, value.toString());
        return true;
    }
    const bool shareColumn = index.column() == dryMassShareColumn()
        || isComponentShareColumn(index.column());
    if (index.column() != MassColumn && !isGradeColumn(index.column()) && !shareColumn) return false;
    if (shareColumn && !m_streams[index.row()].mergeBranch) return false;
    if (!shareColumn && !m_editableStreamIds.contains(m_streams[index.row()].streamId)) return false;
    const QString text = value.toString().trimmed();
    std::optional<double> number;
    if (!text.isEmpty()) {
        bool ok = false;
        const double parsed = text.toDouble(&ok);
        const bool valid = ok && parsed >= 0.0
            && (!isGradeColumn(index.column()) || parsed <= 100.0)
            && (!shareColumn || parsed <= 100.0);
        if (!valid) return false;
        number = parsed;
    }
    const auto& id = m_streams[index.row()].streamId;
    if (index.column() == MassColumn) m_document.setDryMass(id, number);
    else if (isGradeColumn(index.column()))
        m_document.setGradePercent(id, componentIdForColumn(index.column()), number);
    else if (index.column() == dryMassShareColumn())
        m_document.setDryMassSharePercent(id, number);
    else m_document.setComponentSharePercent(id, componentIdForColumn(index.column()), number);
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
