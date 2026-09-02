#pragma once

#include "adapters/CanvasTopologyBuilder.h"

#include <QAbstractTableModel>
#include <QSet>

namespace afs {

class FlowsheetDocument;

class TerminalProductTableModel final : public QAbstractTableModel {
    Q_OBJECT
public:
    enum FixedColumn { NameColumn, ProductNameColumn, MassColumn,
                       FirstGradeColumn, GradeColumn = FirstGradeColumn };
    enum CustomRole { CompletionRole = Qt::UserRole + 1 };

    explicit TerminalProductTableModel(FlowsheetDocument& document, QObject* parent = nullptr);
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;

    void setStreams(QVector<CanvasStreamDescriptor> streams);
    void setStreams(QVector<CanvasStreamDescriptor> streams,
                    QSet<QString> measurementStreamIds);
    [[nodiscard]] const CanvasStreamDescriptor* streamAt(int row) const;
    [[nodiscard]] int rowForGraphicsItem(const QGraphicsItem* item) const;
    [[nodiscard]] int completedCount() const;
    [[nodiscard]] int requiredCount() const { return m_measurementStreamIds.size(); }
    [[nodiscard]] int gradeColumn(const QString& componentId) const;
    [[nodiscard]] int dryMassShareColumn() const;
    [[nodiscard]] int componentShareColumn(const QString& componentId) const;
    [[nodiscard]] int statusColumn() const;
    [[nodiscard]] bool isGradeColumn(int column) const;
    [[nodiscard]] bool isComponentShareColumn(int column) const;
    [[nodiscard]] QString componentIdForColumn(int column) const;

private:
    FlowsheetDocument& m_document;
    QVector<CanvasStreamDescriptor> m_streams;
    QSet<QString> m_measurementStreamIds;
};

} // namespace afs
