#pragma once

#include "document/FlowsheetSnapshot.h"
#include <QString>

namespace afs {

class FlowsheetDocument;

class ExcelDataExporter final {
public:
    ExcelDataExporter() = delete;
    // Only user-named streams, in the project's configured export order.
    static QVector<StreamDescriptor> namedStreams(const FlowsheetDocument& document,
                                                       const FlowsheetSnapshot& snapshot);
    static bool exportWorkbook(const FlowsheetDocument& document,
                               const FlowsheetSnapshot& snapshot,
                               const QString& filePath, QString* error = nullptr);
};

} // namespace afs
