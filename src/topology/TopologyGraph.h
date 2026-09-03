#pragma once

#include "topology/TopologyTypes.h"

namespace afs::topology {

class TopologyGraph final {
public:
    bool addFlotationNode(FlotationNode node);
    bool addMergeNode(MergeNode node);
    bool addStream(MaterialStream stream);

    [[nodiscard]] bool containsNode(const NodeId& id) const;
    [[nodiscard]] NodeKind nodeKind(const NodeId& id) const;
    [[nodiscard]] const FlotationNode* flotationNode(const NodeId& id) const;
    [[nodiscard]] const MergeNode* mergeNode(const NodeId& id) const;
    [[nodiscard]] const MaterialStream* stream(const StreamId& id) const;

    [[nodiscard]] const QVector<NodeId>& nodeIds() const { return m_nodeOrder; }
    [[nodiscard]] const QVector<StreamId>& streamIds() const { return m_streamOrder; }
    [[nodiscard]] QVector<StreamId> streamsFrom(const NodeId& node, PortKind port) const;
    [[nodiscard]] QVector<StreamId> streamsTo(const NodeId& node, PortKind port) const;
    [[nodiscard]] QVector<StreamId> flotationProductStreams(const NodeId& node) const;
    [[nodiscard]] QVector<StreamId> externalFeedStreams() const;
    [[nodiscard]] QVector<StreamId> terminalProductStreams() const;

private:
    QHash<NodeId, FlotationNode> m_flotationNodes;
    QHash<NodeId, MergeNode> m_mergeNodes;
    QHash<StreamId, MaterialStream> m_streams;
    QVector<NodeId> m_nodeOrder;
    QVector<StreamId> m_streamOrder;
};

} // namespace afs::topology
