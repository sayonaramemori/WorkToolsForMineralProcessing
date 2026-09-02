#include "services/RasterExporter.h"
#include "services/SvgExporter.h"

#include <QApplication>
#include <QFileInfo>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QImageReader>
#include <QPainter>
#include <QTemporaryDir>
#include <algorithm>

using namespace afs;

class FontMetricsProbe final : public QGraphicsItem {
public:
    QRectF boundingRect() const override { return {0, 0, 120, 50}; }
    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override {
        QFont font = QApplication::font();
        font.setPointSizeF(9.0);
        painter->setFont(font);
        paintedFontHeight = std::max(paintedFontHeight, painter->fontMetrics().height());
        painter->drawText(boundingRect(), Qt::AlignCenter, QString::fromUtf8("质量\n品位"));
    }
    int paintedFontHeight{0};
};

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QGraphicsScene scene;
    scene.addRect(QRectF(10, 20, 200, 100), QPen(Qt::black), QBrush(Qt::red));
    auto* probe = new FontMetricsProbe;
    probe->setPos(20, 30);
    scene.addItem(probe);

    QTemporaryDir directory;
    if (!directory.isValid()) return 1;
    const QString pngPath = directory.filePath("flowsheet.png");
    const QString jpgPath = directory.filePath("flowsheet.jpg");
    const QString svgPath = directory.filePath("flowsheet.svg");

    if (!RasterExporter::exportScene(scene, pngPath, "png", 4.0)) return 2;
    if (!RasterExporter::exportScene(scene, jpgPath, "jpeg")) return 3;
    if (!SvgExporter::exportScene(scene, svgPath, "export test")) return 4;

    QImageReader pngReader(pngPath);
    QImageReader jpgReader(jpgPath);
    if (!pngReader.canRead() || pngReader.format() != "png") return 5;
    if (!jpgReader.canRead() || jpgReader.format() != "jpeg") return 6;
    if (pngReader.size().isEmpty()
        || pngReader.size().width() != jpgReader.size().width() * 4
        || pngReader.size().height() != jpgReader.size().height() * 4) return 7;
    if (RasterExporter::exportScene(scene, directory.filePath("invalid.png"), "png", 0.0))
        return 8;
    if (QFileInfo(svgPath).size() <= 0) return 9;
    // Output scaling must not alter point-font layout in scene coordinates.
    if (probe->paintedFontHeight > 24) return 10;
    return 0;
}
