#pragma once

class QGraphicsScene;
class QPainter;

namespace afs {

// Visual-only orthogonal-line crossing renderer. It deliberately has no
// knowledge of topology: crossing lines remain disconnected unless an editor
// command creates an explicit junction.
class CrossingBridgeRenderer final {
public:
    CrossingBridgeRenderer() = delete;
    static void draw(const QGraphicsScene& scene, QPainter& painter);
};

} // namespace afs
