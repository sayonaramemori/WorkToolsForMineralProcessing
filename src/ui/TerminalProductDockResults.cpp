#include "ui/TerminalProductDock.h"
#include "ui/TerminalProductTableModel.h"
#include "ui/TerminalProductViewSupport.h"
#include <QLabel>
#include <QPushButton>

namespace afs {

void TerminalProductDock::setScenarioName(const QString& name) {
    setWindowTitle(QString("物流实测参数 · %1").arg(name));
}

void TerminalProductDock::updateSummary() {
    const int complete = m_model->completedCount();
    m_progressLabel->setText(m_model->rowCount() == 0
        ? "暂无物流" : QString("显示 %1/%2 · 已录入 %3")
              .arg(m_filterModel->rowCount()).arg(m_model->rowCount()).arg(complete));
}

void TerminalProductDock::setCalculationState(const CalculationPanelState& state) {
    m_calculateButton->setEnabled(state.canCalculate);
    m_calculateButton->setText(state.calculated ? "重新计算" : "计算");
    m_calculationStatus->setText(state.message);
}

} // namespace afs
