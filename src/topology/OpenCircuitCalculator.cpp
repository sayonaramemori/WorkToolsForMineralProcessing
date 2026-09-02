#include "topology/OpenCircuitCalculator.h"
#include "topology/TopologyValidator.h"

#include <algorithm>
#include <cmath>

namespace afs::topology {
namespace {
constexpr double kTolerance = 1e-9;

bool validValue(const StreamValue& value) {
    return std::isfinite(value.dryMass) && std::isfinite(value.componentMass)
        && value.dryMass >= 0.0 && value.componentMass >= 0.0
        && value.componentMass <= value.dryMass + kTolerance;
}

StreamValue add(const StreamValue& a, const StreamValue& b) {
    return {a.dryMass + b.dryMass, a.componentMass + b.componentMass};
}

std::optional<StreamValue> subtract(const StreamValue& total, const StreamValue& part) {
    StreamValue result{total.dryMass - part.dryMass, total.componentMass - part.componentMass};
    if (result.dryMass < -kTolerance || result.componentMass < -kTolerance
        || result.componentMass > result.dryMass + kTolerance) return std::nullopt;
    result.dryMass = std::max(0.0, result.dryMass);
    result.componentMass = std::max(0.0, result.componentMass);
    return result;
}

bool approximatelyEqual(const StreamValue& a, const StreamValue& b) {
    const double massScale = std::max({1.0, std::abs(a.dryMass), std::abs(b.dryMass)});
    const double componentScale = std::max({1.0, std::abs(a.componentMass), std::abs(b.componentMass)});
    return std::abs(a.dryMass - b.dryMass) <= 1e-7 * massScale
        && std::abs(a.componentMass - b.componentMass) <= 1e-7 * componentScale;
}

void addIssue(CalculationResult& result, IssueCode code,
              const QString& objectId, const QString& message) {
    result.issues.append({IssueSeverity::Error, code, objectId, message});
}

void addWarning(CalculationResult& result, IssueCode code,
                const QString& objectId, const QString& message) {
    result.issues.append({IssueSeverity::Warning, code, objectId, message});
}

void addIssueOnce(CalculationResult& result, IssueCode code,
                  const QString& objectId, const QString& message) {
    for (const auto& issue : result.issues)
        if (issue.code == code && issue.objectId == objectId) return;
    addIssue(result, code, objectId, message);
}

ProductMetrics metrics(const StreamValue& product, const StreamValue& feed) {
    return {
        feed.dryMass > 0.0 ? product.dryMass / feed.dryMass * 100.0 : 0.0,
        feed.componentMass > 0.0 ? product.componentMass / feed.componentMass * 100.0 : 0.0
    };
}
}

CalculationResult OpenCircuitCalculator::calculate(
    const TopologyGraph& graph,
    const QHash<StreamId, StreamValue>& knownValues,
    const QHash<StreamId, BranchAllocation>& allocations) {
    CalculationResult result;
    result.issues = TopologyValidator::validate(graph);
    if (TopologyValidator::hasErrors(result.issues)) return result;

    for (const auto& nodeId : graph.nodeIds()) {
        if (graph.nodeKind(nodeId) != NodeKind::Merge) continue;
        const auto inputs = graph.streamsTo(nodeId, PortKind::MergeInput);
        bool hasAllocation = false;
        int allocationCount = 0;
        double drySum = 0.0, componentSum = 0.0;
        for (const auto& input : inputs) {
            const auto it = allocations.constFind(input);
            if (it == allocations.cend()) continue;
            hasAllocation = true;
            ++allocationCount;
            if (!std::isfinite(it->dryMassSharePercent)
                || !std::isfinite(it->componentSharePercent)
                || it->dryMassSharePercent < 0.0 || it->componentSharePercent < 0.0) {
                addIssue(result, IssueCode::InvalidMeasurement, input,
                         QStringLiteral("合流支路占比无效"));
            }
            drySum += it->dryMassSharePercent;
            componentSum += it->componentSharePercent;
        }
        if (hasAllocation && (allocationCount != inputs.size()
            || std::abs(drySum - 100.0) > 1e-7
            || std::abs(componentSum - 100.0) > 1e-7))
            addIssue(result, IssueCode::InvalidMeasurement, nodeId,
                     QStringLiteral("合流节点的干质量占比和组分占比必须分别合计为 100%"));
    }
    if (TopologyValidator::hasErrors(result.issues)) return result;

    for (auto it = knownValues.cbegin(); it != knownValues.cend(); ++it) {
        if (!graph.stream(it.key())) {
            addIssue(result, IssueCode::MissingMeasurement, it.key(), QStringLiteral("实测值对应的物料流不存在"));
        } else if (!validValue(it.value())) {
            addIssue(result, IssueCode::InvalidMeasurement, it.key(), QStringLiteral("质量或品位数据无效"));
        } else {
            result.values.insert(it.key(), it.value());
        }
    }
    if (TopologyValidator::hasErrors(result.issues)) return result;

    // The external feed is identifiable from the overall boundary even when
    // allocations inside a terminal product merge are not unique.
    const auto terminalIds = graph.terminalProductStreams();
    const auto externalIds = graph.externalFeedStreams();
    bool allTerminalValuesKnown = !terminalIds.isEmpty();
    StreamValue terminalSum;
    for (const auto& streamId : terminalIds) {
        if (!result.values.contains(streamId)) { allTerminalValuesKnown = false; break; }
        terminalSum = add(terminalSum, result.values[streamId]);
    }
    if (allTerminalValuesKnown && externalIds.size() == 1
        && !result.values.contains(externalIds.front()))
        result.values.insert(externalIds.front(), terminalSum);

    bool changed = true;
    int passes = 0;
    const int maximumPasses = std::max(1, static_cast<int>(graph.streamIds().size()) * 2);
    while (changed && passes++ < maximumPasses) {
        changed = false;
        for (const auto& nodeId : graph.nodeIds()) {
            if (graph.nodeKind(nodeId) == NodeKind::Flotation) {
                const auto feedId = graph.streamsTo(nodeId, PortKind::Feed).front();
                const auto leftId = graph.streamsFrom(nodeId, PortKind::LeftProduct).front();
                const auto rightId = graph.streamsFrom(nodeId, PortKind::RightProduct).front();
                const bool hasFeed = result.values.contains(feedId);
                const bool hasLeft = result.values.contains(leftId);
                const bool hasRight = result.values.contains(rightId);
                if (!hasFeed && hasLeft && hasRight) {
                    result.values.insert(feedId, add(result.values[leftId], result.values[rightId]));
                    changed = true;
                } else if (hasFeed && hasLeft && !hasRight) {
                    if (auto value = subtract(result.values[feedId], result.values[leftId])) {
                        result.values.insert(rightId, *value); changed = true;
                    } else addIssueOnce(result, IssueCode::InconsistentBalance, nodeId,
                                        QStringLiteral("已知入料小于已知产品，无法满足浮选单元守恒"));
                } else if (hasFeed && !hasLeft && hasRight) {
                    if (auto value = subtract(result.values[feedId], result.values[rightId])) {
                        result.values.insert(leftId, *value); changed = true;
                    } else addIssueOnce(result, IssueCode::InconsistentBalance, nodeId,
                                        QStringLiteral("已知入料小于已知产品，无法满足浮选单元守恒"));
                }
            } else {
                const auto inputIds = graph.streamsTo(nodeId, PortKind::MergeInput);
                const auto outputId = graph.streamsFrom(nodeId, PortKind::MergeOutput).front();
                if (result.values.contains(outputId)) {
                    for (const auto& input : inputIds) {
                        if (result.values.contains(input)) continue;
                        const auto allocation = allocations.constFind(input);
                        if (allocation == allocations.cend()) continue;
                        const auto output = result.values[outputId];
                        result.values.insert(input, {
                            output.dryMass * allocation->dryMassSharePercent / 100.0,
                            output.componentMass * allocation->componentSharePercent / 100.0});
                        changed = true;
                    }
                }
                int missingInputs = 0;
                StreamId missingId;
                StreamValue inputSum;
                for (const auto& input : inputIds) {
                    if (result.values.contains(input)) inputSum = add(inputSum, result.values[input]);
                    else { ++missingInputs; missingId = input; }
                }
                if (!result.values.contains(outputId) && missingInputs == 0) {
                    result.values.insert(outputId, inputSum); changed = true;
                } else if (result.values.contains(outputId) && missingInputs == 1) {
                    if (auto value = subtract(result.values[outputId], inputSum)) {
                        result.values.insert(missingId, *value); changed = true;
                    } else addIssueOnce(result, IssueCode::InconsistentBalance, nodeId,
                                        QStringLiteral("汇流输出小于已知输入之和，无法满足汇流守恒"));
                }
            }
        }
    }

    for (const auto& nodeId : graph.nodeIds()) {
        if (graph.nodeKind(nodeId) == NodeKind::Flotation) {
            const auto feedId = graph.streamsTo(nodeId, PortKind::Feed).front();
            const auto leftId = graph.streamsFrom(nodeId, PortKind::LeftProduct).front();
            const auto rightId = graph.streamsFrom(nodeId, PortKind::RightProduct).front();
            if (result.values.contains(feedId) && result.values.contains(leftId) && result.values.contains(rightId)) {
                const auto outputSum = add(result.values[leftId], result.values[rightId]);
                if (!approximatelyEqual(result.values[feedId], outputSum))
                    addIssue(result, IssueCode::InconsistentBalance, nodeId, QStringLiteral("浮选单元质量或组分不平衡"));
                result.flotationPerformance.insert(nodeId, {
                    metrics(result.values[leftId], result.values[feedId]),
                    metrics(result.values[rightId], result.values[feedId])
                });
            }
        } else {
            const auto inputIds = graph.streamsTo(nodeId, PortKind::MergeInput);
            const auto outputId = graph.streamsFrom(nodeId, PortKind::MergeOutput).front();
            if (result.values.contains(outputId)) {
                StreamValue sum;
                bool completeInputs = true;
                for (const auto& input : inputIds) {
                    if (!result.values.contains(input)) { completeInputs = false; break; }
                    sum = add(sum, result.values[input]);
                }
                if (completeInputs && !approximatelyEqual(sum, result.values[outputId]))
                    addIssue(result, IssueCode::InconsistentBalance, nodeId, QStringLiteral("汇流节点质量或组分不平衡"));
            }
        }
    }

    for (const auto& streamId : graph.streamIds()) {
        if (!result.values.contains(streamId))
            addWarning(result, IssueCode::Underdetermined, streamId,
                       QStringLiteral("中间物料流未唯一确定"));
    }
    if (graph.externalFeedStreams().size() == 1) {
        const auto feedId = graph.externalFeedStreams().front();
        if (result.values.contains(feedId)) {
            const auto feed = result.values[feedId];
            for (auto it = result.values.cbegin(); it != result.values.cend(); ++it)
                result.relativeToExternalFeed.insert(it.key(), metrics(it.value(), feed));
        }
    }
    allTerminalValuesKnown = true;
    for (const auto& streamId : terminalIds)
        allTerminalValuesKnown = allTerminalValuesKnown && result.values.contains(streamId);
    const bool externalFeedKnown = externalIds.size() == 1
        && result.values.contains(externalIds.front());
    result.fullySolved = !TopologyValidator::hasErrors(result.issues)
        && result.values.size() == graph.streamIds().size();
    result.complete = !TopologyValidator::hasErrors(result.issues)
        && allTerminalValuesKnown && externalFeedKnown;
    return result;
}

} // namespace afs::topology
