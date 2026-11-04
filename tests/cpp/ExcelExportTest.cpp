#include "adapters/CanvasTopologyBuilder.h"
#include "document/FlowsheetDocument.h"
#include "services/ExcelDataExporter.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QTemporaryDir>
#include <QXmlStreamReader>
#include <QtEndian>
#include <stdexcept>

using namespace afs;

namespace {
void require(bool condition, const QString& message) {
    if (!condition) throw std::runtime_error(message.toStdString());
}

QByteArray readFile(const QString& path) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "Cannot read exported workbook");
    return file.readAll();
}

QByteArray worksheet(const QByteArray& zip) {
    qsizetype offset = 0;
    while (offset + 30 <= zip.size()
        && qFromLittleEndian<quint32>(zip.constData() + offset) == 0x04034b50) {
        const auto size = qFromLittleEndian<quint32>(zip.constData() + offset + 18);
        const auto nameSize = qFromLittleEndian<quint16>(zip.constData() + offset + 26);
        const auto extraSize = qFromLittleEndian<quint16>(zip.constData() + offset + 28);
        const auto start = offset + 30 + nameSize + extraSize;
        require(start + size <= zip.size(), "Truncated workbook ZIP entry");
        if (zip.mid(offset + 30, nameSize) == "xl/worksheets/sheet1.xml")
            return zip.mid(start, size);
        offset = start + size;
    }
    throw std::runtime_error("Workbook worksheet is missing");
}

struct Sheet {
    QHash<QString, QString> cells;
    QStringList merges;
    int rows{0};
};

Sheet exportSheet(const FlowsheetDocument& document, const CanvasTopologySnapshot& snapshot,
                  const QString& path) {
    QString error;
    require(ExcelDataExporter::exportWorkbook(document, snapshot, path, &error), error);
    const auto bytes = readFile(path);
    require(bytes.startsWith("PK\x03\x04"), "Export is not a ZIP workbook");
    QXmlStreamReader xml(worksheet(bytes));
    Sheet result;
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement()) continue;
        if (xml.name() == "row") ++result.rows;
        else if (xml.name() == "mergeCell") result.merges.append(xml.attributes().value("ref").toString());
        else if (xml.name() == "c") {
            const auto reference = xml.attributes().value("r").toString();
            result.cells.insert(reference, xml.readElementText(QXmlStreamReader::IncludeChildElements));
        }
    }
    require(!xml.hasError(), "Export contains invalid worksheet XML");
    return result;
}

