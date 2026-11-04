#pragma once

class QGraphicsScene;
class QPainter;

namespace afs {

// Visual-only orthogonal-line crossing renderer. It deliberately has no
// knowledge of topology: strict interior crossings receive bridges, including
// crossings between branches within one item. Bends and junction endpoints
// stay connected; rendering never creates ports, streams or junctions.
class CrossingBridgeRenderer final {
public:
    CrossingBridgeRenderer() = delete;
    static void draw(const QGraphicsScene& scene, QPainter& painter);
};

} // namespace afs
