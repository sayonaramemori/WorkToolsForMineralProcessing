#include "services/ExcelDataExporter.h"

#include "adapters/CanvasTopologyBuilder.h"
#include "document/FlowsheetDocument.h"

#include <QDataStream>
#include <QFile>
#include <QSet>
#include <QVector>

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

quint32 crc32(const QByteArray& data) {
    quint32 crc = 0xffffffffU;
    for (const auto byte : data) {
        crc ^= static_cast<unsigned char>(byte);
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
    }
    return ~crc;
}

struct ZipEntry {
    QByteArray name;
    QByteArray data;
    quint32 crc{0};
    quint32 offset{0};
};

bool writeZip(const QString& path, QVector<ZipEntry> entries, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = QObject::tr("无法创建文件：%1").arg(file.errorString());
        return false;
    }
    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);
    for (auto& entry : entries) {
        entry.offset = static_cast<quint32>(file.pos());
        entry.crc = crc32(entry.data);
        out << quint32(0x04034b50) << quint16(20) << quint16(0) << quint16(0)
            << quint16(0) << quint16(0) << entry.crc
            << quint32(entry.data.size()) << quint32(entry.data.size())
            << quint16(entry.name.size()) << quint16(0);
        file.write(entry.name); file.write(entry.data);
    }
    const quint32 centralOffset = static_cast<quint32>(file.pos());
    for (const auto& entry : entries) {
        out << quint32(0x02014b50) << quint16(20) << quint16(20)
            << quint16(0) << quint16(0) << quint16(0) << quint16(0)
            << entry.crc << quint32(entry.data.size()) << quint32(entry.data.size())
            << quint16(entry.name.size()) << quint16(0) << quint16(0)
            << quint16(0) << quint16(0) << quint32(0) << entry.offset;
        file.write(entry.name);
    }
    const quint32 centralSize = static_cast<quint32>(file.pos()) - centralOffset;
    out << quint32(0x06054b50) << quint16(0) << quint16(0)
        << quint16(entries.size()) << quint16(entries.size())
        << centralSize << centralOffset << quint16(0);
    if (out.status() != QDataStream::Ok || !file.flush()) {
        if (error) *error = QObject::tr("写入 Excel 文件失败：%1").arg(file.errorString());
        return false;
    }
    return true;
}

QByteArray worksheetXml(const FlowsheetDocument& document,
                        const CanvasTopologySnapshot& snapshot) {
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
    QVector<CanvasStreamDescriptor> orderedStreams;
    QHash<QString, CanvasStreamDescriptor> byId;
    for (const auto& stream : snapshot.reportStreams) byId.insert(stream.streamId, stream);
    QSet<QString> added;
    for (const auto& streamId : document.exportStreamOrder()) {
        const auto found = byId.constFind(streamId);
        if (found == byId.cend()) continue;
        orderedStreams.append(found.value()); added.insert(streamId);
    }
    for (const auto& stream : snapshot.reportStreams) {
        if (added.contains(stream.streamId)) continue;
        orderedStreams.append(stream); added.insert(stream.streamId);
    }

    for (const auto& scenario : document.scenarios()) {
        const auto* result = scenario.calculationResult ? &*scenario.calculationResult : nullptr;
        if (!result) continue;
        const int firstRow = row;
        bool firstProductInScenario = true;
        for (const auto& product : orderedStreams) {
            if (!result->values.contains(product.streamId)) continue;
            const QString configuredName = document.productName(product.streamId).simplified();
            const QString displayName = configuredName.isEmpty() ? product.displayName : configuredName;
            rows += "<row r=\"" + QByteArray::number(row) + "\">";
            rows += textCell("A" + QString::number(row),
                             firstProductInScenario ? scenario.name : QString(), 1);
            rows += textCell("B" + QString::number(row), displayName, 3);
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

} // namespace

bool ExcelDataExporter::exportWorkbook(const FlowsheetDocument& document,
                                       const CanvasTopologySnapshot& snapshot,
                                       const QString& filePath, QString* error) {
    if (snapshot.reportStreams.isEmpty()) {
        if (error) *error = QObject::tr("当前流程没有可导出的产品物流。");
        return false;
    }
    const QByteArray contentTypes = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/><Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/><Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/><Override PartName="/xl/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml"/></Types>)";
    const QByteArray rootRels = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/></Relationships>)";
    const QByteArray workbook = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?><workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><sheets><sheet name="方案数据" sheetId="1" r:id="rId1"/></sheets></workbook>)";
    const QByteArray workbookRels = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/><Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/></Relationships>)";
    const QByteArray styles = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?><styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><fonts count="2"><font><sz val="11"/><name val="Microsoft YaHei"/></font><font><b/><sz val="11"/><name val="Microsoft YaHei"/></font></fonts><fills count="3"><fill><patternFill patternType="none"/></fill><fill><patternFill patternType="gray125"/></fill><fill><patternFill patternType="solid"><fgColor rgb="FFD9EAF7"/><bgColor indexed="64"/></patternFill></fill></fills><borders count="2"><border/><border><left style="thin"/><right style="thin"/><top style="thin"/><bottom style="thin"/></border></borders><cellStyleXfs count="1"><xf numFmtId="0" fontId="0" fillId="0" borderId="0"/></cellStyleXfs><cellXfs count="5"><xf numFmtId="0" fontId="0" fillId="0" borderId="0" xfId="0"/><xf numFmtId="0" fontId="0" fillId="0" borderId="1" xfId="0" applyAlignment="1"><alignment horizontal="center" vertical="center"/></xf><xf numFmtId="0" fontId="1" fillId="2" borderId="1" xfId="0" applyAlignment="1"><alignment horizontal="center" vertical="center"/></xf><xf numFmtId="0" fontId="0" fillId="0" borderId="1" xfId="0" applyAlignment="1"><alignment horizontal="center" vertical="center"/></xf><xf numFmtId="2" fontId="0" fillId="0" borderId="1" xfId="0" applyNumberFormat="1" applyAlignment="1"><alignment horizontal="center" vertical="center"/></xf></cellXfs></styleSheet>)";
    return writeZip(filePath, {
        {"[Content_Types].xml", contentTypes}, {"_rels/.rels", rootRels},
        {"xl/workbook.xml", workbook}, {"xl/_rels/workbook.xml.rels", workbookRels},
        {"xl/styles.xml", styles}, {"xl/worksheets/sheet1.xml", worksheetXml(document, snapshot)}
    }, error);
}

} // namespace afs
