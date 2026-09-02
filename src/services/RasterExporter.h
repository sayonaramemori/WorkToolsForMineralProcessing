#pragma once

#include <QByteArray>
#include <QString>

class QGraphicsScene;

namespace afs {

class RasterExporter final {
public:
    RasterExporter() = delete;
    static bool exportScene(QGraphicsScene& scene, const QString& filePath,
                            const QByteArray& format, qreal scaleFactor = 1.0);
};

} // namespace afs
