#pragma once

#include <QString>

namespace afs {

class FlowsheetDocument;
struct CanvasTopologySnapshot;

class ExcelDataExporter final {
public:
    ExcelDataExporter() = delete;
    static bool exportWorkbook(const FlowsheetDocument& document,
                               const CanvasTopologySnapshot& snapshot,
                               const QString& filePath, QString* error = nullptr);
};

} // namespace afs
