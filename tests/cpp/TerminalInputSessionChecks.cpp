#include "contracts/TerminalTableSession.h"
#include "ui/TerminalProductTableModel.h"

namespace afs::tests {
namespace {
// UI can be exercised without a document or an application controller.
class FakeSession final : public TerminalTableSession {
public:
    TerminalTableState state;
    std::optional<TableCommand> last;
    bool accepts{true};
    const TerminalTableState& snapshot() const override { return state; }
    bool submit(const TableCommand& command) override { last = command; return accepts; }
    void publish(TableChange kind, const QString& id = {}) { emit stateChanged(state, id, kind); }
};
}
int runTerminalSessionChecks() {
    FakeSession session;
    session.state.components = {{"cu", "Cu"}};
    session.state.streams = {{"a", "物流 A"}};
    session.state.editableStreamIds = {"a"};
    session.state.measurements["a"].dryMass = 10;
    session.state.measurements["a"].components["cu"].gradePercent = 2;
    TerminalProductTableModel model(session);
    const auto mass = model.index(0, TerminalProductTableModel::MassColumn);
    if (model.rowCount() != 1 || model.columnCount() != 5 || model.data(mass, Qt::EditRole).toDouble() != 10)
        return 91;
    if (!model.setData(mass, "12.5", Qt::EditRole) || !session.last) return 92;
    const auto* edit = std::get_if<EditTableMeasurement>(&*session.last);
    if (!edit || edit->streamId != "a" || edit->field != MeasurementField::DryMass || edit->text != "12.5"
        || model.data(mass, Qt::EditRole).toDouble() != 10) return 93;
    session.accepts = false;
    if (model.setData(mass, "bad input", Qt::EditRole)) return 94;
    int firstColumn = -1, lastColumn = -1;
    QObject::connect(&model, &QAbstractItemModel::dataChanged,
        [&](const QModelIndex& first, const QModelIndex& last) { firstColumn = first.column(); lastColumn = last.column(); });
    session.state.productNames["a"] = "精矿";
    session.publish(TableChange::ProductName, "a");
    if (firstColumn != TerminalProductTableModel::ProductNameColumn || lastColumn != firstColumn
        || model.data(model.index(0, firstColumn), Qt::DisplayRole).toString() != "精矿") return 95;
    session.state.measurements["a"].dryMass = 12.5;
    session.publish(TableChange::Measurement, "a");
    if (model.data(mass, Qt::EditRole).toDouble() != 12.5) return 96;
    session.state.components.append(ComponentDefinition{"zn", "Zn"});
    session.state.mode = CalculationMode::DataReconciliation;
    session.publish(TableChange::All);
    if (model.columnCount() != 9 || model.gradeColumn("zn") != 6) return 97;
    session.state.streams.clear(); session.publish(TableChange::All);
    if (model.rowCount() != 0 || model.flags({}).testFlag(Qt::ItemIsEditable)) return 98;
    return 0;
}
} // namespace afs::tests
