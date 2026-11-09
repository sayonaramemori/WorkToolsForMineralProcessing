#include "services/XlsxWorkbookWriter.h"
#include <QDataStream>
#include <QFile>
#include <QObject>
#include <QVector>

namespace afs {
namespace {
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

} // namespace

bool XlsxWorkbookWriter::write(const QString& filePath, const QByteArray& worksheetXml, QString* error) {
    const QByteArray contentTypes = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/><Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/><Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/><Override PartName="/xl/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml"/></Types>)";
    const QByteArray rootRels = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/></Relationships>)";
    const QByteArray workbook = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?><workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><sheets><sheet name="方案数据" sheetId="1" r:id="rId1"/></sheets></workbook>)";
    const QByteArray workbookRels = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/><Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/></Relationships>)";
    const QByteArray styles = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?><styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><fonts count="2"><font><sz val="11"/><name val="Microsoft YaHei"/></font><font><b/><sz val="11"/><name val="Microsoft YaHei"/></font></fonts><fills count="3"><fill><patternFill patternType="none"/></fill><fill><patternFill patternType="gray125"/></fill><fill><patternFill patternType="solid"><fgColor rgb="FFD9EAF7"/><bgColor indexed="64"/></patternFill></fill></fills><borders count="2"><border/><border><left style="thin"/><right style="thin"/><top style="thin"/><bottom style="thin"/></border></borders><cellStyleXfs count="1"><xf numFmtId="0" fontId="0" fillId="0" borderId="0"/></cellStyleXfs><cellXfs count="5"><xf numFmtId="0" fontId="0" fillId="0" borderId="0" xfId="0"/><xf numFmtId="0" fontId="0" fillId="0" borderId="1" xfId="0" applyAlignment="1"><alignment horizontal="center" vertical="center"/></xf><xf numFmtId="0" fontId="1" fillId="2" borderId="1" xfId="0" applyAlignment="1"><alignment horizontal="center" vertical="center"/></xf><xf numFmtId="0" fontId="0" fillId="0" borderId="1" xfId="0" applyAlignment="1"><alignment horizontal="center" vertical="center"/></xf><xf numFmtId="2" fontId="0" fillId="0" borderId="1" xfId="0" applyNumberFormat="1" applyAlignment="1"><alignment horizontal="center" vertical="center"/></xf></cellXfs></styleSheet>)";
    return writeZip(filePath, {
        {"[Content_Types].xml", contentTypes}, {"_rels/.rels", rootRels},
        {"xl/workbook.xml", workbook}, {"xl/_rels/workbook.xml.rels", workbookRels},
        {"xl/styles.xml", styles}, {"xl/worksheets/sheet1.xml", worksheetXml}
    }, error);
}
} // namespace afs
