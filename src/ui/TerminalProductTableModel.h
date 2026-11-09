#pragma once

#include "contracts/TerminalTableSession.h"

#include <QAbstractTableModel>
#include <QSet>

namespace afs {

class TerminalProductTableModel final : public QAbstractTableModel {
    Q_OBJECT
public:
    enum FixedColumn { NameColumn, ProductNameColumn, MassColumn,
                       MassStdDevColumn, GradeColumn = MassStdDevColumn };
    enum CustomRole { CompletionRole = Qt::UserRole + 1 };

    explicit TerminalProductTableModel(TerminalTableSession& session, QObject* parent = nullptr);
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;

    [[nodiscard]] const StreamDescriptor* streamAt(int row) const;
    [[nodiscard]] int rowForStreamId(const QString& streamId) const;
    [[nodiscard]] int completedCount() const;
    [[nodiscard]] int editableCount() const { return m_state.editableStreamIds.size(); }
    [[nodiscard]] int gradeColumn(const QString& componentId) const;
    [[nodiscard]] int gradeStdDevColumn(const QString& componentId) const;
    [[nodiscard]] int statusColumn() const;
    [[nodiscard]] bool isGradeColumn(int column) const;
    [[nodiscard]] bool isUncertaintyColumn(int column) const;
    [[nodiscard]] QString componentIdForColumn(int column) const;

    const TerminalTableState& tableState() const { return m_state; }
signals:
    void schemaChanged();
private:
    TerminalTableSession& m_session;
    TerminalTableState m_state;
    void applySnapshot(const TerminalTableState& state, const QString& streamId, TableChange change);
};

} // namespace afs
