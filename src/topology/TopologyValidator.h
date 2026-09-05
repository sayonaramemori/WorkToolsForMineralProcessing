#pragma once

#include "topology/TopologyGraph.h"

namespace afs::topology {

class TopologyValidator final {
public:
    TopologyValidator() = delete;
    static QVector<TopologyIssue> validate(const TopologyGraph& graph,
                                           bool allowMultipleExternalFeeds = false);
    static bool hasErrors(const QVector<TopologyIssue>& issues);
};

} // namespace afs::topology
