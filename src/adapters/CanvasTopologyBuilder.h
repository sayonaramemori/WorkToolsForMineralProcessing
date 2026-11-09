#pragma once

#include "topology/TopologyGraph.h"

#include <QVector>
#include <QStringList>

class QGraphicsItem;

namespace afs {

class FlowsheetScene;

struct CanvasStreamDescriptor {
    topology::StreamId streamId;
    QString displayName;
    QGraphicsItem* graphicsItem{nullptr};
    bool mergeBranch{false};
    bool terminal{false};
    bool feed{false};
    bool recycle{false};
    QStringList ownerIds;
};

struct CanvasTopologySnapshot {
    topology::TopologyGraph graph;
    QVector<CanvasStreamDescriptor> reportStreams;
    QVector<CanvasStreamDescriptor> productStreams;
    QVector<CanvasStreamDescriptor> terminalProducts;
};

class CanvasTopologyBuilder final {
public:
    [[nodiscard]] static CanvasTopologySnapshot build(const FlowsheetScene& scene);
};

} // namespace afs
