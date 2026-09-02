#include "topology/TopologyAlgorithms.h"

#include <QQueue>
#include <algorithm>

namespace afs::topology {

TopologyOrder TopologyAlgorithms::sort(const TopologyGraph& graph) {
    QHash<NodeId, int> indegree;
    QHash<NodeId, QVector<NodeId>> adjacency;
    for (const auto& node : graph.nodeIds()) indegree.insert(node, 0);
    for (const auto& streamId : graph.streamIds()) {
        const auto* stream = graph.stream(streamId);
        if (!stream || !stream->source || !stream->target) continue;
        const auto& from = stream->source->nodeId;
        const auto& to = stream->target->nodeId;
        if (!graph.containsNode(from) || !graph.containsNode(to)) continue;
        adjacency[from].append(to);
        ++indegree[to];
    }

    QQueue<NodeId> ready;
    for (const auto& node : graph.nodeIds()) if (indegree[node] == 0) ready.enqueue(node);
    TopologyOrder result;
    while (!ready.isEmpty()) {
        const NodeId node = ready.dequeue();
        result.forward.append(node);
        for (const auto& next : adjacency[node]) {
            if (--indegree[next] == 0) ready.enqueue(next);
        }
    }
    result.hasCycle = result.forward.size() != graph.nodeIds().size();
    result.reverse = result.forward;
    std::reverse(result.reverse.begin(), result.reverse.end());
    return result;
}

} // namespace afs::topology
