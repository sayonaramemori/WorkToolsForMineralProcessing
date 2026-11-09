#include "services/ExcelDataExporter.h"
#include "services/ExcelWorksheetBuilder.h"
#include "services/XlsxWorkbookWriter.h"
#include "adapters/CanvasTopologyBuilder.h"
#include "document/FlowsheetDocument.h"
#include <QSet>
#include <algorithm>

namespace afs {
QVector<CanvasStreamDescriptor> ExcelDataExporter::namedStreams(
    const FlowsheetDocument& document, const CanvasTopologySnapshot& snapshot) {
    QHash<QString, CanvasStreamDescriptor> byId;
    for (auto stream : snapshot.reportStreams) {
        stream.displayName = document.productName(stream.streamId).simplified();
        if (!stream.displayName.isEmpty()) {
            const QString id = stream.streamId;
            byId.insert(id, std::move(stream));
        }
    }
    QVector<CanvasStreamDescriptor> streams;
    QSet<QString> added;
    const auto append = [&](const QString& id) {
        const auto found = byId.constFind(id);
        if (found == byId.cend() || added.contains(id)) return;
        streams.append(found.value()); added.insert(id);
    };
    for (const auto& id : document.exportStreamOrder()) append(id);
    for (const auto& stream : snapshot.reportStreams) append(stream.streamId);
    return streams;
}

bool ExcelDataExporter::exportWorkbook(const FlowsheetDocument& document,
                                       const CanvasTopologySnapshot& snapshot,
                                       const QString& filePath, QString* error) {
    const auto streams = namedStreams(document, snapshot);
    if (streams.isEmpty()) {
        if (error) *error = QObject::tr("当前流程没有已命名的产品，请先在右侧表格填写产品名称。");
        return false;
    }
    const bool hasResults = std::any_of(document.scenarios().cbegin(), document.scenarios().cend(),
        [&](const auto& scenario) {
            return scenario.calculationResult && std::any_of(streams.cbegin(), streams.cend(),
                [&](const auto& stream) { return scenario.calculationResult->values.contains(stream.streamId); });
        });
    if (!hasResults) {
        if (error) *error = QObject::tr("已命名的产品暂无计算结果，请先完成计算。");
        return false;
    }
    return XlsxWorkbookWriter::write(filePath, ExcelWorksheetBuilder::build(document, streams), error);
}
} // namespace afs
