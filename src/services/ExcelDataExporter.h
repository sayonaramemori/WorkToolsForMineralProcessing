#pragma once

#include "adapters/CanvasTopologyBuilder.h"
#include <QString>

namespace afs {

class FlowsheetDocument;

class ExcelDataExporter final {
public:
    ExcelDataExporter() = delete;
    // Only user-named streams, in the project's configured export order.
    static QVector<CanvasStreamDescriptor> namedStreams(const FlowsheetDocument& document,
                                                       const CanvasTopologySnapshot& snapshot);
    static bool exportWorkbook(const FlowsheetDocument& document,
                               const CanvasTopologySnapshot& snapshot,
                               const QString& filePath, QString* error = nullptr);
};

} // namespace afs
