#pragma once

#include "topology/TopologyGraph.h"

namespace afs::topology {

class TopologyAlgorithms final {
public:
    TopologyAlgorithms() = delete;
    static TopologyOrder sort(const TopologyGraph& graph);
};

} // namespace afs::topology
