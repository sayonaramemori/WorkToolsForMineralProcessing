#include "services/SvgExporter.h"

#include <QGraphicsScene>
#include <QPainter>
#include <QSvgGenerator>

namespace afs {

bool SvgExporter::exportScene(QGraphicsScene& scene, const QString& filePath, const QString& title) {
    const QRectF source = scene.itemsBoundingRect().adjusted(-30, -30, 30, 30);
    if (source.isEmpty()) return false;
    QSvgGenerator generator;
    generator.setFileName(filePath);
    generator.setSize(source.size().toSize());
    generator.setViewBox(QRectF(QPointF(0, 0), source.size()));
    generator.setTitle(title);
    QPainter painter(&generator);
    if (!painter.isActive()) return false;
    painter.fillRect(QRectF(QPointF(0, 0), source.size()), Qt::white);
    scene.render(&painter, QRectF(QPointF(0, 0), source.size()), source);
    return true;
}

} // namespace afs
