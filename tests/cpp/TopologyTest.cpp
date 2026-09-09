#include "topology/OpenCircuitCalculator.h"
#include "topology/LinearSystemSolver.h"
#include "topology/TopologyAlgorithms.h"
#include "topology/TopologyValidator.h"

#include <QCoreApplication>
#include <cmath>

using namespace afs::topology;

namespace {
bool close(double a, double b) { return std::abs(a - b) < 1e-8; }

MaterialStream externalFeed(const QString& id, const QString& target) {
    return {id, std::nullopt, PortRef{target, PortKind::Feed}};
}

MaterialStream terminal(const QString& id, const QString& source, PortKind port) {
    return {id, PortRef{source, port}, std::nullopt};
}

MaterialStream connection(const QString& id, const QString& source, PortKind sourcePort,
                          const QString& target) {
    return {id, PortRef{source, sourcePort}, PortRef{target, PortKind::Feed}};
}
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    TopologyGraph graph;
    if (!graph.addFlotationNode({"U1"}) || !graph.addFlotationNode({"U2"})
        || !graph.addFlotationNode({"U3"})) return 1;
    if (!graph.addStream(externalFeed("F0", "U1"))) return 2;
    if (!graph.addStream(connection("S12", "U1", PortKind::LeftProduct, "U2"))) return 3;
    if (!graph.addStream(terminal("P1", "U1", PortKind::RightProduct))) return 4;
    if (!graph.addStream(connection("S23", "U2", PortKind::LeftProduct, "U3"))) return 5;
    if (!graph.addStream(terminal("P2", "U2", PortKind::RightProduct))) return 6;
    if (!graph.addStream(terminal("P3", "U3", PortKind::LeftProduct))) return 7;
    if (!graph.addStream(terminal("P4", "U3", PortKind::RightProduct))) return 8;

    const auto issues = TopologyValidator::validate(graph);
    if (TopologyValidator::hasErrors(issues)) return 9;
    if (graph.nodeIds().size() != 3 || graph.streamIds().size() != 7) return 10;
    if (graph.terminalProductStreams().size() != 4 || graph.externalFeedStreams().size() != 1) return 11;
    const auto order = TopologyAlgorithms::sort(graph);
    if (order.hasCycle || order.forward != QVector<NodeId>{"U1", "U2", "U3"}) return 12;
    if (order.reverse != QVector<NodeId>{"U3", "U2", "U1"}) return 13;

    QHash<StreamId, StreamValue> products;
    products.insert("P1", *StreamValue::fromMassAndGrade(10, 1));
    products.insert("P2", *StreamValue::fromMassAndGrade(20, 2));
    products.insert("P3", *StreamValue::fromMassAndGrade(30, 3));
    products.insert("P4", *StreamValue::fromMassAndGrade(40, 4));
    const auto result = OpenCircuitCalculator::calculate(graph, products);
    if (!result.complete || result.values.size() != 7) return 14;
    if (!close(result.values["S23"].dryMass, 70)
        || !close(result.values["S23"].gradePercent(), 250.0 / 70.0)) return 15;
    if (!close(result.values["S12"].dryMass, 90)
        || !close(result.values["S12"].gradePercent(), 290.0 / 90.0)) return 16;
    if (!close(result.values["F0"].dryMass, 100)
        || !close(result.values["F0"].gradePercent(), 3.0)) return 17;
    if (!close(result.flotationPerformance["U3"].left.massYieldPercent, 30.0 / 70.0 * 100.0)
        || !close(result.flotationPerformance["U3"].left.recoveryPercent, 36.0)) return 18;
    if (!close(result.relativeToExternalFeed["P4"].massYieldPercent, 40.0)
        || !close(result.relativeToExternalFeed["P4"].recoveryPercent, 160.0 / 3.0)) return 19;

    TopologyGraph cyclic;
    cyclic.addFlotationNode({"A"});
    cyclic.addFlotationNode({"B"});
    cyclic.addStream(connection("AB", "A", PortKind::LeftProduct, "B"));
    cyclic.addStream(connection("BA", "B", PortKind::LeftProduct, "A"));
    cyclic.addStream(terminal("AR", "A", PortKind::RightProduct));
    cyclic.addStream(terminal("BR", "B", PortKind::RightProduct));
    const auto cyclicIssues = TopologyValidator::validate(cyclic);
    bool foundCycle = false;
    for (const auto& issue : cyclicIssues) if (issue.code == IssueCode::Cycle) foundCycle = true;
    if (!foundCycle) return 20;

