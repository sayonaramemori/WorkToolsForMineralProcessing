#include "adapters/CanvasTopologyBuilder.h"
#include "document/FlowsheetDocument.h"
#include "services/ExcelDataExporter.h"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>

using namespace afs;

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;

    CanvasTopologySnapshot snapshot;
    snapshot.reportStreams = {
        {"1:feed", "合计（单元 1 入料）", nullptr},
        {"1:left", "单元 1 - 左产品", nullptr},
        {"1:right", "单元 1 - 右产品", nullptr}
    };
    FlowsheetDocument document;
    document.setComponents({{"component-1", "Cu"}, {"component-2", "Zn"}});
    document.renameScenario(0, "制度 A");
    document.setProductName("1:left", "精矿");
    document.setExportStreamOrder({"1:right", "1:left", "1:feed"});
    topology::CalculationResult first;
    first.complete = true;
    first.values.insert("1:left", {56.5, 2.28825});
    first.values.insert("1:right", {212.2, 3.7135});
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

    const QString path = directory.filePath("schemes.xlsx");
    QString error;
    if (!ExcelDataExporter::exportWorkbook(document, snapshot, path, &error)) return 2;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return 3;
    const QByteArray bytes = file.readAll();
    if (!bytes.startsWith("PK\x03\x04") || !bytes.contains("xl/worksheets/sheet1.xml")
        || !bytes.contains(QString("制度 A").toUtf8())
        || !bytes.contains(QString("精矿").toUtf8())
        || !bytes.contains(QString("单元 1 - 右产品").toUtf8())
        || !bytes.contains(QString("Cu 品位 / %").toUtf8())
        || !bytes.contains(QString("Zn 回收率 / %").toUtf8())
        || bytes.contains(QString("合计（单元 1 入料）").toUtf8())
        || bytes.contains(QString("制度 C").toUtf8())
        || bytes.indexOf(QString("单元 1 - 右产品").toUtf8())
            > bytes.indexOf(QString("精矿").toUtf8())) return 4;
    return 0;
}
