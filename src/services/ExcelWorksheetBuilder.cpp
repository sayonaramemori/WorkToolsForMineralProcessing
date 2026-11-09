#include "services/ExcelWorksheetBuilder.h"
#include "adapters/CanvasTopologyBuilder.h"
#include "document/FlowsheetDocument.h"

namespace afs {
namespace {
QByteArray xmlEscape(QString value) {
    value.replace('&', "&amp;").replace('<', "&lt;").replace('>', "&gt;")
         .replace('"', "&quot;").replace('\'', "&apos;");
    return value.toUtf8();
}

QByteArray textCell(const QString& reference, const QString& value, int style) {
    return "<c r=\"" + reference.toUtf8() + "\" s=\"" + QByteArray::number(style)
        + "\" t=\"inlineStr\"><is><t>" + xmlEscape(value) + "</t></is></c>";
}

QByteArray numberCell(const QString& reference, double value) {
    return "<c r=\"" + reference.toUtf8() + "\" s=\"4\"><v>"
        + QByteArray::number(value, 'g', 15) + "</v></c>";
}

QString columnName(int zeroBased) {
    QString result;
    for (int value = zeroBased + 1; value > 0; value = (value - 1) / 26)
        result.prepend(QChar('A' + (value - 1) % 26));
    return result;
}

} // namespace

QByteArray ExcelWorksheetBuilder::build(const FlowsheetDocument& document,
                        const QVector<CanvasStreamDescriptor>& orderedStreams) {
    QByteArray rows;
    rows += "<row r=\"1\" ht=\"24\" customHeight=\"1\">";
    QStringList headers{"方案名称", "产物名称", "重量", "产率 / %"};
    for (const auto& component : document.components()) {
        headers.append(QString("%1 品位 / %").arg(component.name));
        headers.append(QString("%1 回收率 / %").arg(component.name));
    }
    for (int column = 0; column < headers.size(); ++column)
        rows += textCell(columnName(column) + "1", headers[column], 2);
    rows += "</row>";

    QByteArray merges;
    int mergeCount = 0;
    int row = 2;
    for (const auto& scenario : document.scenarios()) {
        const auto* result = scenario.calculationResult ? &*scenario.calculationResult : nullptr;
        if (!result) continue;
        const int firstRow = row;
        bool firstProductInScenario = true;
        for (const auto& product : orderedStreams) {
            if (!result->values.contains(product.streamId)) continue;
            rows += "<row r=\"" + QByteArray::number(row) + "\">";
            rows += textCell("A" + QString::number(row),
                             firstProductInScenario ? scenario.name : QString(), 1);
            rows += textCell("B" + QString::number(row), product.displayName, 3);
            const auto value = result->values.value(product.streamId);
            rows += numberCell("C" + QString::number(row), value.dryMass);
            const auto metrics = result->relativeToExternalFeed.constFind(product.streamId);
            if (metrics != result->relativeToExternalFeed.cend())
                rows += numberCell("D" + QString::number(row), metrics->massYieldPercent);
            int column = 4;
            for (int componentIndex = 0; componentIndex < document.components().size();
                 ++componentIndex) {
                const auto& component = document.components()[componentIndex];
                const auto componentResult = result->components.constFind(component.id);
                if (componentResult != result->components.cend()
                    && componentResult->values.contains(product.streamId)) {
                    rows += numberCell(columnName(column) + QString::number(row),
                        componentResult->values.value(product.streamId).gradePercent());
                    const auto componentMetrics =
                        componentResult->relativeToExternalFeed.constFind(product.streamId);
                    if (componentMetrics != componentResult->relativeToExternalFeed.cend())
                        rows += numberCell(columnName(column + 1) + QString::number(row),
                                           componentMetrics->recoveryPercent);
                } else if (componentIndex == 0) {
                    rows += numberCell(columnName(column) + QString::number(row),
                                       value.gradePercent());
                    if (metrics != result->relativeToExternalFeed.cend())
                        rows += numberCell(columnName(column + 1) + QString::number(row),
                                           metrics->recoveryPercent);
                }
                column += 2;
            }
            rows += "</row>";
            firstProductInScenario = false;
            ++row;
        }
        if (row - firstRow > 1) {
            merges += "<mergeCell ref=\"A" + QByteArray::number(firstRow) + ":A"
                + QByteArray::number(row - 1) + "\"/>";
            ++mergeCount;
        }
    }

    QByteArray xml = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
        "<sheetViews><sheetView workbookViewId=\"0\"><pane ySplit=\"1\" topLeftCell=\"A2\" activePane=\"bottomLeft\" state=\"frozen\"/></sheetView></sheetViews>"
        "<cols><col min=\"1\" max=\"1\" width=\"18\" customWidth=\"1\"/>"
        "<col min=\"2\" max=\"2\" width=\"28\" customWidth=\"1\"/>"
        "<col min=\"3\" max=\"64\" width=\"14\" customWidth=\"1\"/></cols><sheetData>";
    xml += rows + "</sheetData>";
    if (!merges.isEmpty())
        xml += "<mergeCells count=\"" + QByteArray::number(mergeCount)
            + "\">" + merges + "</mergeCells>";
    xml += "<autoFilter ref=\"A1:" + columnName(headers.size() - 1).toUtf8()
        + "1\"/><pageMargins left=\"0.3\" right=\"0.3\" top=\"0.5\" bottom=\"0.5\" header=\"0.2\" footer=\"0.2\"/>"
           "</worksheet>";
    return xml;
}
} // namespace afs