    TopologyGraph inconsistent;
    inconsistent.addFlotationNode({"I1"});
    inconsistent.addStream(externalFeed("IF", "I1"));
    inconsistent.addStream(terminal("IL", "I1", PortKind::LeftProduct));
    inconsistent.addStream(terminal("IR", "I1", PortKind::RightProduct));
    QHash<StreamId, StreamValue> impossible;
    impossible.insert("IF", *StreamValue::fromMassAndGrade(5, 2));
    impossible.insert("IL", *StreamValue::fromMassAndGrade(10, 2));
    const auto inconsistentResult = OpenCircuitCalculator::calculate(inconsistent, impossible);
    bool foundInconsistent = false;
    for (const auto& issue : inconsistentResult.issues)
        if (issue.code == IssueCode::InconsistentBalance && issue.objectId == "I1")
            foundInconsistent = true;
    if (!foundInconsistent || inconsistentResult.complete) return 21;

    TopologyGraph multipleFeeds;
    multipleFeeds.addFlotationNode({"M1"});
    multipleFeeds.addFlotationNode({"M2"});
    multipleFeeds.addStream(externalFeed("MF1", "M1"));
    multipleFeeds.addStream(externalFeed("MF2", "M2"));
    multipleFeeds.addStream(terminal("M1L", "M1", PortKind::LeftProduct));
    multipleFeeds.addStream(terminal("M1R", "M1", PortKind::RightProduct));
    multipleFeeds.addStream(terminal("M2L", "M2", PortKind::LeftProduct));
    multipleFeeds.addStream(terminal("M2R", "M2", PortKind::RightProduct));
    const auto multipleFeedIssues = TopologyValidator::validate(multipleFeeds);
    bool foundMultipleFeeds = false;
    for (const auto& issue : multipleFeedIssues)
        if (issue.code == IssueCode::MultipleExternalFeeds) foundMultipleFeeds = true;
    if (!foundMultipleFeeds || !TopologyValidator::hasErrors(multipleFeedIssues)) return 22;

    TopologyGraph aggregate;
    aggregate.addFlotationNode({"A1"});
    aggregate.addFlotationNode({"A2"});
    aggregate.addFlotationNode({"A3"});
    aggregate.addMergeNode({"AM"});
    aggregate.addStream(externalFeed("AF", "A1"));
    aggregate.addStream({"AL1", PortRef{"A1", PortKind::LeftProduct},
                         PortRef{"AM", PortKind::MergeInput}});
    aggregate.addStream(connection("AS12", "A1", PortKind::RightProduct, "A2"));
    aggregate.addStream({"AL2", PortRef{"A2", PortKind::LeftProduct},
                         PortRef{"AM", PortKind::MergeInput}});
    aggregate.addStream(connection("AS23", "A2", PortKind::RightProduct, "A3"));
    aggregate.addStream({"AL3", PortRef{"A3", PortKind::LeftProduct},
                         PortRef{"AM", PortKind::MergeInput}});
    aggregate.addStream(terminal("AT", "A3", PortKind::RightProduct));
    aggregate.addStream({"AC", PortRef{"AM", PortKind::MergeOutput}, std::nullopt});
    QHash<StreamId, StreamValue> aggregateProducts;
    aggregateProducts.insert("AC", *StreamValue::fromMassAndGrade(60, 10));
    aggregateProducts.insert("AT", *StreamValue::fromMassAndGrade(40, 1));
    const auto partial = OpenCircuitCalculator::calculate(aggregate, aggregateProducts);
    if (!partial.complete || partial.fullySolved || !partial.values.contains("AF")
        || !close(partial.values["AF"].dryMass, 100)
        || !close(partial.values["AF"].gradePercent(), 6.4)) return 23;

    QHash<StreamId, BranchAllocation> allocations;
    allocations.insert("AL1", {20, 30});
    allocations.insert("AL2", {30, 30});
    allocations.insert("AL3", {50, 40});
    const auto allocated = OpenCircuitCalculator::calculate(
        aggregate, aggregateProducts, allocations);
    if (!allocated.complete || !allocated.fullySolved
        || !close(allocated.values["AL1"].dryMass, 12)
        || !close(allocated.values["AL3"].componentMass, 2.4)) return 24;

