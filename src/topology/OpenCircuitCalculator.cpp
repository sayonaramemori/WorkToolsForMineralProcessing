#include "topology/OpenCircuitCalculator.h"
#include "topology/TopologyValidator.h"

#include <algorithm>
#include <cmath>
#include <QSet>

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

ProductMetrics metrics(const StreamValue& product, const StreamValue& feed) {
    return {
        feed.dryMass > 0.0 ? product.dryMass / feed.dryMass * 100.0 : 0.0,
        feed.componentMass > 0.0 ? product.componentMass / feed.componentMass * 100.0 : 0.0
    };
}

struct ScalarSolution {
    QVector<std::optional<double>> values;
    bool inconsistent{false};
};

ScalarSolution solveScalarSystem(
    const TopologyGraph& graph,
    const QHash<StreamId, StreamValue>& knownValues,
    const QHash<StreamId, BranchAllocation>& allocations,
    bool componentMass) {
    const auto streamIds = graph.streamIds();
    const int variableCount = streamIds.size();
    QHash<StreamId, int> columns;
    for (int index = 0; index < variableCount; ++index) columns.insert(streamIds[index], index);
    QVector<QVector<double>> matrix;
    const auto equation = [variableCount] { return QVector<double>(variableCount + 1, 0.0); };

    for (const auto& nodeId : graph.nodeIds()) {
        auto row = equation();
        if (graph.nodeKind(nodeId) == NodeKind::Flotation) {
            for (const auto& id : graph.streamsTo(nodeId, PortKind::Feed)) row[columns[id]] += 1.0;
            for (const auto& id : graph.streamsFrom(nodeId, PortKind::LeftProduct)) row[columns[id]] -= 1.0;
            for (const auto& id : graph.streamsFrom(nodeId, PortKind::MiddleProduct)) row[columns[id]] -= 1.0;
            for (const auto& id : graph.streamsFrom(nodeId, PortKind::RightProduct)) row[columns[id]] -= 1.0;
        } else {
            for (const auto& id : graph.streamsTo(nodeId, PortKind::MergeInput)) row[columns[id]] += 1.0;
            for (const auto& id : graph.streamsFrom(nodeId, PortKind::MergeOutput)) row[columns[id]] -= 1.0;
        }
        matrix.append(std::move(row));
        if (graph.nodeKind(nodeId) == NodeKind::Flotation) {
            const auto* node = graph.flotationNode(nodeId);
            if (node && node->leftSplitPercent) {
                const auto feedIds = graph.streamsTo(nodeId, PortKind::Feed);
                const auto leftIds = graph.streamsFrom(nodeId, PortKind::LeftProduct);
                const auto rightIds = graph.streamsFrom(nodeId, PortKind::RightProduct);
                if (!feedIds.isEmpty() && !leftIds.isEmpty() && !rightIds.isEmpty()) {
                    auto leftRow = equation();
                    leftRow[columns[leftIds.front()]] = 1.0;
                    leftRow[columns[feedIds.front()]] = -*node->leftSplitPercent / 100.0;
                    matrix.append(std::move(leftRow));
                    auto rightRow = equation();
                    rightRow[columns[rightIds.front()]] = 1.0;
                    rightRow[columns[feedIds.front()]] =
                        -(100.0 - *node->leftSplitPercent) / 100.0;
                    matrix.append(std::move(rightRow));
                }
            }
        }
    }
    for (auto it = knownValues.cbegin(); it != knownValues.cend(); ++it) {
        auto row = equation();
        row[columns[it.key()]] = 1.0;
        row[variableCount] = componentMass ? it->componentMass : it->dryMass;
        matrix.append(std::move(row));
    }
    for (const auto& nodeId : graph.nodeIds()) {
        if (graph.nodeKind(nodeId) != NodeKind::Merge) continue;
        const auto outputIds = graph.streamsFrom(nodeId, PortKind::MergeOutput);
        if (outputIds.isEmpty()) continue;
        const int outputColumn = columns[outputIds.front()];
        for (const auto& inputId : graph.streamsTo(nodeId, PortKind::MergeInput)) {
            const auto allocation = allocations.constFind(inputId);
            if (allocation == allocations.cend()) continue;
            auto row = equation();
            row[columns[inputId]] = 1.0;
            const double percent = componentMass ? allocation->componentSharePercent
                                                 : allocation->dryMassSharePercent;
            row[outputColumn] = -percent / 100.0;
            matrix.append(std::move(row));
        }
    }

    constexpr double epsilon = 1e-10;
    QVector<int> pivotColumns;
    int pivotRow = 0;
    for (int column = 0; column < variableCount && pivotRow < matrix.size(); ++column) {
        int best = pivotRow;
        for (int row = pivotRow + 1; row < matrix.size(); ++row)
            if (std::abs(matrix[row][column]) > std::abs(matrix[best][column])) best = row;
        if (std::abs(matrix[best][column]) <= epsilon) continue;
        if (best != pivotRow) matrix.swapItemsAt(best, pivotRow);
        const double pivot = matrix[pivotRow][column];
        for (int index = column; index <= variableCount; ++index) matrix[pivotRow][index] /= pivot;
        for (int row = 0; row < matrix.size(); ++row) {
            if (row == pivotRow || std::abs(matrix[row][column]) <= epsilon) continue;
            const double factor = matrix[row][column];
            for (int index = column; index <= variableCount; ++index)
                matrix[row][index] -= factor * matrix[pivotRow][index];
        }
        pivotColumns.append(column);
        ++pivotRow;
    }

    ScalarSolution result{QVector<std::optional<double>>(variableCount)};
    for (const auto& row : matrix) {
        bool zero = true;
        for (int column = 0; column < variableCount; ++column)
            zero = zero && std::abs(row[column]) <= epsilon;
        if (zero && std::abs(row[variableCount]) > 1e-8) {
            result.inconsistent = true;
            return result;
        }
    }
    QSet<int> pivots(pivotColumns.cbegin(), pivotColumns.cend());
    for (int row = 0; row < pivotColumns.size(); ++row) {
        bool dependsOnFreeVariable = false;
        for (int column = 0; column < variableCount; ++column)
            if (!pivots.contains(column) && std::abs(matrix[row][column]) > epsilon) {
                dependsOnFreeVariable = true;
                break;
            }
        if (!dependsOnFreeVariable) {
            double value = matrix[row][variableCount];
            if (std::abs(value) <= epsilon) value = 0.0;
            result.values[pivotColumns[row]] = value;
        }
    }
    return result;
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

    const auto terminalIds = graph.terminalProductStreams();
    const auto externalIds = graph.externalFeedStreams();
    const auto drySolution = solveScalarSystem(graph, knownValues, allocations, false);
    const auto componentSolution = solveScalarSystem(graph, knownValues, allocations, true);
    if (drySolution.inconsistent || componentSolution.inconsistent) {
        addIssue(result, IssueCode::InconsistentBalance, QStringLiteral("equation-system"),
                 QStringLiteral("实测值、支路占比与流程守恒方程相互矛盾"));
        result.values.clear();
        return result;
    }
    result.values.clear();
    const auto streamIds = graph.streamIds();
    for (int index = 0; index < streamIds.size(); ++index) {
        if (!drySolution.values[index] || !componentSolution.values[index]) continue;
        StreamValue value{*drySolution.values[index], *componentSolution.values[index]};
        if (value.dryMass < 0.0 && value.dryMass > -1e-8) value.dryMass = 0.0;
        if (value.componentMass < 0.0 && value.componentMass > -1e-8) value.componentMass = 0.0;
        if (value.componentMass > value.dryMass
            && value.componentMass - value.dryMass < 1e-8)
            value.componentMass = value.dryMass;
        if (!validValue(value)) {
            const auto* stream = graph.stream(streamIds[index]);
            const QString objectId = stream && stream->source ? stream->source->nodeId
                : stream && stream->target ? stream->target->nodeId : streamIds[index];
            addIssue(result, IssueCode::InconsistentBalance, objectId,
                     QStringLiteral("方程解得到负质量或组分质量超出总质量"));
            continue;
        }
        result.values.insert(streamIds[index], value);
    }

    for (const auto& nodeId : graph.nodeIds()) {
        if (graph.nodeKind(nodeId) == NodeKind::Flotation) {
            const auto feedId = graph.streamsTo(nodeId, PortKind::Feed).front();
            const auto leftId = graph.streamsFrom(nodeId, PortKind::LeftProduct).front();
            const auto rightId = graph.streamsFrom(nodeId, PortKind::RightProduct).front();
            const auto middleIds = graph.streamsFrom(nodeId, PortKind::MiddleProduct);
            const bool middleComplete = middleIds.isEmpty() || result.values.contains(middleIds.front());
            if (result.values.contains(feedId) && result.values.contains(leftId)
                && result.values.contains(rightId) && middleComplete) {
                auto outputSum = add(result.values[leftId], result.values[rightId]);
                if (!middleIds.isEmpty()) outputSum = add(outputSum, result.values[middleIds.front()]);
                if (!approximatelyEqual(result.values[feedId], outputSum))
                    addIssue(result, IssueCode::InconsistentBalance, nodeId, QStringLiteral("浮选单元质量或组分不平衡"));
                const auto* node = graph.flotationNode(nodeId);
                if (!node || !node->leftSplitPercent) {
                    FlotationPerformance performance{
                        metrics(result.values[leftId], result.values[feedId]),
                        metrics(result.values[rightId], result.values[feedId])};
                    if (!middleIds.isEmpty())
                        performance.middle = metrics(result.values[middleIds.front()], result.values[feedId]);
                    result.flotationPerformance.insert(nodeId, performance);
                }
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
    bool allTerminalValuesKnown = true;
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
