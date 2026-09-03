#include "topology/TopologyValidator.h"
#include "topology/TopologyAlgorithms.h"

#include <QQueue>
#include <QSet>
#include <cmath>

namespace afs::topology {
namespace {
void addError(QVector<TopologyIssue>& issues, IssueCode code,
              const QString& objectId, const QString& message) {
    issues.append({IssueSeverity::Error, code, objectId, message});
}

void addWarning(QVector<TopologyIssue>& issues, IssueCode code,
                const QString& objectId, const QString& message) {
    issues.append({IssueSeverity::Warning, code, objectId, message});
}

bool validSourcePort(NodeKind kind, PortKind port) {
    return kind == NodeKind::Flotation
        ? port == PortKind::LeftProduct || port == PortKind::RightProduct
        : port == PortKind::MergeOutput;
}

bool validTargetPort(NodeKind kind, PortKind port) {
    return kind == NodeKind::Flotation ? port == PortKind::Feed : port == PortKind::MergeInput;
}
}

QVector<TopologyIssue> TopologyValidator::validate(const TopologyGraph& graph) {
    QVector<TopologyIssue> issues;
    for (const auto& streamId : graph.streamIds()) {
        const auto* stream = graph.stream(streamId);
        if (!stream->source && !stream->target) {
            addError(issues, IssueCode::EmptyStream, streamId, QStringLiteral("物料流没有来源和去向"));
            continue;
        }
        if (stream->source) {
            if (!graph.containsNode(stream->source->nodeId)) {
                addError(issues, IssueCode::MissingNode, streamId, QStringLiteral("物料流来源节点不存在"));
            } else if (!validSourcePort(graph.nodeKind(stream->source->nodeId), stream->source->port)) {
                addError(issues, IssueCode::InvalidPort, streamId, QStringLiteral("物料流连接了无效的输出端口"));
            }
        }
        if (stream->target) {
            if (!graph.containsNode(stream->target->nodeId)) {
                addError(issues, IssueCode::MissingNode, streamId, QStringLiteral("物料流目标节点不存在"));
            } else if (!validTargetPort(graph.nodeKind(stream->target->nodeId), stream->target->port)) {
                addError(issues, IssueCode::InvalidPort, streamId, QStringLiteral("物料流连接了无效的输入端口"));
            }
        }
    }

    for (const auto& nodeId : graph.nodeIds()) {
        if (graph.nodeKind(nodeId) == NodeKind::Flotation) {
            const auto* flotation = graph.flotationNode(nodeId);
            if (flotation && flotation->leftSplitPercent
                && (!std::isfinite(*flotation->leftSplitPercent)
                    || *flotation->leftSplitPercent <= 0.0
                    || *flotation->leftSplitPercent >= 100.0))
                addError(issues, IssueCode::InvalidMeasurement, nodeId,
                         QStringLiteral("二分流器左支路比例必须大于 0% 且小于 100%"));
            if (graph.streamsTo(nodeId, PortKind::Feed).size() != 1)
                addError(issues, IssueCode::MissingFeed, nodeId, QStringLiteral("浮选单元必须恰好有一条入料流"));
            if (graph.streamsFrom(nodeId, PortKind::LeftProduct).size() != 1)
                addError(issues, IssueCode::MissingProduct, nodeId, QStringLiteral("浮选单元必须恰好有一条左产品流"));
            if (graph.streamsFrom(nodeId, PortKind::RightProduct).size() != 1)
                addError(issues, IssueCode::MissingProduct, nodeId, QStringLiteral("浮选单元必须恰好有一条右产品流"));
        } else {
            if (graph.streamsTo(nodeId, PortKind::MergeInput).size() < 2)
                addError(issues, IssueCode::InvalidMerge, nodeId, QStringLiteral("汇流节点至少需要两条输入流"));
            if (graph.streamsFrom(nodeId, PortKind::MergeOutput).size() != 1)
                addError(issues, IssueCode::InvalidMerge, nodeId, QStringLiteral("汇流节点必须恰好有一条输出流"));
        }
    }

    if (graph.externalFeedStreams().isEmpty())
        addError(issues, IssueCode::NoExternalFeed, {}, QStringLiteral("拓扑图没有外部入料流"));
    else if (graph.externalFeedStreams().size() != 1)
        addError(issues, IssueCode::MultipleExternalFeeds, {},
                 QStringLiteral("项目只允许一个主流程，必须恰好有一条外部入料流"));
    if (graph.terminalProductStreams().isEmpty())
        addError(issues, IssueCode::NoTerminalProduct, {}, QStringLiteral("拓扑图没有终端产品流"));
    if (TopologyAlgorithms::sort(graph).hasCycle)
        addWarning(issues, IssueCode::Cycle, {}, QStringLiteral("检测到闭路循环"));

    QSet<NodeId> reached;
    QQueue<NodeId> queue;
    for (const auto& streamId : graph.externalFeedStreams()) {
        const auto* stream = graph.stream(streamId);
        if (stream && stream->target && graph.containsNode(stream->target->nodeId)
            && !reached.contains(stream->target->nodeId)) {
            reached.insert(stream->target->nodeId);
            queue.enqueue(stream->target->nodeId);
        }
    }
    while (!queue.isEmpty()) {
        const auto node = queue.dequeue();
        for (const auto& streamId : graph.streamIds()) {
            const auto* stream = graph.stream(streamId);
            if (stream && stream->source && stream->target && stream->source->nodeId == node
                && graph.containsNode(stream->target->nodeId) && !reached.contains(stream->target->nodeId)) {
                reached.insert(stream->target->nodeId);
                queue.enqueue(stream->target->nodeId);
            }
        }
    }
    for (const auto& node : graph.nodeIds()) {
        if (!reached.contains(node))
            addError(issues, IssueCode::DisconnectedNode, node, QStringLiteral("节点不能从任何外部入料到达"));
    }
    return issues;
}

bool TopologyValidator::hasErrors(const QVector<TopologyIssue>& issues) {
    for (const auto& issue : issues) if (issue.severity == IssueSeverity::Error) return true;
    return false;
}

} // namespace afs::topology
