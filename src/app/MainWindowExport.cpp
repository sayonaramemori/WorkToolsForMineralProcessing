#include "app/MainWindow.h"
#include "adapters/CanvasTopologyBuilder.h"
#include "document/FlowsheetDocument.h"
#include "editor/FlowsheetScene.h"
#include "services/ExcelDataExporter.h"
#include "services/RasterExporter.h"
#include "services/SvgExporter.h"
#include "ui/StreamOrderDialog.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsScene>
#include <QInputDialog>
#include <QMessageBox>
#include <QStatusBar>

namespace afs {

void MainWindow::exportScene() {
    const QString svgFilter = tr("SVG 文件 (*.svg)");
    const QString pngFilter = tr("PNG 图像 (*.png)");
    const QString jpegFilter = tr("JPEG 图像 (*.jpg *.jpeg)");
    QString selectedFilter = svgFilter;
    QString path = QFileDialog::getSaveFileName(this, tr("导出流程图"),
        "flotation-flowsheet", QStringList{svgFilter, pngFilter, jpegFilter}.join(";;"),
        &selectedFilter);
    if (path.isEmpty()) return;
    QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix.isEmpty()) {
        suffix = selectedFilter == pngFilter ? "png"
            : selectedFilter == jpegFilter ? "jpg" : "svg";
        path += "." + suffix;
    }
    qreal rasterScale = 1.0;
    if (suffix == "png" || suffix == "jpg" || suffix == "jpeg") {
        const QStringList qualities{tr("标准（1×）"), tr("高清（2×，推荐）"),
                                    tr("超清（3×）"), tr("超清（4×）")};
        bool accepted = false;
        const QString quality = QInputDialog::getItem(this, tr("栅格图像清晰度"),
            tr("选择输出倍率；倍率越高，文字越清晰，文件也越大。"),
            qualities, 1, false, &accepted);
        if (!accepted) return;
        rasterScale = quality == qualities[0] ? 1.0
            : quality == qualities[1] ? 2.0 : quality == qualities[2] ? 3.0 : 4.0;
    }
    const bool screenWasDark = m_darkTheme;
    if (screenWasDark) applyTheme(false, false);
    const QBrush screenBackground = m_scene->backgroundBrush();
    m_scene->setBackgroundBrush(Qt::white);
    bool exported = false;
    if (suffix == "svg")
        exported = SvgExporter::exportScene(*m_scene, path, tr("浮选工艺流程图"));
    else if (suffix == "png")
        exported = RasterExporter::exportScene(*m_scene, path, "png", rasterScale);
    else if (suffix == "jpg" || suffix == "jpeg")
        exported = RasterExporter::exportScene(*m_scene, path, "jpeg", rasterScale);
    m_scene->setBackgroundBrush(screenBackground);
    if (screenWasDark) applyTheme(true, false);
    statusBar()->showMessage(exported ? tr("已导出：%1").arg(path)
                                      : tr("导出失败，请检查文件格式和保存路径"), 8000);
}

void MainWindow::exportExcelData() {
    const auto snapshot = CanvasTopologyBuilder::build(*static_cast<FlowsheetScene*>(m_scene));
    if (ExcelDataExporter::namedStreams(*m_document, snapshot).isEmpty()) {
        QMessageBox::information(this, tr("导出 Excel"),
            tr("当前流程没有已命名的产品，请先在右侧表格填写产品名称。"));
        return;
    }
    QString path = QFileDialog::getSaveFileName(this, tr("导出方案数据"),
        "flotation-scenarios.xlsx", tr("Excel 工作簿 (*.xlsx)"));
    if (path.isEmpty()) return;
    if (!path.endsWith(".xlsx", Qt::CaseInsensitive)) path += ".xlsx";
    QString error;
    if (!ExcelDataExporter::exportWorkbook(*m_document, snapshot, path, &error)) {
        QMessageBox::warning(this, tr("导出 Excel 失败"), error); return;
    }
    statusBar()->showMessage(tr("方案数据已导出：%1").arg(path), 8000);
}

void MainWindow::editExcelExportOrder() {
    const auto snapshot = CanvasTopologyBuilder::build(*static_cast<FlowsheetScene*>(m_scene));
    const auto streams = ExcelDataExporter::namedStreams(*m_document, snapshot);
    if (streams.isEmpty()) {
        QMessageBox::information(this, tr("Excel 导出顺序"),
            tr("当前流程没有已命名的产品，请先在右侧表格填写产品名称。"));
        return;
    }
    QVector<StreamOrderEntry> ordered;
    for (const auto& stream : streams) ordered.append({stream.streamId, stream.displayName});
    auto order = editStreamOrderDialog(this, ordered, [this, snapshot] {
        QVector<StreamOrderEntry> entries;
        for (const auto& stream : snapshot.reportStreams) {
            const auto name = m_document->productName(stream.streamId).simplified();
            if (!name.isEmpty()) entries.append({stream.streamId, name});
        }
        return entries;
    });
    if (!order) return;
    m_document->setExportStreamOrder(std::move(*order));
    statusBar()->showMessage(tr("Excel 导出顺序已更新"), 3000);
}

} // namespace afs
