#pragma once
#include <QByteArray>
#include <QString>
namespace afs {
// Packages worksheet XML into a single-sheet XLSX file; knows no flow data.
class XlsxWorkbookWriter final {
public:
    static bool write(const QString& filePath, const QByteArray& worksheetXml,
                      QString* error = nullptr);
};
}
