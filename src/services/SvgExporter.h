#pragma once

#include <QString>

class QGraphicsScene;

namespace afs {

class SvgExporter final {
public:
    SvgExporter() = delete;
    static bool exportScene(QGraphicsScene& scene, const QString& filePath, const QString& title);
};

} // namespace afs
