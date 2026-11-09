#pragma once
#include <QByteArray>
#include <QVector>
namespace afs {
class FlowsheetDocument;
struct StreamDescriptor;
class ExcelWorksheetBuilder final {
public:
    static QByteArray build(const FlowsheetDocument& document,
                            const QVector<StreamDescriptor>& orderedStreams);
};
}
