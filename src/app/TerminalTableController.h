#pragma once
#include "contracts/TerminalTableSession.h"

namespace afs {
class FlowsheetDocument;
class TerminalTableController final : public TerminalTableSession {
public:
    explicit TerminalTableController(FlowsheetDocument& document, QObject* parent = nullptr);
    const TerminalTableState& snapshot() const override { return m_state; }
    bool submit(const TableCommand& command) override;
    void setStreams(QVector<StreamDescriptor> streams,
                    std::optional<QSet<QString>> editableStreamIds = std::nullopt);
private:
    FlowsheetDocument& m_document;
    TerminalTableState m_state;
    void refresh(const QString& streamId = {}, TableChange change = TableChange::All);
    bool hasStream(const QString& streamId) const;
    bool apply(const EditTableMeasurement& edit);
    bool apply(const EditTableProductName& edit);
    bool apply(const EditTableComponents& edit);
    bool apply(const EditTableMode& edit);
};
} // namespace afs
