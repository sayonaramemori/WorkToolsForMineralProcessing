#include "ui/TerminalProductDock.h"
#include "document/FlowsheetDocument.h"
#include "services/FlowsheetCalculationService.h"
#include "ui/ResultDetailsView.h"
#include "ui/TerminalProductTableModel.h"
#include "ui/TerminalProductViewSupport.h"
#include <QLabel>
#include <QPushButton>
#include <algorithm>

namespace afs {

void TerminalProductDock::showStreamResult(const QString& streamId) {
    m_resultDetails->showStream(streamId);
}

void TerminalProductDock::showUnitResult(const QString& unitId, const topology::TopologyGraph& graph) {
    m_resultDetails->showUnit(unitId, graph);
}

void TerminalProductDock::clearResultDetails() { m_resultDetails->showPlaceholder(); }

void TerminalProductDock::setScenarioName(const QString& name) {
    setWindowTitle(QString("物流实测参数 · %1").arg(name));
}

void TerminalProductDock::updateSummary() {
    const int complete = m_model->completedCount();
    m_progressLabel->setText(m_model->rowCount() == 0
        ? "暂无物流" : QString("显示 %1/%2 · 已录入 %3")
              .arg(m_filterModel->rowCount()).arg(m_model->rowCount()).arg(complete));
}

void TerminalProductDock::updateCalculationState() {
    const bool hasCompleteInput = m_model->completedCount() > 0;
    m_calculateButton->setEnabled(hasCompleteInput);
    const auto* result = m_document.calculationResult();
    if (result && result->complete) {
        if (result->reconciled) {
            m_calculationStatus->setText(QString("数据协调完成 · 最大标准化残差 %1σ%2")
                .arg(result->maximumAbsoluteStandardizedResidual, 0, 'f', 2)
                .arg(result->maximumAbsoluteStandardizedResidual > 3.0
                    ? " · 存在可疑测量" : ""));
        } else m_calculationStatus->setText(result->fullySolved
            ? "平衡计算成功 · 选择画布对象查看详细结果"
            : "全流程平衡已完成 · 部分中间物流未唯一求解");
        m_calculateButton->setText("重新计算");
    } else if (result) {
        const int missing = std::max(result->dryMassDegreesOfFreedom,
                                     result->componentMassDegreesOfFreedom);
        const bool conflict = std::any_of(result->issues.cbegin(), result->issues.cend(),
            [](const auto& issue) {
                return issue.code == topology::IssueCode::InconsistentBalance
                    || issue.code == topology::IssueCode::InvalidMeasurement;
            });
        m_calculationStatus->setText(conflict
            ? QString("计算未完成 · 输入数据或支路占比相互矛盾")
            : missing > 0
                ? QString("计算未完成 · 方程组仍有 %1 个独立自由度，请补充实测物流")
                      .arg(missing)
                : QString("计算未完成 · %1 个流程问题").arg(result->issues.size()));
        m_calculateButton->setText("重新计算");
    } else {
        const auto preview = FlowsheetCalculationService::calculate(
            m_snapshot, m_document);
        const int missing = std::max(preview.dryMassDegreesOfFreedom,
                                     preview.componentMassDegreesOfFreedom);
        m_calculationStatus->setText(m_model->rowCount() == 0
            ? "请先在画布中建立流程"
            : missing > 0
                ? QString("当前方程组还有 %1 个独立自由度 · 可补充任意实测物流")
                      .arg(missing)
                : hasCompleteInput
                    ? "当前约束已足够，可以计算"
                    : "可填写总入料或任意产品物流的质量和各组分品位");
        m_calculateButton->setText("计算");
    }
}

void TerminalProductDock::showCalculationExplanation() {
    // Once an input changes, its stored result is invalidated.  Use a preview
    // in that case so the explanation always describes the values on screen.
    const auto* stored = m_document.calculationResult();
    const auto preview = stored ? topology::CalculationResult{}
                                : FlowsheetCalculationService::calculate(m_snapshot, m_document);
    const auto& result = stored ? *stored : preview;
    m_resultDetails->showCalculationExplanation(result, m_document.calculationMode());
}

} // namespace afs
