#pragma once

#include <QHash>
#include <QString>
#include <QVector>
#include <optional>

namespace afs::topology {

using NodeId = QString;
using StreamId = QString;

enum class NodeKind { Flotation, Merge };
enum class ProductRole { Concentrate, Tailing, Middling, Unknown };
enum class PortKind { Feed, LeftProduct, MiddleProduct, RightProduct, MergeInput, MergeOutput };

struct PortRef {
    NodeId nodeId;
    PortKind port{PortKind::Feed};
    friend bool operator==(const PortRef&, const PortRef&) = default;
};

struct FlotationNode {
    NodeId id;
    ProductRole leftRole{ProductRole::Unknown};
    ProductRole rightRole{ProductRole::Unknown};
    std::optional<double> leftSplitPercent;
    bool hasMiddleProduct{false};
};

[[nodiscard]] QVector<PortKind> flotationProductPorts(const FlotationNode& node);

struct MergeNode {
    NodeId id;
};

struct MaterialStream {
    StreamId id;
    std::optional<PortRef> source;
    std::optional<PortRef> target;
};

enum class IssueSeverity { Warning, Error };
enum class IssueCode {
    DuplicateId,
    MissingNode,
    InvalidPort,
    EmptyStream,
    MissingFeed,
    MissingProduct,
    InvalidMerge,
    NoExternalFeed,
    MultipleExternalFeeds,
    NoTerminalProduct,
    Cycle,
    DisconnectedNode,
    InvalidMeasurement,
    MissingMeasurement,
    InconsistentBalance,
    Underdetermined
};

struct TopologyIssue {
    IssueSeverity severity{IssueSeverity::Error};
    IssueCode code{IssueCode::InvalidPort};
    QString objectId;
    QString message;
};

struct StreamValue {
    double dryMass{0.0};
    double componentMass{0.0};

    static std::optional<StreamValue> fromMassAndGrade(double dryMass, double gradePercent);
    [[nodiscard]] double gradePercent() const;
};

struct TopologyOrder {
    QVector<NodeId> forward;
    QVector<NodeId> reverse;
    bool hasCycle{false};
};

struct ProductMetrics {
    double massYieldPercent{0.0};
    double recoveryPercent{0.0};
};

struct FlotationPerformance {
    ProductMetrics left;
    ProductMetrics right;
    ProductMetrics middle;

    [[nodiscard]] ProductMetrics forPort(PortKind port) const;
    void setForPort(PortKind port, ProductMetrics metrics);
};

struct ComponentCalculationResult {
    QHash<StreamId, StreamValue> values;
    QHash<NodeId, FlotationPerformance> flotationPerformance;
    QHash<StreamId, ProductMetrics> relativeToExternalFeed;
    bool complete{false};
    bool fullySolved{false};
};

struct CalculationResult {
    QHash<StreamId, StreamValue> values;
    QHash<NodeId, FlotationPerformance> flotationPerformance;
    QHash<StreamId, ProductMetrics> relativeToExternalFeed;
    QHash<QString, ComponentCalculationResult> components;
    QVector<TopologyIssue> issues;
    int dryMassDegreesOfFreedom{0};
    int componentMassDegreesOfFreedom{0};
    // complete means the overall balance is available; fullySolved additionally
    // means every internal stream has a unique value.
    bool complete{false};
    bool fullySolved{false};
};

} // namespace afs::topology
