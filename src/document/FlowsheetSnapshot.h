#pragma once
#include "document/StreamDescriptor.h"
#include "topology/TopologyGraph.h"
#include <QVector>

namespace afs {
// Detached values: safe to calculate, export or inspect without a live canvas.
struct FlowsheetSnapshot {
    topology::TopologyGraph graph;
    QVector<StreamDescriptor> reportStreams;
    QVector<StreamDescriptor> productStreams;
    QVector<StreamDescriptor> terminalProducts;
};
} // namespace afs