    // This closed loop has no node with enough locally-known streams to start
    // iterative propagation. The allocation equation and all balances must be
    // solved simultaneously.
    TopologyGraph simultaneous;
    simultaneous.addMergeNode({"SM"});
    simultaneous.addFlotationNode({"SA"});
    simultaneous.addFlotationNode({"SB"});
    simultaneous.addStream({"SF", std::nullopt, PortRef{"SM", PortKind::MergeInput}});
    simultaneous.addStream({"SBA", PortRef{"SB", PortKind::LeftProduct},
                            PortRef{"SM", PortKind::MergeInput}});
    simultaneous.addStream({"SMF", PortRef{"SM", PortKind::MergeOutput},
                            PortRef{"SA", PortKind::Feed}});
    simultaneous.addStream(terminal("SAT", "SA", PortKind::LeftProduct));
    simultaneous.addStream(connection("SAB", "SA", PortKind::RightProduct, "SB"));
    simultaneous.addStream(terminal("SBT", "SB", PortKind::RightProduct));
    QHash<StreamId, StreamValue> simultaneousKnown;
    simultaneousKnown.insert("SF", *StreamValue::fromMassAndGrade(80, 2));
    simultaneousKnown.insert("SAT", *StreamValue::fromMassAndGrade(30, 3));
    QHash<StreamId, BranchAllocation> simultaneousAllocations;
    simultaneousAllocations.insert("SF", {80, 80});
    simultaneousAllocations.insert("SBA", {20, 20});
    const auto simultaneousResult = OpenCircuitCalculator::calculate(
        simultaneous, simultaneousKnown, simultaneousAllocations);
    if (!simultaneousResult.complete || !simultaneousResult.fullySolved
        || !close(simultaneousResult.values["SMF"].dryMass, 100)
        || !close(simultaneousResult.values["SBA"].dryMass, 20)
        || !close(simultaneousResult.values["SAB"].dryMass, 70)
        || !close(simultaneousResult.values["SBT"].dryMass, 50)
        || !close(simultaneousResult.values["SBT"].gradePercent(), 1.4)) return 25;

    TopologyGraph splitter;
    splitter.addFlotationNode({"SP", ProductRole::Unknown, ProductRole::Unknown, 35.0});
    splitter.addStream(externalFeed("SPF", "SP"));
    splitter.addStream(terminal("SPL", "SP", PortKind::LeftProduct));
    splitter.addStream(terminal("SPR", "SP", PortKind::RightProduct));
    QHash<StreamId, StreamValue> splitterKnown;
    splitterKnown.insert("SPL", *StreamValue::fromMassAndGrade(35, 4.2));
    const auto splitterResult = OpenCircuitCalculator::calculate(splitter, splitterKnown);
    if (!splitterResult.complete || !splitterResult.fullySolved
        || !close(splitterResult.values["SPF"].dryMass, 100)
        || !close(splitterResult.values["SPR"].dryMass, 65)
        || !close(splitterResult.values["SPF"].gradePercent(), 4.2)
        || splitterResult.flotationPerformance.contains("SP")) return 26;

    TopologyGraph threeProduct;
    threeProduct.addFlotationNode({"TP", ProductRole::Unknown, ProductRole::Unknown,
                                   std::nullopt, true});
    threeProduct.addStream(externalFeed("TPF", "TP"));
    threeProduct.addStream(terminal("TPL", "TP", PortKind::LeftProduct));
    threeProduct.addStream(terminal("TPM", "TP", PortKind::MiddleProduct));
    threeProduct.addStream(terminal("TPR", "TP", PortKind::RightProduct));
    QHash<StreamId, StreamValue> threeKnown;
    threeKnown.insert("TPL", *StreamValue::fromMassAndGrade(10, 8));
    threeKnown.insert("TPM", *StreamValue::fromMassAndGrade(20, 3));
    threeKnown.insert("TPR", *StreamValue::fromMassAndGrade(70, 1));
    const auto threeResult = OpenCircuitCalculator::calculate(threeProduct, threeKnown);
    if (!threeResult.complete || !threeResult.fullySolved
        || !close(threeResult.values["TPF"].dryMass, 100)
        || !close(threeResult.values["TPF"].gradePercent(), 2.1)
        || !close(threeResult.flotationPerformance["TP"].middle.massYieldPercent, 20)
        || !close(threeResult.flotationPerformance["TP"].middle.recoveryPercent,
                  0.6 / 2.1 * 100.0)) return 27;