void testExport(const QString& directory) {
    CanvasTopologySnapshot snapshot;
    snapshot.reportStreams = {
        {"1:feed", "合计（单元 1 入料）", nullptr},
        {"1:left", "单元 1 - 左产品", nullptr},
        {"1:right", "单元 1 - 右产品", nullptr},
        {"1:middle", "默认中间产品", nullptr},
        {"1:blank", "空白名称产品", nullptr},
        {"1:missing", "欠定物流", nullptr}
    };
    FlowsheetDocument document;
    document.setComponents({{"component-1", "Cu"}, {"component-2", "Zn"}});
    document.renameScenario(0, "制度 A");
    require(document.productName("1:right").isEmpty(), "Product names must default to empty");
    document.setProductName("1:left", "精矿");
    document.setProductName("1:feed", "总入料");
    document.setProductName("1:blank", " \t\n ");
    document.setProductName("1:missing", "尚无结果的产品");
    document.setProductName("deleted", "已删除产品");
    document.setExportStreamOrder({"deleted", "1:right", "1:left", "1:feed"});
    topology::CalculationResult first;
    first.complete = true;
    first.values.insert("1:feed", {268.7, 6.00175});
    first.values.insert("1:left", {56.5, 2.28825});
    first.values.insert("1:right", {212.2, 3.7135});
    first.values.insert("1:middle", {10, 1});
    first.values.insert("1:blank", {20, 2});
    first.relativeToExternalFeed.insert("1:left", {13.82, 23.97});
    first.relativeToExternalFeed.insert("1:right", {51.91, 38.89});
    first.components.insert("component-1", {first.values, {}, first.relativeToExternalFeed, true, true});
    topology::ComponentCalculationResult zinc;
    zinc.complete = zinc.fullySolved = true;
    zinc.values.insert("1:left", {56.5, 0.565});
    zinc.values.insert("1:right", {212.2, 1.061});
    zinc.relativeToExternalFeed.insert("1:left", {13.82, 18.5});
    zinc.relativeToExternalFeed.insert("1:right", {51.91, 34.7});
    first.components.insert("component-2", zinc);
    document.setCalculationResult(first);
    document.addScenario("制度 B", true);
    document.addScenario("制度 C", false);

    qInfo() << "Only named streams with results export, including explicitly named feed";
    const QString path = directory + "/schemes.xlsx";
    auto sheet = exportSheet(document, snapshot, path);
    require(sheet.rows == 5 && sheet.cells.value("B2") == "精矿" && sheet.cells.value("B3") == "总入料"
        && sheet.cells.value("A2") == "制度 A" && sheet.cells.value("A4") == "制度 B",
        "Unnamed streams were exported or named stream order changed");
    require(sheet.merges == QStringList{"A2:A3", "A4:A5"}, "Scenario merges include skipped rows");
    require(sheet.cells.value("E1") == "Cu 品位 / %" && sheet.cells.value("H1") == "Zn 回收率 / %"
        && sheet.cells.value("C2").toDouble() == 56.5 && sheet.cells.value("G2").toDouble() == 1.0,
        "Filtering changed exported component values");
    for (const auto& value : sheet.cells)
        require(value != "制度 C" && value != "尚无结果的产品" && value != "已删除产品"
            && value != "单元 1 - 右产品" && value != "默认中间产品" && value != "空白名称产品",
            "Export included unnamed, uncalculated or deleted product");

    qInfo() << "Naming, clearing and whitespace changes eligibility without default-name fallback";
    document.setProductName("1:right", "  尾矿  ");
    sheet = exportSheet(document, snapshot, path);
    require(sheet.rows == 7 && sheet.cells.value("B2") == "尾矿" && sheet.cells.value("B3") == "精矿"
        && sheet.cells.value("B4") == "总入料"
        && sheet.merges == QStringList{"A2:A4", "A5:A7"}, "New product name did not enable export");
    document.setProductName("1:left", " \t ");
    sheet = exportSheet(document, snapshot, path);
    require(sheet.rows == 5 && sheet.cells.value("B2") == "尾矿" && sheet.cells.value("B3") == "总入料",
            "Clearing a name did not remove its export row");
    const auto named = ExcelDataExporter::namedStreams(document, snapshot);
    require(named.size() == 3 && named.front().displayName == "尾矿"
        && snapshot.reportStreams.front().displayName == "合计（单元 1 入料）",
        "Export selection changed topology descriptions or used a default name");

    qInfo() << "No eligible rows produces an error without overwriting a workbook";
    const auto before = readFile(path);
    document.setProductName("1:right", {}); document.setProductName("1:feed", {});
    QString error;
    require(!ExcelDataExporter::exportWorkbook(document, snapshot, path, &error)
        && error.contains("计算结果") && readFile(path) == before, "No-result export overwrote existing file");
    document.setProductName("1:missing", {});
    error.clear();
    require(!ExcelDataExporter::exportWorkbook(document, snapshot, path, &error)
        && error.contains("产品名称") && readFile(path) == before, "Unnamed export overwrote existing file");
    const QString emptyPath = directory + "/empty.xlsx";
    require(!ExcelDataExporter::exportWorkbook(document, snapshot, emptyPath, &error)
        && !QFile::exists(emptyPath), "Unnamed export generated an empty workbook");

    // A partial calculation may still export the named values it did solve.
    document.setProductName("1:left", "部分可解产品");
    first.complete = false;
    document.setCalculationResult(first);
    sheet = exportSheet(document, snapshot, path);
    require(sheet.cells.value("B2") == "部分可解产品", "Named partial results should remain exportable");
}
} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;
    try {
        testExport(directory.path());
        qInfo() << "All Excel export checks passed";
    } catch (const std::exception& error) {
        qCritical().noquote() << error.what(); return 1;
    }
    return 0;
}
