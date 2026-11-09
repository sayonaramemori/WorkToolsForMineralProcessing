#pragma once
#include <QByteArray>
#include <QVector>
namespace afs {
class FlowsheetDocument;
struct CanvasStreamDescriptor;
class ExcelWorksheetBuilder final {
public:
    static QByteArray build(const FlowsheetDocument& document,
                            const QVector<CanvasStreamDescriptor>& orderedStreams);
};
}
