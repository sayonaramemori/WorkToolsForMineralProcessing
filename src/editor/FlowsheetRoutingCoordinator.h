#pragma once

class QGraphicsScene;

namespace afs {

// Coordinates geometry-only refreshes after a move, resize or manual route
// edit. It never creates or removes a topology relationship.
class FlowsheetRoutingCoordinator final {
public:
    FlowsheetRoutingCoordinator() = delete;
    static void refreshPaths(QGraphicsScene& scene);
    static void refreshAppearance(QGraphicsScene& scene);
};

} // namespace afs
