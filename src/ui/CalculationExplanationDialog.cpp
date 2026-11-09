#include "ui/CalculationExplanationDialog.h"
#include "document/ExperimentScenario.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <algorithm>

namespace afs {

void showCalculationExplanationDialog(QWidget* parent,
    const topology::CalculationResult& result, CalculationMode mode) {
    const int maximumDegreesOfFreedom = std::max(result.dryMassDegreesOfFreedom,
                                                  result.componentMassDegreesOfFreedom);
    QString state;
    if (result.complete) {
        state = result.fullySolved ? "已完成：全部物流已唯一确定"
            : "部分完成：已保留可唯一确定的物流，其余内部物流仍欠定";
    } else if (maximumDegreesOfFreedom > 0) {
        state = QString("尚欠定：至少还需要 %1 个独立约束").arg(maximumDegreesOfFreedom);
    } else {
        state = "未完成：请检查下方列出的拓扑或实测数据问题";
    }
    const QString balanceDescription = mode == CalculationMode::DataReconciliation
        ? "在物料守恒约束下，以标准差为权重最小化实测值修正；结果显示为协调值。"
        : "实测值作为严格约束，与各单元、汇流和贮池的质量守恒方程联立求解。";
    QString issues;
    for (const auto& issue : result.issues) {
        if (!issues.isEmpty()) issues.append('\n');
        issues.append(issue.message);
    }
    if (issues.isEmpty()) issues = "无";

    const QStringList labels{
        "计算模式", "求解变量", "守恒与观测约束", "当前状态", "自由度", "诊断"};
    const QStringList values{
        mode == CalculationMode::DataReconciliation ? "数据协调" : "严格模式",
        "每条物流分别以绝对干质量和各组分质量为变量；品位由组分质量 / 干质量换算。",
        balanceDescription,
        state,
        QString("干质量 %1；组分质量 %2")
            .arg(result.dryMassDegreesOfFreedom)
            .arg(result.componentMassDegreesOfFreedom),
        issues};
    QStringList sections;
    for (int row = 0; row < labels.size(); ++row)
        sections.append(labels[row] + "：\n" + values[row]);
    QDialog dialog(parent);
    dialog.setObjectName("calculationExplanationDialog");
    dialog.setWindowTitle("计算说明");
    dialog.resize(640, 480);
    auto* layout = new QVBoxLayout(&dialog);
    auto* text = new QTextBrowser(&dialog);
    text->setObjectName("calculationExplanationText");
    text->setPlainText(sections.join("\n\n"));
    layout->addWidget(text);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.exec();
}

} // namespace afs
