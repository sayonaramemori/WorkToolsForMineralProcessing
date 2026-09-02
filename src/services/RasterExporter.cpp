#include "services/RasterExporter.h"

#include <QGraphicsScene>
#include <QImage>
#include <QImageWriter>
#include <QPainter>
#include <algorithm>
#include <cmath>

namespace afs {

bool RasterExporter::exportScene(QGraphicsScene& scene, const QString& filePath,
                                 const QByteArray& format, qreal scaleFactor) {
    const QRectF source = scene.itemsBoundingRect().adjusted(-30, -30, 30, 30);
    if (source.isEmpty() || !std::isfinite(scaleFactor) || scaleFactor <= 0.0) return false;

    constexpr qreal maximumDimension = 16384.0;
    constexpr qreal maximumPixelCount = 64.0 * 1024.0 * 1024.0;
    const qreal dimensionScale = maximumDimension / std::max(source.width(), source.height());
    const qreal areaScale = std::sqrt(maximumPixelCount / (source.width() * source.height()));
    const qreal scale = std::min({scaleFactor, dimensionScale, areaScale});
    const QSize imageSize(
        std::max(1, static_cast<int>(std::ceil(source.width() * scale))),
        std::max(1, static_cast<int>(std::ceil(source.height() * scale))));

    QImage image(imageSize, QImage::Format_ARGB32_Premultiplied);
    if (image.isNull()) return false;
    image.fill(Qt::white);

    QPainter painter(&image);
    if (!painter.isActive()) return false;
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    scene.render(&painter, QRectF(QPointF(0, 0), imageSize), source);
    painter.end();

    // DPI is output metadata. Setting it before scene.render() also changes
    // QPainter's point-font metrics, while item bounds were laid out at the
    // application's normal DPI. At 4x that made annotation text wrap and clip.
    const int dotsPerMeter = static_cast<int>(std::lround(96.0 * scale / 0.0254));
    image.setDotsPerMeterX(dotsPerMeter);
    image.setDotsPerMeterY(dotsPerMeter);

    QImageWriter writer(filePath, format);
    if (format.compare("jpeg", Qt::CaseInsensitive) == 0
        || format.compare("jpg", Qt::CaseInsensitive) == 0) {
        writer.setQuality(95);
    }
    return writer.write(image);
}

} // namespace afs
