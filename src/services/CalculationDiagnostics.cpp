#include "services/CalculationDiagnostics.h"
#include "adapters/CanvasTopologyBuilder.h"
#include "document/FlowsheetDocument.h"
#include <QSet>
#include <cmath>
namespace afs {
QVector<CalculationDiagnostic> CalculationDiagnostics::build(const CanvasTopologySnapshot& snapshot,
    const FlowsheetDocument& document, const topology::CalculationResult& result) {
    QVector<CalculationDiagnostic> entries; QSet<QString> seen;
    for (const auto& issue : result.issues) {
        CalculationDiagnostic entry; entry.objectId = issue.objectId; entry.message = issue.message;
        entry.error = issue.severity == topology::IssueSeverity::Error;
        if (!document.components().isEmpty()) entry.componentId = document.components().front().id;
        for (const auto& component : document.components())
            if (issue.message.startsWith(component.name + "：")) entry.componentId = component.id;
        for (const auto& component : document.components())
            if (component.id == entry.componentId && !entry.message.startsWith(component.name + "："))
                entry.message.prepend(component.name + "：");
        for (const auto& stream : snapshot.reportStreams) if (stream.streamId == issue.objectId) {
            entry.streamId = stream.streamId; entry.title = document.productName(stream.streamId).trimmed();
            if (entry.title.isEmpty()) entry.title = stream.displayName;
            break;
        }
        if (entry.streamId.isEmpty()) {
            for (const auto& stream : snapshot.reportStreams) if (stream.ownerIds.contains(issue.objectId)) {
                entry.streamId = stream.streamId;
                if (stream.feed) break;
            }
            if (const auto* merge = snapshot.graph.mergeNode(issue.objectId))
                entry.title = QString("%1 · %2")
                    .arg(merge->role == topology::MergeRole::ProductMerge ? "产品汇流" : "入料汇流", issue.objectId);
            else if (snapshot.graph.containsNode(issue.objectId))
                entry.title = QString("流程单元 · %1").arg(issue.objectId);
        }
        if (entry.title.isEmpty()) entry.title = "全局约束";
        if (!entry.streamId.isEmpty()) {
            const auto measurement = document.measurement(entry.streamId);
            entry.gradeField = measurement.dryMass.has_value() && !measurement.grade(entry.componentId).has_value();
            if (issue.code == topology::IssueCode::Underdetermined) {
                entry.message += entry.gradeField
                    ? "；该物流已填质量，可补充品位作为约束。"
                    : "；可选择补充该物流的质量和品位作为约束，不必逐条填满。";
            }
        }
        const auto key = entry.objectId + '/' + entry.componentId + '/' + entry.message;
        if (!seen.contains(key)) { entries.append(entry); seen.insert(key); }
    }
    // A global conflict does not identify a unique offending observation.
    // Offer the participating measurements as review links without blaming one.
    bool globalConflict = false;
    for (const auto& issue : result.issues)
        globalConflict |= issue.code == topology::IssueCode::InconsistentBalance && issue.objectId == "equation-system";
    if (globalConflict) for (const auto& stream : snapshot.reportStreams) {
        const auto measurement = document.measurement(stream.streamId);
        if (!measurement.dryMass && !measurement.dryMassSharePercent && measurement.components.isEmpty()) continue;
        entries.append({stream.streamId, stream.streamId, {}, stream.displayName,
            "检查这条已录入物流的测量值及支路占比；全局矛盾可能来自多条约束。", false, false});
    }
    for (const auto& component : document.components()) {
        const auto solved = result.components.constFind(component.id);
        if (solved == result.components.cend()) continue;
        for (const auto& stream : snapshot.reportStreams) {
            const auto residual = solved->residuals.constFind(stream.streamId);
            if (residual == solved->residuals.cend()) continue;
            const double mass = std::abs(residual->dryMassStandardized), content = std::abs(residual->componentMassStandardized);
            if (mass <= 3 && content <= 3) continue;
            auto title = document.productName(stream.streamId).trimmed();
            if (title.isEmpty()) title = stream.displayName;
            entries.append({stream.streamId, stream.streamId, component.id, title,
                QString("%1：干质量残差 %2σ，组分质量残差 %3σ；请检查实测值与测量标准差。")
                    .arg(component.name).arg(mass, 0, 'f', 2).arg(content, 0, 'f', 2), false, content > mass});
        }
    }
    return entries;
}
}
