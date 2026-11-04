#include "topology/OpenCircuitCalculator.h"
#include "topology/BalanceEquationSolver.h"
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

}

CalculationResult OpenCircuitCalculator::calculate(
    const TopologyGraph& graph,
    const QHash<StreamId, StreamValue>& knownValues,
    const QHash<StreamId, BranchAllocation>& allocations,
    bool scopedCalculation,
    const QVector<LinearBalanceConstraint>& constraints,
    const QHash<StreamId, StreamUncertainty>& uncertainties) {
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
    for (const auto& constraint : constraints) {
        for (auto it = constraint.coefficients.cbegin();
             it != constraint.coefficients.cend(); ++it) {
            if (!graph.stream(it.key()) || !std::isfinite(it.value())) {
                addIssue(result, IssueCode::InvalidMeasurement, constraint.id,
                         QStringLiteral("附加平衡约束引用了无效物流或系数"));
                break;
            }
        }
        if (!validValue(constraint.rightHandSide))
            addIssue(result, IssueCode::InvalidMeasurement, constraint.id,
                     QStringLiteral("附加平衡约束的边界值无效"));
    }
    if (TopologyValidator::hasErrors(result.issues)) return result;
    const auto drySolution = calculation_detail::solveScalarSystem(graph, knownValues, allocations, constraints, false,
                                               uncertainties);
    const auto componentSolution = calculation_detail::solveScalarSystem(graph, knownValues, allocations, constraints, true,
                                                     uncertainties);
    result.reconciled = !uncertainties.isEmpty();
    result.dryMassDegreesOfFreedom = drySolution.degreesOfFreedom;
    result.componentMassDegreesOfFreedom = componentSolution.degreesOfFreedom;
    if (!uncertainties.isEmpty()) {
        result.dryMassDegreesOfFreedom = std::count_if(
            drySolution.values.cbegin(), drySolution.values.cend(),
            [](const auto& value) { return !value.has_value(); });
        result.componentMassDegreesOfFreedom = std::count_if(
            componentSolution.values.cbegin(), componentSolution.values.cend(),
            [](const auto& value) { return !value.has_value(); });
    }
    if (drySolution.inconsistent || componentSolution.inconsistent) {
        addIssue(result, IssueCode::InconsistentBalance, QStringLiteral("equation-system"),
                 QStringLiteral("实测值、支路占比与流程守恒方程相互矛盾"));
        result.values.clear();
        return result;
    }
    result.values.clear();
    bool physicalSolutionInvalid = false;
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
            physicalSolutionInvalid = true;
            continue;
        }
        result.values.insert(streamIds[index], value);
    }
    if (result.reconciled) {
        for (auto it = knownValues.cbegin(); it != knownValues.cend(); ++it) {
            if (!result.values.contains(it.key()) || !uncertainties.contains(it.key())) continue;
            const auto delta = StreamValue{result.values[it.key()].dryMass - it->dryMass,
                result.values[it.key()].componentMass - it->componentMass};
            const auto sigma = uncertainties.value(it.key());
            ReconciliationResidual residual{delta.dryMass, delta.componentMass,
                delta.dryMass / sigma.dryMassStdDev,
                delta.componentMass / sigma.componentMassStdDev};
            result.maximumAbsoluteStandardizedResidual = std::max({
                result.maximumAbsoluteStandardizedResidual,
                std::abs(residual.dryMassStandardized),
                std::abs(residual.componentMassStandardized)});
            result.residuals.insert(it.key(), residual);
        }
        if (result.maximumAbsoluteStandardizedResidual > 3.0)
            addWarning(result, IssueCode::InconsistentBalance, QStringLiteral("reconciliation"),
                QStringLiteral("协调后存在超过 3σ 的测量残差，请检查异常样品"));
    }

    if (physicalSolutionInvalid) {
        // Values from one exact solution are coupled.  Once any derived value
        // is physically impossible, none of its sibling derived values are
        // trustworthy. Preserve only independently supplied measurements.
        result.values.clear();
        for (auto it = knownValues.cbegin(); it != knownValues.cend(); ++it)
            if (validValue(it.value())) result.values.insert(it.key(), it.value());
        result.complete = false;
        result.fullySolved = false;
        return result;
    }

    for (const auto& nodeId : graph.nodeIds()) {
        if (graph.nodeKind(nodeId) == NodeKind::Flotation) {
            const auto feedId = graph.streamsTo(nodeId, PortKind::Feed).front();
            const auto* node = graph.flotationNode(nodeId);
            const auto productPorts = node ? flotationProductPorts(*node) : QVector<PortKind>{};
            const auto productIds = graph.flotationProductStreams(nodeId);
            bool productsComplete = productIds.size() == productPorts.size();
            for (const auto& productId : productIds)
                productsComplete = productsComplete && result.values.contains(productId);
            if (result.values.contains(feedId) && productsComplete) {
                StreamValue outputSum;
                for (const auto& productId : productIds)
                    outputSum = add(outputSum, result.values[productId]);
                if (!approximatelyEqual(result.values[feedId], outputSum))
                    addIssue(result, IssueCode::InconsistentBalance, nodeId, QStringLiteral("浮选单元质量或组分不平衡"));
                if (!node || !node->leftSplitPercent) {
                    FlotationPerformance performance;
                    for (int index = 0; index < productIds.size(); ++index)
                        performance.setForPort(productPorts[index],
                            metrics(result.values[productIds[index]], result.values[feedId]));
                    result.flotationPerformance.insert(nodeId, performance);
                }
            }
        } else if (graph.nodeKind(nodeId) == NodeKind::Merge) {
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
        } else {
            const auto* pool = graph.storagePoolNode(nodeId);
            if (!pool || pool->terminal) continue;
            const auto inputs = graph.streamsTo(nodeId, PortKind::Feed);
            const auto outputs = graph.streamsFrom(nodeId, PortKind::PoolOutput);
            if (inputs.size() == 1 && outputs.size() == 1
                && result.values.contains(inputs.front()) && result.values.contains(outputs.front())
                && !approximatelyEqual(result.values[inputs.front()], result.values[outputs.front()])) {
                addIssue(result, IssueCode::InconsistentBalance, nodeId,
                         QStringLiteral("中间贮池入料与出料不平衡"));
            }
        }
    }

    for (const auto& streamId : graph.streamIds()) {
        if (!result.values.contains(streamId))
            addWarning(result, IssueCode::Underdetermined, streamId,
                       QStringLiteral("中间物料流未唯一确定"));
    }
    StreamValue totalExternalFeed;
    bool allExternalFeedsKnown = !externalIds.isEmpty();
    for (const auto& feedId : externalIds) {
        if (!result.values.contains(feedId)) {
            allExternalFeedsKnown = false;
            break;
        }
        totalExternalFeed = add(totalExternalFeed, result.values[feedId]);
    }
    if (allExternalFeedsKnown) {
        for (auto it = result.values.cbegin(); it != result.values.cend(); ++it)
            result.relativeToExternalFeed.insert(it.key(), metrics(it.value(), totalExternalFeed));
    }
    bool allTerminalValuesKnown = true;
    for (const auto& streamId : terminalIds)
        allTerminalValuesKnown = allTerminalValuesKnown && result.values.contains(streamId);
    const bool externalFeedKnown = allExternalFeedsKnown;
    result.fullySolved = !TopologyValidator::hasErrors(result.issues)
        && result.values.size() == graph.streamIds().size();
    result.complete = !TopologyValidator::hasErrors(result.issues)
        && allTerminalValuesKnown && externalFeedKnown;
    return result;
}

} // namespace afs::topology