    // Global boundary closure remains an independent equation, so a measured
    // feed that disagrees with the terminal sum must be rejected.
    LinearBalanceConstraint boundary;
    boundary.id = "global-boundary";
    boundary.coefficients.insert("TPF", 1.0);
    boundary.coefficients.insert("TPL", -1.0);
    boundary.coefficients.insert("TPM", -1.0);
    boundary.coefficients.insert("TPR", -1.0);
    QHash<StreamId, StreamValue> contradictory = threeKnown;
    contradictory.insert("TPF", *StreamValue::fromMassAndGrade(110, 2.1));
    const auto boundaryConflict = OpenCircuitCalculator::calculate(
        threeProduct, contradictory, {}, false, {boundary});
    if (boundaryConflict.complete || !boundaryConflict.values.isEmpty()
        || !TopologyValidator::hasErrors(boundaryConflict.issues)) return 28;

    // If an exact algebraic solution contains a negative derived stream, all
    // sibling derived values are unsafe; only independent measurements remain.
    TopologyGraph impossiblePhysical;
    impossiblePhysical.addFlotationNode({"IP"});
    impossiblePhysical.addStream(externalFeed("IPF", "IP"));
    impossiblePhysical.addStream(terminal("IPL", "IP", PortKind::LeftProduct));
    impossiblePhysical.addStream(terminal("IPR", "IP", PortKind::RightProduct));
    QHash<StreamId, StreamValue> impossiblePhysicalKnown;
    impossiblePhysicalKnown.insert("IPF", *StreamValue::fromMassAndGrade(5, 2));
    impossiblePhysicalKnown.insert("IPL", *StreamValue::fromMassAndGrade(10, 2));
    const auto impossiblePhysicalResult = OpenCircuitCalculator::calculate(
        impossiblePhysical, impossiblePhysicalKnown);
    if (impossiblePhysicalResult.complete
        || impossiblePhysicalResult.values.size() != 2
        || !impossiblePhysicalResult.values.contains("IPF")
        || !impossiblePhysicalResult.values.contains("IPL")
        || impossiblePhysicalResult.values.contains("IPR")) return 29;

    // The generic solver independently guarantees partial-unique and
    // inconsistent-system semantics used by every topology calculation.
    const auto partialLinear = LinearSystemSolver::solve({
        {1.0, 0.0, 0.0, 1.0},
        {0.0, 1.0, 1.0, 2.0}}, 3);
    if (partialLinear.inconsistent || partialLinear.degreesOfFreedom != 1
        || !partialLinear.values[0] || !close(*partialLinear.values[0], 1)
        || partialLinear.values[1] || partialLinear.values[2]) return 30;
    const auto inconsistentLinear = LinearSystemSolver::solve({
        {1.0, 1.0}, {1.0, 2.0}}, 1);
    if (!inconsistentLinear.inconsistent) return 31;

    QHash<StreamId, StreamValue> noisy;
    noisy.insert("IF", *StreamValue::fromMassAndGrade(100, 2));
    noisy.insert("IL", *StreamValue::fromMassAndGrade(60, 2));
    noisy.insert("IR", *StreamValue::fromMassAndGrade(50, 2));
    QHash<StreamId, StreamUncertainty> uncertainty;
    uncertainty.insert("IF", {2, 0.1});
    uncertainty.insert("IL", {2, 0.1});
    uncertainty.insert("IR", {2, 0.1});
    const auto reconciled = OpenCircuitCalculator::calculate(
        inconsistent, noisy, {}, false, {}, uncertainty);
    if (!reconciled.reconciled || !reconciled.complete
        || !close(reconciled.values["IF"].dryMass,
                  reconciled.values["IL"].dryMass + reconciled.values["IR"].dryMass)
        || reconciled.residuals.size() != 3
        || reconciled.maximumAbsoluteStandardizedResidual <= 0.0) return 32;
    LinearBalanceConstraint duplicateBoundary;
    duplicateBoundary.id = "duplicate-boundary";
    duplicateBoundary.coefficients.insert("IF", 1.0);
    duplicateBoundary.coefficients.insert("IL", -1.0);
    duplicateBoundary.coefficients.insert("IR", -1.0);
    const auto reconciledWithBoundary = OpenCircuitCalculator::calculate(
        inconsistent, noisy, {}, false, {duplicateBoundary}, uncertainty);
    if (!reconciledWithBoundary.complete
        || reconciledWithBoundary.values.size() != 3) return 33;
    return 0;
}
