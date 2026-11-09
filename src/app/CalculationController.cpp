#include "app/CalculationController.h"
#include "document/FlowsheetDocument.h"
#include "services/FlowsheetCalculationService.h"
#include <QTimer>
#include <algorithm>
#include <utility>

namespace afs {
CalculationController::CalculationController(FlowsheetDocument& document, QObject* parent)
    : QObject(parent), m_document(document), m_refreshTimer(new QTimer(this)) {
    m_refreshTimer->setSingleShot(true);
    m_refreshTimer->setInterval(0);
    connect(m_refreshTimer, &QTimer::timeout, this, &CalculationController::publishState);
    connect(&document, &FlowsheetDocument::measurementChanged, this, &CalculationController::invalidatePreview);
    connect(&document, &FlowsheetDocument::componentsChanged, this, &CalculationController::invalidatePreview);
    connect(&document, &FlowsheetDocument::calculationModeChanged, this, &CalculationController::invalidatePreview);
    connect(&document, &FlowsheetDocument::currentScenarioChanged, this, &CalculationController::invalidatePreview);
    connect(&document, &FlowsheetDocument::calculationChanged, this, &CalculationController::invalidatePreview);
}

void CalculationController::setSnapshot(FlowsheetSnapshot snapshot) {
    m_snapshot = std::move(snapshot);
    invalidatePreview();
}

void CalculationController::invalidatePreview() {
    m_preview.reset();
    m_refreshTimer->start();
}

const topology::CalculationResult& CalculationController::displayResult() {
    if (const auto* stored = m_document.calculationResult()) return *stored;
    if (!m_preview) m_preview = calculate();
    return *m_preview;
}

topology::CalculationResult CalculationController::calculate() const {
    return FlowsheetCalculationService::calculate(m_snapshot, m_document);
}

void CalculationController::publishState() {
    CalculationPanelState state;
    const auto components = m_document.componentIds();
    state.canCalculate = std::any_of(m_snapshot.reportStreams.cbegin(), m_snapshot.reportStreams.cend(),
        [&](const StreamDescriptor& stream) { return m_document.measurement(stream.streamId).completeFor(components); });
    const auto* stored = m_document.calculationResult();
    const auto& result = displayResult();
    state.calculated = stored != nullptr;
    const int missing = std::max(result.dryMassDegreesOfFreedom, result.componentMassDegreesOfFreedom);
    if (!stored) {
        state.message = m_snapshot.reportStreams.isEmpty() ? "请先在画布中建立流程"
            : missing > 0 ? QString("当前方程组还有 %1 个独立自由度 · 可补充任意实测物流").arg(missing)
            : state.canCalculate ? "当前约束已足够，可以计算"
            : "可填写总入料或任意产品物流的质量和各组分品位";
    } else if (result.complete) {
        if (result.reconciled) state.message = QString("数据协调完成 · 最大标准化残差 %1σ%2")
            .arg(result.maximumAbsoluteStandardizedResidual, 0, 'f', 2)
            .arg(result.maximumAbsoluteStandardizedResidual > 3.0 ? " · 存在可疑测量" : "");
        else state.message = result.fullySolved ? "平衡计算成功"
                                               : "全流程平衡已完成 · 部分中间物流未唯一求解";
    } else {
        const bool conflict = std::any_of(result.issues.cbegin(), result.issues.cend(), [](const auto& issue) {
            return issue.code == topology::IssueCode::InconsistentBalance
                || issue.code == topology::IssueCode::InvalidMeasurement;
        });
        state.message = conflict ? "计算未完成 · 输入数据或支路占比相互矛盾"
            : missing > 0 ? QString("计算未完成 · 方程组仍有 %1 个独立自由度，请补充实测物流").arg(missing)
            : QString("计算未完成 · %1 个流程问题").arg(result.issues.size());
    }
    emit stateChanged(state);
}
} // namespace afs
