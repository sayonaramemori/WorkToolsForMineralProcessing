#pragma once

#include "topology/TopologyGraph.h"

#include <QVector>

class QGraphicsItem;

namespace afs {

class FlowsheetScene;

struct CanvasStreamDescriptor {
    topology::StreamId streamId;
    QString displayName;
    QGraphicsItem* graphicsItem{nullptr};
    bool mergeBranch{false};
};

struct CanvasTopologySnapshot {
    topology::TopologyGraph graph;
    QVector<CanvasStreamDescriptor> reportStreams;
    QVector<CanvasStreamDescriptor> productStreams;
    QVector<CanvasStreamDescriptor> terminalProducts;
    QVector<CanvasStreamDescriptor> requiredMeasurements;
};

class CanvasTopologyBuilder final {
public:
    [[nodiscard]] static CanvasTopologySnapshot build(const FlowsheetScene& scene);
};

} // namespace afs
