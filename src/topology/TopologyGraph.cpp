#include "topology/TopologyGraph.h"

namespace afs::topology {

bool TopologyGraph::addFlotationNode(FlotationNode node) {
    if (node.id.isEmpty() || containsNode(node.id)) return false;
    m_nodeOrder.append(node.id);
    m_flotationNodes.insert(node.id, std::move(node));
    return true;
}

bool TopologyGraph::addMergeNode(MergeNode node) {
    if (node.id.isEmpty() || containsNode(node.id)) return false;
    m_nodeOrder.append(node.id);
    m_mergeNodes.insert(node.id, std::move(node));
    return true;
}

bool TopologyGraph::addStream(MaterialStream streamValue) {
    if (streamValue.id.isEmpty() || m_streams.contains(streamValue.id)) return false;
    m_streamOrder.append(streamValue.id);
    m_streams.insert(streamValue.id, std::move(streamValue));
    return true;
}

bool TopologyGraph::containsNode(const NodeId& id) const {
    return m_flotationNodes.contains(id) || m_mergeNodes.contains(id);
}

NodeKind TopologyGraph::nodeKind(const NodeId& id) const {
    return m_mergeNodes.contains(id) ? NodeKind::Merge : NodeKind::Flotation;
}

const FlotationNode* TopologyGraph::flotationNode(const NodeId& id) const {
    const auto it = m_flotationNodes.constFind(id);
    return it == m_flotationNodes.cend() ? nullptr : &it.value();
}

const MergeNode* TopologyGraph::mergeNode(const NodeId& id) const {
    const auto it = m_mergeNodes.constFind(id);
    return it == m_mergeNodes.cend() ? nullptr : &it.value();
}

const MaterialStream* TopologyGraph::stream(const StreamId& id) const {
    const auto it = m_streams.constFind(id);
    return it == m_streams.cend() ? nullptr : &it.value();
}

QVector<StreamId> TopologyGraph::streamsFrom(const NodeId& node, PortKind port) const {
    QVector<StreamId> result;
    for (const auto& id : m_streamOrder) {
        const auto& item = m_streams[id];
        if (item.source && item.source->nodeId == node && item.source->port == port) result.append(id);
    }
    return result;
}

QVector<StreamId> TopologyGraph::streamsTo(const NodeId& node, PortKind port) const {
    QVector<StreamId> result;
    for (const auto& id : m_streamOrder) {
        const auto& item = m_streams[id];
        if (item.target && item.target->nodeId == node && item.target->port == port) result.append(id);
    }
    return result;
}

QVector<StreamId> TopologyGraph::externalFeedStreams() const {
    QVector<StreamId> result;
    for (const auto& id : m_streamOrder) if (!m_streams[id].source && m_streams[id].target) result.append(id);
    return result;
}

QVector<StreamId> TopologyGraph::terminalProductStreams() const {
    QVector<StreamId> result;
    for (const auto& id : m_streamOrder) if (m_streams[id].source && !m_streams[id].target) result.append(id);
    return result;
}

} // namespace afs::topology
